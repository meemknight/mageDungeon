@echo off
setlocal EnableExtensions EnableDelayedExpansion

REM Compiles every stage-extension shader in this folder to SPIR-V.
REM Expected naming pattern: *.vert, *.frag, *.comp, etc.

where glslc >nul 2>nul
if errorlevel 1 (
	echo [ERROR] glslc was not found in PATH.
	echo Install Vulkan SDK or add glslc.exe to PATH.
	exit /b 1
)

pushd "%~dp0" >nul || (
	echo [ERROR] Could not open shader directory.
	exit /b 1
)

set "count=0"
set "failed=0"

for %%F in (*) do (
	set "stage="
	for %%I in ("%%~xF") do set "stage=%%~I"
	if defined stage set "stage=!stage:~1!"

	set "useExplicitStage=0"
	if /I "!stage!"=="vert" set "useExplicitStage=1"
	if /I "!stage!"=="frag" set "useExplicitStage=1"
	if /I "!stage!"=="comp" set "useExplicitStage=1"
	if /I "!stage!"=="geom" set "useExplicitStage=1"
	if /I "!stage!"=="tesc" set "useExplicitStage=1"
	if /I "!stage!"=="tese" set "useExplicitStage=1"
	if /I "!stage!"=="rgen" set "useExplicitStage=1"
	if /I "!stage!"=="rint" set "useExplicitStage=1"
	if /I "!stage!"=="rahit" set "useExplicitStage=1"
	if /I "!stage!"=="rchit" set "useExplicitStage=1"
	if /I "!stage!"=="rmiss" set "useExplicitStage=1"
	if /I "!stage!"=="rcall" set "useExplicitStage=1"
	if /I "!stage!"=="task" set "useExplicitStage=1"
	if /I "!stage!"=="mesh" set "useExplicitStage=1"

	if "!useExplicitStage!"=="1" (
		set /a count+=1
		echo [glslc] %%F ^(stage=!stage!^)
		glslc -fshader-stage=!stage! "%%~fF" -o "%%~fF.spv"

		if errorlevel 1 (
			set /a failed+=1
			echo [ERROR] Failed to compile %%F
		) else (
			echo [OK] Wrote %%~nxF.spv
		)
	)
)

if !count! EQU 0 (
	echo [INFO] No stage-extension shader files found in %~dp0
	echo [INFO] Expected extensions: .vert .frag .comp .geom .tesc .tese .rgen .rint .rahit .rchit .rmiss .rcall .task .mesh
	popd >nul
	exit /b 0
)

echo.
if !failed! GTR 0 (
	echo [DONE] Completed with !failed! shader compile error^(s^).
	popd >nul
	exit /b 1
)

echo [DONE] Successfully compiled !count! shader file^(s^).
popd >nul
exit /b 0
