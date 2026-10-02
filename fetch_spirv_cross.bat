@echo off
setlocal

if exist third_party\SPIRV-Cross\spirv_hlsl.cpp (
    echo SPIRV-Cross ja esta presente.
    exit /b 0
)

where git >nul 2>nul
if errorlevel 1 (
    echo ERRO: git nao encontrado.
    echo Baixe SPIRV-Cross da Khronos e extraia em:
    echo third_party\SPIRV-Cross
    exit /b 1
)

if not exist third_party mkdir third_party

git clone --depth 1 https://github.com/KhronosGroup/SPIRV-Cross.git third_party\SPIRV-Cross
if errorlevel 1 exit /b 1

echo SPIRV-Cross pronto.
