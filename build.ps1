# Builds Hymnal (Release) and dist\Hymnal-win64.zip with its engines, through build.bat (Visual Studio 2022 + CMake).
$ErrorActionPreference = 'Stop'
& cmd /c "`"$PSScriptRoot\build.bat`""
exit $LASTEXITCODE
