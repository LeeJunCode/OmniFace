编译

    cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release --target OmniFace

打包

    powershell -ExecutionPolicy Bypass -File scripts/package.ps1