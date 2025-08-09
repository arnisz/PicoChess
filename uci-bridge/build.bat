@echo off
echo Building UCI Bridge for Pi Pico Chess Engine...

:: Set build parameters
set GOOS=windows
set GOARCH=amd64
set CGO_ENABLED=0

:: Build the executable
go build -ldflags="-s -w" -o uci-bridge.exe main.go

if %ERRORLEVEL% EQU 0 (
    echo ✅ Build successful: uci-bridge.exe
    echo.
    echo Usage:
    echo   uci-bridge.exe -port COM15 -baud 115200 -debug
    echo.
) else (
    echo ❌ Build failed!
    pause
    exit /b 1
)

pause