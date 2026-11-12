# Installs Hymnal for this user (%LOCALAPPDATA%\Programs\Hymnal, Start menu + Desktop shortcuts), through install.bat.
& cmd /c "`"$PSScriptRoot\install.bat`" $args"
exit $LASTEXITCODE
