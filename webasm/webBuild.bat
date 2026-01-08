@echo off
setlocal

cd ..

call emsdk_env.bat
call emcmake cmake -S . -B build-web -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-web

pause
endlocal