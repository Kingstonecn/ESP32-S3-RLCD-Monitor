@echo off
cd /d D:\CCWorkspace\ESP32-S3-RLCD-Monitor\bridge
set DEEPSEEK_API_KEY=REMOVED
"D:\anaconda3\pythonw.exe" bridge.py > bridge.log 2>&1
