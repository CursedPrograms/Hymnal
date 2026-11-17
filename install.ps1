# Installs Hymnal for this user (%LOCALAPPDATA%\Programs\Hymnal, Start menu and Desktop shortcuts named "Atelier - Hymnal"), through install.bat.
& cmd /c "`"$PSScriptRoot\install.bat`" $args"
exit $LASTEXITCODE
