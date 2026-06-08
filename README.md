# ESP32-S3-RLCD-Monitor

[English](README.en.md)

把你的 DeepSeek 实时用量显示在 Waveshare ESP32-S3-RLCD-4.2 反射式 LCD 上的桌面摆件。

![实物效果图](device_photo.png)

## 实现逻辑

```
bridge 守护进程                            ESP32-S3-RLCD-4.2
──────────────                            ─────────────────
• 获取 DeepSeek 账户余额                   • 开机连接 Wi-Fi
• 获取室外天气（open-meteo，免 key）       • 每 60 秒 GET /api/usage
• 提取 DeepSeek 今日/本月 tokens           • LVGL 单栏显示：
  调用 ccusage 解析 Claude Code 日志        · 时间 + 天气 + 状态图标
• 缓存结果，在 :7777 提供 JSON 服务          · 居中 DeepSeek 余额
                                            · 今日/本月 tokens、费用、缓存率
                                          • 读取室内温湿度（SHTC3）
                                          • NTP 对时显示时间
```

### 显示效果

```
14:30 Overcast                    [≡]
FC 22-30°C / IN 26.3°C / RH 65%
           🐋 DEEPSEEK
              balance
              ¥37.42
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
   TODAY              MONTH
  161M tokens       230M tokens
   ¥5.64              ¥8.08
 cache 99.0%        cache 99.0%
```

## 硬件

- [Waveshare ESP32-S3-RLCD-4.2](https://www.waveshare.com/wiki/ESP32-S3-RLCD-4.2) — 4.2" 反射式 LCD（类纸面），ESP32-S3，WiFi，RTC，温湿度，SD，音频。
  - 国内购买：[天猫链接](https://detail.tmall.com/item.htm?id=1010403328696)
- USB-C 数据线（用于烧录）。

## 快速开始

### 前置工具

- [ESP-IDF v5.x](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/get-started/) — Windows 下载 Universal Online Installer，目标芯片选 `esp32s3`
- Node.js（可选，仅当需要 DeepSeek 今日/本月 Token 统计数据时需要 ccusage）

### 编译烧录固件

```powershell
# 配置 secrets
cd D:\CCWorkspace\ESP32-S3-RLCD-Monitor\firmware
copy main\secrets.h.example main\secrets.h
# 编辑 secrets.h：WiFi SSID/密码、bridge URL

# 编译烧录（仅首次需要 set-target）
idf.py set-target esp32s3
idf.py build flash monitor
```

#### secrets.h 配置

| 字段 | 示例 | 说明 |
|------|------|------|
| `RLCD_WIFI_SSID` | `"MyWiFi"` | 仅支持 2.4GHz |
| `RLCD_WIFI_PASSWORD` | `"password"` | WPA2 |
| `RLCD_BRIDGE_URL` | `"http://192.168.0.129:7777/api/usage"` | bridge 地址（局域网 IP） |
| `RLCD_BRIDGE_TOKEN` | `""` | 未设置则留空 |
| `RLCD_POLL_SEC` | `60` | 轮询间隔 |

### 启动 bridge

```powershell
cd D:\CCWorkspace\ESP32-S3-RLCD-Monitor\bridge
set DEEPSEEK_API_KEY=sk-xxxxx
pip install fastapi uvicorn pydantic
python bridge.py
```

验证数据：
```powershell
curl http://localhost:7777/api/usage
curl 'http://localhost:7777/api/usage?mock=1'  # 模拟数据，无需 ccusage
```

#### 开机自启（可选）

`scripts/start_bridge.bat` 使用 `pythonw.exe` 无窗口后台运行。放入 `shell:startup` 即可开机启动。

#### 环境变量（`.env`）

```ini
DEEPSEEK_API_KEY=sk-xxxxx          # DeepSeek 余额显示
RLCD_WEATHER_LAT=31.2304           # 纬度（默认上海）
RLCD_WEATHER_LON=121.4737          # 经度
RLCD_WEATHER_CITY=Shanghai         # 城市名
RLCD_AUTH_TOKEN=<随机串>            # 非局域网访问时必设
RLCD_USD_CNY=7.25                  # 汇率（USD→CNY）
```

## 省电特性

| 特性 | 说明 |
|:----|:-----|
| Modem Sleep | WiFi 射频空闲时关闭，省 ~50mA |
| CPU 自动变频 | 空闲 80MHz / 峰值 240MHz |
| Light Sleep | 双核空闲时自动进入暂停模式 |
| 编译优化 -Os | 减小代码体积，间接省电 |
| 关闭 AMPDU | 小 HTTP 请求无需聚合 |

**预估续航**（2500mAh）：~15-25mA 平均 → **100-160 小时**

## 项目结构

```
ESP32-S3-RLCD-Monitor/
├── bridge/                    # Python FastAPI bridge 守护进程
│   ├── bridge.py              # 主程序 + 后台刷新缓存
│   ├── schema.py              # Pydantic 响应模型
│   ├── sources/
│   │   ├── deepseek.py        # DeepSeek 余额 API
│   │   └── weather.py         # open-meteo（免 API key）
│   └── .env                   # 本地配置（已 gitignore）
├── firmware/                  # ESP-IDF v5 + LVGL v9 项目
│   ├── main/
│   │   ├── secrets.h.example  # → 复制为 secrets.h
│   │   └── user_config.h      # 引脚定义
│   └── components/
│       ├── net_app/           # WiFi STA + NTP
│       ├── sensor/            # SHTC3 温湿度
│       ├── usage_client/      # HTTP 轮询 + cJSON 解析
│       └── ui_app/            # LVGL 仪表盘 + 图标
└── scripts/
    ├── start_bridge.bat       # Windows 后台启动脚本
    └── copy_xiaozhi_icons.py  # 图标生成工具
```

## 许可证

MIT
