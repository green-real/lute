#include "lute/luauflags.h"

#include "Luau/Common.h"
#include "Luau/ExperimentalFlags.h"

#include <cstdlib>
#include <stdexcept>
#include <string_view>

// The native code generator refuses a function past any of these and leaves it to the interpreter. The defaults suit
// hand-written Luau; a whole module compiled from something else lands in one file and can exceed them, and then the
// measurement compares native code against bytecode rather than comparing two compilers.
LUAU_FASTINT(CodegenHeuristicsInstructionLimit)
LUAU_FASTINT(CodegenHeuristicsBlockLimit)
LUAU_FASTINT(CodegenHeuristicsBlockInstructionLimit)

static void enableAllLuauFlags()
{
    for (Luau::FValue<bool>* flag = Luau::FValue<bool>::list; flag; flag = flag->next)
    {
        if (strncmp(flag->name, "Luau", 4) == 0 && !Luau::isAnalysisFlagExperimental(flag->name))
            flag->value = true;
    }
}

[[maybe_unused]] static void setLuauFlag(std::string_view name, bool state)
{
    for (Luau::FValue<bool>* flag = Luau::FValue<bool>::list; flag; flag = flag->next)
    {
        if (name == flag->name)
        {
            flag->value = state;
            return;
        }
    }

    throw std::runtime_error("Unrecognized Luau flag");
}

// Raise the native code generator's size heuristics. enableAllLuauFlags covers boolean flags only, so these integer
// limits keep their defaults however the run is configured, and a module large enough to exceed one silently drops to
// the interpreter with nothing in the output to say so.
static void raiseCodegenLimits()
{
    FInt::CodegenHeuristicsInstructionLimit.value = 1 << 28;
    FInt::CodegenHeuristicsBlockLimit.value = 1 << 22;
    FInt::CodegenHeuristicsBlockInstructionLimit.value = 1 << 24;
}

void setLuauFlags()
{
    enableAllLuauFlags();

    setLuauFlag("LuauRemovePrimitiveTypeConstraintAndSubtypingUnifier", false);
    // Individual flags can be overridden here as needed, e.g.:
    // setLuauFlag("LuauSomeFlagThatCausedARegression", false);

    if (const char* high = std::getenv("LUTE_CODEGEN_HIGH_LIMITS"); high && std::string_view(high) == "1")
        raiseCodegenLimits();
}
