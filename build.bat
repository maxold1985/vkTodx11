@echo off
setlocal

if "%VULKAN_SDK%"=="" (
    echo ERRO: VULKAN_SDK nao definido.
    pause
    exit /b 1
)

if not exist "third_party\SPIRV-Cross\spirv_hlsl.cpp" (
    call fetch_spirv_cross.bat
    if errorlevel 1 (
        pause
        exit /b 1
    )
)

if not exist build mkdir build

g++ ^
    -std=c++17 ^
    -O2 ^
    -municode ^
    -mwindows ^
    src\main.cpp ^
    src\app\application.cpp ^
    src\renderer\renderer.cpp ^
    src\renderer\buffer_utils.cpp ^
    src\shaders\embedded_spirv.cpp ^
    src\vk_d3d11.cpp ^
    third_party\SPIRV-Cross\spirv_cross.cpp ^
    third_party\SPIRV-Cross\spirv_parser.cpp ^
    third_party\SPIRV-Cross\spirv_cross_parsed_ir.cpp ^
    third_party\SPIRV-Cross\spirv_cfg.cpp ^
    third_party\SPIRV-Cross\spirv_glsl.cpp ^
    third_party\SPIRV-Cross\spirv_hlsl.cpp ^
    -I"%VULKAN_SDK%\Include" ^
    -Ithird_party\SPIRV-Cross ^
    -ld3d11 ^
    -ldxgi ^
    -luser32 ^
    -lgdi32 ^
    -o build\vkd3d11.exe

if errorlevel 1 (
    echo BUILD FALHOU
    pause
    exit /b 1
)

echo BUILD OK: build\vkd3d11.exe
pause
