@echo off
echo ===================================================
echo   Compiling Memory Defragmenter and Optimizer...
echo ===================================================

echo Compiling resource manifest...
windres resource.rc -O coff -o resource.o
if %ERRORLEVEL% neq 0 (
    echo Resource compilation failed!
    exit /b %ERRORLEVEL%
)

echo Compiling C files...
gcc main.c optimizer.c resource.o -o MemoryOptimizer.exe -mwindows -lpsapi -lshlwapi -ldwmapi -lcomctl32 -Wall -O2
if %ERRORLEVEL% neq 0 (
    echo C compilation failed!
    exit /b %ERRORLEVEL%
)

echo.
echo ===================================================
echo   Compilation Successful! Created MemoryOptimizer.exe
echo ===================================================
