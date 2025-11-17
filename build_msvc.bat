@echo off

call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1

if errorlevel 1 (
    echo Cannot vcvars64.bat
    pause
    exit /b 1
)

cl.exe /nologo /EHsc /std:c++17 /O2 ^
    /I. ^
    main.cpp ^
    catboost_lib\catboostmodel.lib ^
    /Fe:inference.exe

if %ERRORLEVEL% EQU 0 (
    echo.
    copy /Y catboost_lib\catboostmodel.dll . >nul 2>&1
    echo DLL copied
    echo.
) else (
    echo.
    echo FAILED
)

pause