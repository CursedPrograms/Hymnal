@echo off
setlocal
cd /d "%~dp0"
rem Builds Hymnal (Release) and dist\Hymnal-win64.zip, with both of its engines:
rem   engine\cuda  ace-server for NVIDIA graphics cards (needs the acestep.cpp CUDA build and the CUDA 12 toolkit's DLLs)
rem   engine\cpu   ace-server for any PC
rem Needs Visual Studio 2022 (C++) and CMake. SDL2 and Dear ImGui are fetched into external\ once.
rem ACESTEP points at an acestep.cpp checkout with build-cuda\ and build-cpu\ built (see README).
if not defined ACESTEP set "ACESTEP=D:\cc\_src\acestep.cpp"
if not defined CUDA_BIN set "CUDA_BIN=%ProgramFiles%\NVIDIA GPU Computing Toolkit\CUDA\v12.9\bin"
if not exist "external\SDL2-2.30.9\cmake" (
    echo Getting SDL2's development package, once...
    if not exist external mkdir external
    curl -sL -o external\SDL2-devel.zip https://github.com/libsdl-org/SDL/releases/download/release-2.30.9/SDL2-devel-2.30.9-VC.zip || goto :error
    tar -xf external\SDL2-devel.zip -C external || goto :error
    del external\SDL2-devel.zip
)
rem BUILD_DIR: another build folder, for packing while the usual build is open
if not defined BUILD_DIR set "BUILD_DIR=build"
cmake -B %BUILD_DIR% -G "Visual Studio 17 2022" -A x64 || goto :error
cmake --build %BUILD_DIR% --config Release || goto :error
if exist dist\Hymnal rmdir /s /q dist\Hymnal
mkdir dist\Hymnal
copy /y %BUILD_DIR%\Release\Hymnal.exe dist\Hymnal\ >nul || goto :error
copy /y %BUILD_DIR%\Release\SDL2.dll dist\Hymnal\ >nul || goto :error
xcopy /e /i /q /y %BUILD_DIR%\Release\assets dist\Hymnal\assets >nul
rem Microsoft's Visual C++ runtime, for the engines (Hymnal itself has it built in)
set "CRT="
for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -find VC\Redist\MSVC\*\x64\Microsoft.VC143.CRT`) do set "CRT=%%i"
if not defined CRT echo Visual C++ runtime folder not found, is Visual Studio 2022 installed?& goto :error
for %%e in (cuda cpu) do (
    if exist "%ACESTEP%\build-%%e\ace-server.exe" (
        mkdir dist\Hymnal\engine\%%e
        copy /y "%ACESTEP%\build-%%e\ace-server.exe" dist\Hymnal\engine\%%e\ >nul
        copy /y "%ACESTEP%\build-%%e\ggml*.dll" dist\Hymnal\engine\%%e\ >nul
        for %%f in (vcruntime140.dll vcruntime140_1.dll msvcp140.dll) do copy /y "%CRT%\%%f" dist\Hymnal\engine\%%e\ >nul
    ) else echo No %%e engine at %ACESTEP%\build-%%e: Hymnal is packed without it.
)
if exist dist\Hymnal\engine\cuda for %%f in (cudart64_12.dll cublas64_12.dll cublasLt64_12.dll) do copy /y "%CUDA_BIN%\%%f" dist\Hymnal\engine\cuda\ >nul || goto :error
for %%f in (README.md LICENSE NOTICE install.bat install.ps1) do copy /y %%f dist\Hymnal\ >nul
if exist dist\Hymnal-win64.zip del dist\Hymnal-win64.zip
"%SystemRoot%\System32\tar.exe" -a -c -f dist\Hymnal-win64.zip -C dist Hymnal || goto :error
echo Built: build\Release\Hymnal.exe and dist\Hymnal-win64.zip
rem installed on this PC: this build replaces that copy too
if exist "%LOCALAPPDATA%\Programs\Hymnal\Hymnal.exe" call "%~dp0install.bat" --quiet
exit /b 0
:error
echo Build failed, see above.
pause
exit /b 1
