add_library(Lute.Require)

target_sources(Lute.Require PRIVATE
    include/lute/batteriesvfs.h
    include/lute/bundlevfs.h
    include/lute/clivfs.h
    include/lute/filevfs.h
    include/lute/lutevfs.h
    include/lute/modulepath.h
    include/lute/options.h
    include/lute/packagerequirevfs.h
    include/lute/require.h
    include/lute/requirevfs.h
    include/lute/stdlibvfs.h
    include/lute/userlandvfs.h

    src/batteriesvfs.cpp
    src/bundlevfs.cpp
    src/clivfs.cpp
    src/filevfs.cpp
    src/lutevfs.cpp
    src/modulepath.cpp
    src/options.cpp
    src/packagerequirevfs.cpp
    src/require.cpp
    src/requirevfs.cpp
    src/stdlibvfs.cpp
    src/userlandvfs.cpp

    # wasm-luau fork: the Luau CLI's codegen counters, which upstream builds only into its Repl rather than into
    # Luau.CLI.lib. This is where lute native-compiles a required module, so they are compiled in here.
    ${PROJECT_SOURCE_DIR}/extern/luau/CLI/src/Counters.cpp
)

target_compile_features(Lute.Require PUBLIC cxx_std_17)
target_include_directories(Lute.Require PUBLIC include)
target_link_libraries(Lute.Require PUBLIC Luau.Require Luau.Compiler PRIVATE Luau.CLI.lib Luau.CodeGen Lute.Common Lute.Std Lute.CLI.Commands Lute.Batteries Lute.Std Lute.Lute Lute.Crypto Lute.Fs Lute.IO Lute.Luau Lute.Net Lute.Syntax Lute.Task Lute.VM Lute.Process Lute.System Lute.Time Lute.Runtime Lute.Debug)
target_compile_options(Lute.Require PRIVATE ${LUTE_OPTIONS})
