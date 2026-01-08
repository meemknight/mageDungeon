@echo off
setlocal
cd ..

echo === Entering Emscripten environment ===
call emsdk_env.bat
if errorlevel 1 goto fail

echo.
echo === Configuring WebAssembly (Release) ===
emcmake cmake -S . -B build-web -G Ninja -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 goto fail

echo.
echo === Building ===
cmake --build build-web
if errorlevel 1 goto fail

echo.
echo ===== BUILD SUCCESS =====
pause
exit /b 0

:fail
echo.
echo ===== BUILD FAILED =====
pause
exit /b 1
