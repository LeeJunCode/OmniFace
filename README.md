编译

    cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release --target OmniFace

    cmake -B build -DCMAKE_CXX_FLAGS_RELEASE="/O2 /Ob2 /DNDEBUG /GL" -DCMAKE_SHARED_LINKER_FLAGS_RELEASE="/LTCG" -DCMAKE_EXE_LINKER_FLAGS_RELEASE="/LTCG"
    cmake --build build --config Release

打包

    powershell -ExecutionPolicy Bypass -File scripts/package.ps1