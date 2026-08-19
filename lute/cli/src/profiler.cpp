#include "lute/profiler.h"

#include "lute/reporter.h"

#include "lua.h"

#include <algorithm>
#include <atomic>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>


struct FrameInfo
{
    std::string name;
    std::string file;
    int line;

    FrameInfo(const lua_Debug& dbg)
        : name(dbg.name ? dbg.name : "(anonymous)")
        , file(dbg.source ? &dbg.source[1] : "(unknown)") // TODO(Varun)replace with chunkname utils API
        , line(dbg.linedefined)
    {
    }

    bool operator==(const FrameInfo& other) const
    {
        return name == other.name && file == other.file && line == other.line;
    }
};

struct TraceEvent
{
    TraceEvent(char phase, const FrameInfo& fi, uint64_t timestamp)
        : phase(phase)
        , name(fi.name)
        , file(fi.file)
        , line(fi.line)
        , timestamp(timestamp)
    {
    }
    char phase; // 'B' for begin, 'E' for end
    std::string name;
    std::string file;
    int line;
    uint64_t timestamp; // Timestamp in microseconds
};

struct Profiler
{
    // static state
    lua_Callbacks* callbacks = nullptr;
    int frequency = ProfileOptions{}.frequency;
    std::thread thread;

    // variables for communication between loop and trigger
    std::atomic<bool> exit = false;
    std::atomic<uint64_t> pendingTicks = 0;

    // state for tracking stack changes (only accessed in trigger)
    std::vector<FrameInfo> previousStack;
    std::vector<TraceEvent> events;
    uint64_t currentTimestamp = 0;

    // A second channel: ticks per "file:line" of the executing leaf frame. It is kept out of FrameInfo on purpose.
    // FrameInfo::operator== drives the B/E stack diff below, so a frame carrying the current line would compare
    // unequal to itself the moment the line advanced, closing and reopening every frame on the stack once per line
    // and turning a trace of a few hundred frames into hundreds of megabytes.
    std::unordered_map<std::string, uint64_t> leafLines;

} gProfiler;

static void profilerTrigger(lua_State* L, int gc)
{
    uint64_t ticks = gProfiler.pendingTicks.exchange(0);
    if (ticks == 0)
    {
        gProfiler.callbacks->interrupt = nullptr;
        return;
    }

    std::vector<FrameInfo> currentStack;
    lua_Debug dbg;
    for (int level = 0; lua_getinfo(L, level, "sln", &dbg); ++level)
    {
        // Level 0 is the leaf, and its currentline is the only line in the stack that is executing rather than
        // merely waiting on a call below it. Weighted by ticks so the histogram and the trace measure the same thing.
        if (level == 0 && dbg.currentline > 0)
        {
            std::string where = std::string(dbg.source ? &dbg.source[1] : "(unknown)");
            where += ":";
            where += std::to_string(dbg.currentline);
            gProfiler.leafLines[where] += ticks;
        }
        currentStack.emplace_back(dbg);
    }

    // Stacks are in reverse order, i.e
    // leaf ..... main, so we should walk backwards
    size_t commonDepth = 0;
    size_t iterPrev = gProfiler.previousStack.size();
    size_t iterCurr = currentStack.size();

    while (iterPrev > 0 && iterCurr > 0 && gProfiler.previousStack[iterPrev - 1] == currentStack[iterCurr - 1])
    {
        commonDepth++;
        iterPrev--;
        iterCurr--;
    }

    for (size_t i = 0; i < gProfiler.previousStack.size() - commonDepth; i++)
    {
        const FrameInfo& frame = gProfiler.previousStack.at(i);
        gProfiler.events.emplace_back('E', frame, gProfiler.currentTimestamp);
    }

    for (size_t i = currentStack.size() - commonDepth; i > 0; i--)
    {
        const FrameInfo& frame = currentStack.at(i - 1);
        gProfiler.events.emplace_back('B', frame, gProfiler.currentTimestamp);
    }

    gProfiler.currentTimestamp += ticks;
    gProfiler.previousStack = std::move(currentStack);
    gProfiler.callbacks->interrupt = nullptr;
}

static void profilerLoop()
{
    double last = lua_clock();

    while (!gProfiler.exit)
    {
        double now = lua_clock();

        if (now - last >= 1.0 / double(gProfiler.frequency))
        {
            uint64_t ticks = uint64_t((now - last) * 1e6);

            gProfiler.pendingTicks += ticks;
            gProfiler.callbacks->interrupt = profilerTrigger;

            last += ticks * 1e-6;
        }
        else
        {
            std::this_thread::yield();
        }
    }
}

void profilerStart(lua_State* L, int frequency)
{
    gProfiler.frequency = frequency;
    gProfiler.callbacks = lua_callbacks(L);
    gProfiler.previousStack.clear();
    gProfiler.events.clear();
    gProfiler.leafLines.clear();
    gProfiler.currentTimestamp = 0;
    gProfiler.pendingTicks = 0;

    gProfiler.exit = false;
    gProfiler.thread = std::thread(profilerLoop);
}

void profilerStop()
{
    gProfiler.exit = true;
    gProfiler.thread.join();

    // For any of the remaining frames on the stack, insert an artificial end(E) event.
    for (size_t i = 0; i < gProfiler.previousStack.size(); i++)
    {
        const FrameInfo& frame = gProfiler.previousStack.at(i);
        gProfiler.events.emplace_back('E', frame, gProfiler.currentTimestamp);
    }
    gProfiler.previousStack.clear();
}

void profilerDump(const char* path, LuteReporter& reporter)
{
    FILE* out = fopen(path, "w");
    if (!out)
    {
        reporter.formatError("Failed to open profile output file: %s", path);
        return;
    }

    fprintf(out, "{\"traceEvents\":[\n");

    for (size_t i = 0; i < gProfiler.events.size(); i++)
    {
        const TraceEvent& evt = gProfiler.events[i];

        if (i > 0)
            fprintf(out, ",\n");

        fprintf(out, "{\"ph\":\"%c\",", evt.phase);
        fprintf(out, "\"name\":\"%s\",", evt.name.c_str());
        fprintf(out, "\"cat\":\"function\",");
        fprintf(out, "\"pid\":1,\"tid\":1,");
        fprintf(out, "\"ts\":%llu", static_cast<unsigned long long>(evt.timestamp));
        fprintf(out, ",\"args\":{\"file\":\"%s\",\"line\":%d}}", evt.file.c_str(), evt.line);
    }

    fprintf(out, "\n]}\n");
    fclose(out);

    reporter.reportOutput("Profile written to " + std::string(path) + " (" + std::to_string(gProfiler.events.size()) + " events)");

    // The leaf-line histogram, in its own file beside the trace. Separate rather than folded into the trace because
    // the two answer different questions and are collected differently: the trace is a stack diff, this is a count.
    std::string linePath = std::string(path) + ".lines";
    FILE* lineOut = fopen(linePath.c_str(), "w");
    if (!lineOut)
    {
        reporter.formatError("Failed to open leaf-line output file: %s", linePath.c_str());
        return;
    }

    std::vector<std::pair<std::string, uint64_t>> lines(gProfiler.leafLines.begin(), gProfiler.leafLines.end());
    std::sort(lines.begin(), lines.end(), [](const std::pair<std::string, uint64_t>& a, const std::pair<std::string, uint64_t>& b) {
        if (a.second != b.second)
            return a.second > b.second;
        return a.first < b.first;
    });

    uint64_t total = 0;
    for (const auto& entry : lines)
        total += entry.second;

    // Say what this is, in the file, because the number invites a reading it does not support. A sample lands on the
    // line the interpreter is currently on, which for a call is the call site and for a loop is whichever line the
    // back edge reports, so this attributes to call sites and loop edges. It is not instruction level and a line's
    // share is not that line's cost in isolation.
    fprintf(lineOut, "# leaf-line histogram: ticks charged to the executing line of the leaf frame\n");
    fprintf(lineOut, "# call-site and loop-edge attribution, NOT instruction level\n");
    fprintf(lineOut, "# %llu ticks over %zu distinct lines\n", static_cast<unsigned long long>(total), lines.size());
    for (const auto& entry : lines)
    {
        double share = total > 0 ? double(entry.second) * 100.0 / double(total) : 0.0;
        fprintf(lineOut, "%12llu  %6.2f%%  %s\n", static_cast<unsigned long long>(entry.second), share, entry.first.c_str());
    }
    fclose(lineOut);

    reporter.reportOutput("Leaf lines written to " + linePath + " (" + std::to_string(lines.size()) + " lines)");
}
