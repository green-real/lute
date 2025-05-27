target_sources(Lute.Runtime PRIVATE
    runtime/include/lute/options.h
    runtime/include/lute/ref.h
    runtime/include/lute/require.h
    runtime/include/lute/requireutils.h
    runtime/include/lute/runtime.h
    runtime/include/lute/userdatas.h

    runtime/src/options.cpp
    runtime/src/ref.cpp
    runtime/src/require.cpp
    runtime/src/requireutils.cpp
    runtime/src/runtime.cpp
)

target_sources(Lute.Crypto PRIVATE
    crypto/include/lute/crypto.h

    crypto/src/crypto.cpp
)

target_sources(Lute.FFI PRIVATE
    ffi/include/lute/ffi.h
    ffi/include/lute/ffi/ctype.h
    ffi/include/lute/ffi/cdata.h
    ffi/include/lute/ffi/dlib.h
    ffi/include/lute/ffi/state.h
    ffi/include/lute/ffi/utils.h

    ffi/src/ffi.cpp
    ffi/src/ffi_c.cpp
    ffi/src/dlib.cpp
    ffi/src/state.cpp
    ffi/src/utils.cpp

    # ctype src
    ffi/src/ctype/array.cpp
    ffi/src/ctype/base.cpp
    ffi/src/ctype/core.cpp
    ffi/src/ctype/func.cpp
    ffi/src/ctype/pointer.cpp
    ffi/src/ctype/struct.cpp

    # cdata src
    ffi/src/cdata/array.cpp
    ffi/src/cdata/base.cpp
    ffi/src/cdata/core.cpp
    ffi/src/cdata/func.cpp
    ffi/src/cdata/pointer.cpp
    ffi/src/cdata/struct.cpp
)

target_sources(Lute.Fs PRIVATE
    fs/include/lute/fs.h

    fs/src/fs.cpp
)

target_sources(Lute.Luau PRIVATE
    luau/include/lute/luau.h

    luau/src/luau.cpp
)

target_sources(Lute.Net PRIVATE
    net/include/lute/net.h

    net/src/net.cpp
)

target_sources(Lute.Std PRIVATE
    std/include/lute/stdlib.h

    std/src/stdlib.cpp
    std/src/generated/modules.h
    std/src/generated/modules.cpp
)

target_sources(Lute.Task PRIVATE
    task/include/lute/task.h

    task/src/task.cpp
)

target_sources(Lute.VM PRIVATE
    vm/include/lute/spawn.h
    vm/include/lute/vm.h

    vm/src/spawn.cpp
    vm/src/vm.cpp
)

target_sources(Lute.Process PRIVATE
    process/include/lute/process.h

    process/src/process.cpp
)

target_sources(Lute.CLI PRIVATE
    cli/main.cpp
    cli/tc.h
    cli/tc.cpp
)

target_sources(Lute.System PRIVATE
    system/include/lute/system.h

    system/src/system.cpp
)

target_sources(Lute.Time PRIVATE
	time/include/lute/time.h

	time/src/time.cpp
)

target_sources(Lute.Test PRIVATE
    tests/src/doctest.h
    tests/src/main.cpp

    tests/src/require.test.cpp
)