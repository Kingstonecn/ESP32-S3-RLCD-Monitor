@echo off
cd /d D:\CCWorkspace\ESP32-S3-RLCD-Monitor\bridge
if "%DEEPSEEK_API_KEY%"=="" (
    echo Error: DEEPSEEK_API_KEY not set. Create bridge/.env or set system env var. >&2
    exit /b 1
)
"D:\anaconda3\pythonw.exe" bridge.py > bridge.log 2>&1
