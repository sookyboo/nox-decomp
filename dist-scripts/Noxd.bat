@echo off
setlocal EnableExtensions EnableDelayedExpansion

REM ---------------------------
REM Logging
REM ---------------------------
set "GAMEDIR=%~dp0"
cd /d "%GAMEDIR%"
set "LOG=%GAMEDIR%log.txt"
echo. > "%LOG%"

call :log "Starting launcher in %GAMEDIR%"

REM ---------------------------
REM Arch detection
REM ---------------------------
set "DEVICE_ARCH=x86"
if /i "%PROCESSOR_ARCHITECTURE%"=="AMD64" set "DEVICE_ARCH=x86_64"
if /i "%PROCESSOR_ARCHITECTURE%"=="ARM64" set "DEVICE_ARCH=aarch64"

set "RUN_ARCH=%DEVICE_ARCH%"
if /i "%RUN_ARCH%"=="x86_64" set "RUN_ARCH=i386"
if /i "%RUN_ARCH%"=="amd64"  set "RUN_ARCH=i386"
if /i "%RUN_ARCH%"=="aarch64" set "RUN_ARCH=armhf"

REM ---------------------------
REM Paths
REM ---------------------------
set "CONF_DIR=%GAMEDIR%conf"
set "SRC=%GAMEDIR%gamefiles"
set "ASSET_DIR=%SRC%\app"
set "SAVE_DIR=%ASSET_DIR%\Save"
set "NEEDED=%ASSET_DIR%\gamedata.bin"
set "MARKER=%ASSET_DIR%\converted_dialog.txt"
set "DIALOG_DIR=%ASSET_DIR%\Dialog"

set "INNOEXTRACT=%GAMEDIR%utils\innoextract.%DEVICE_ARCH%.exe"
set "FFMPEG=%GAMEDIR%utils\ffmpeg.%DEVICE_ARCH%.exe"

REM ---------------------------
REM Create dirs
REM ---------------------------
mkdir "%CONF_DIR%\Save" 2>nul
mkdir "%SAVE_DIR%" 2>nul

REM ---------------------------
REM Install / extract if needed
REM ---------------------------
if exist "%NEEDED%" goto :after_install

call :log "Nox data not extracted; checking for installer..."
set "FOUND_INSTALLER="
for %%F in ("%SRC%\setup_nox*.exe") do (
  if exist "%%~fF" set "FOUND_INSTALLER=%%~fF"
)

if defined FOUND_INSTALLER (
  call :log "Found installer: %FOUND_INSTALLER%"
  if not exist "%INNOEXTRACT%" (
    call :log "ERROR: missing %INNOEXTRACT%"
    echo Missing innoextract: "%INNOEXTRACT%"
    goto :end
  )
  call :log "Extracting installer with innoextract..."
  "%INNOEXTRACT%" "%FOUND_INSTALLER%" -d "%SRC%" >> "%LOG%" 2>&1
) else (
  call :log "No installer found (setup_nox*.exe)."
)

if not exist "%NEEDED%" (
  call :log "ERROR: Extraction failed; %NEEDED% still missing."
  echo Extraction failed; expected "%NEEDED%"
  goto :end
)

:after_install
call :log "Game data present."

REM ---------------------------
REM Convert dialog (skip if already done)
REM ---------------------------
if exist "%MARKER%" goto :after_convert

REM Skip conversion on 32-bit Windows (ffmpeg assumed 64-bit)
if /i "%PROCESSOR_ARCHITECTURE%"=="x86" (
  call :log "32-bit Windows detected; skipping dialog conversion."
  goto :after_convert
)

if not exist "%FFMPEG%" (
  call :log "ffmpeg not found; skipping dialog conversion (%FFMPEG%)."
  goto :after_convert
)

if not exist "%DIALOG_DIR%" (
  call :log "Dialog dir not found; skipping conversion (%DIALOG_DIR%)."
  goto :after_convert
)

call :log "Converting dialog WAV files to mono 22050Hz PCM..."
for %%W in ("%DIALOG_DIR%\*.wav") do (
  if exist "%%~fW" (
    call :log "Converting %%~nxW"
    "%FFMPEG%" -y -loglevel error -i "%%~fW" -ac 1 -ar 22050 -c:a pcm_s16le -f wav "%%~fW.tmp" >> "%LOG%" 2>&1
    if exist "%%~fW.tmp" (
      move /y "%%~fW.tmp" "%%~fW" >nul
    ) else (
      call :log "ERROR converting %%~nxW"
      echo Error converting %%~nxW
      goto :end
    )
  )
)

echo Dialog audio converted on %DATE% %TIME%> "%MARKER%"
call :log "Dialog conversion complete."

:after_convert

REM ---------------------------
REM Ensure/config nox.cfg
REM ---------------------------
if not exist "%ASSET_DIR%" mkdir "%ASSET_DIR%" 2>nul

if not exist "%ASSET_DIR%\nox.cfg" (
  if exist "%GAMEDIR%nox.cfg" (
    copy /y "%GAMEDIR%nox.cfg" "%ASSET_DIR%\nox.cfg" >nul
    call :log "Copied nox.cfg into assets."
  ) else (
    call :log "ERROR: nox.cfg not found in game dir."
    echo Missing "%GAMEDIR%nox.cfg"
    goto :end
  )
)

REM Simple default video mode (keep it easy in .bat)
set "NOX_GAME_WIDTH=1024"
set "NOX_GAME_HEIGHT=768"
set "NOX_GAME_BITS=16"
set "NOX_GAME_FULLSCREEN=1"

REM Update config lines using PowerShell inline (no ps1 file, minimal friction)
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$p='%ASSET_DIR%\nox.cfg';" ^
  "$c=Get-Content -Raw $p;" ^
  "$c=[regex]::Replace($c,'(?m)^\s*VideoMode\s*=.*$','VideoMode = %NOX_GAME_WIDTH% %NOX_GAME_HEIGHT% %NOX_GAME_BITS%');" ^
  "$c=[regex]::Replace($c,'(?m)^\s*Fullscreen\s*=.*$','Fullscreen = %NOX_GAME_FULLSCREEN%');" ^
  "Set-Content -Encoding ASCII -Path $p -Value $c;" >> "%LOG%" 2>&1

REM ---------------------------
REM Environment vars
REM ---------------------------
set NOX_CONTROL_SERVER=0
set NOX_CONTROL_SERVER_PASSWORD=secret
set NOX_CONTROL_SERVER_BIND=127.0.0.1
set NOX_CONTROL_SERVER_PORT=2323
set NOX_CAPTURE_INPUT=0
set NOX_NET_LOG=0
set NOX_WSA_LOG=0
set NOX_PACKET_LOG=0
set NOX_SKIP_INTRO_MOVIES=0
set NOX_LOBBY_REGISTER_ENABLE=0
set NOX_UPNP_ENABLE=0
set NOX_LIMIT_RANGE_ON_RUN=1
set NOX_LIMIT_RANGE_ON_RUN_RADIUS=118
set NOX_GAMEPAD_INI=%GAMEDIR%nox.gptk2.ini
set NOX_GAMEPAD_EXIT=1
set NOX_GAMEPAD=1

REM ---------------------------
REM Launch
REM ---------------------------
set "EXE=%GAMEDIR%noxd.%RUN_ARCH%.exe"
if not exist "%EXE%" set "EXE=%GAMEDIR%noxd.exe"

if not exist "%EXE%" (
  call :log "ERROR: no game binary found (noxd.%RUN_ARCH%.exe or noxd.exe)."
  echo Missing noxd executable.
  goto :end
)

call :log "Launching %EXE%"
pushd "%ASSET_DIR%"
start "" /D "%ASSET_DIR%" "%EXE%"
popd

goto :end

:log
echo [%DATE% %TIME%] %~1
>> "%LOG%" echo [%DATE% %TIME%] %~1
exit /b 0

:end
call :log "Done."
endlocal
exit /b 0
