@echo off
setlocal
if not exist "build\vkd3d11.exe" (
    echo ERRO: build\vkd3d11.exe nao existe.
    pause
    exit /b 1
)
if exist "fxc.exe" copy /y "fxc.exe" "build\fxc.exe" >nul
pushd build
vkd3d11.exe
set RC=%ERRORLEVEL%
popd
echo Exit code: %RC%
pause
