' Start bridge silently (no cmd window)
CreateObject("WScript.Shell").Run _
    "D:\CCWorkspace\ESP32-S3-RLCD-Monitor\scripts\start_bridge.bat", _
    0, False
