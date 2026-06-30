# ESP32-S3-RLCD-Monitor

[English](README.en.md)

把你的 **DeepSeek** 用量 + **OpenCode Go** 配额显示在 Waveshare ESP32-S3-RLCD-4.2 反射式 LCD 上的桌面摆件。**纯 DeepSeek + OpenCode 数据**，不包含 Claude。

## 实现逻辑

```
~/.claude/**/*.jsonl           （Claude Code 会话日志 → ccusage 解析 DeepSeek Token）
         │
         ▼
   bridge 守护进程                            ESP32-S3-RLCD-4.2
   ──────────────                            ─────────────────
   • 调用 ccusage 解析 DeepSeek Token       • 开机连接 Wi-Fi
   • 获取 DeepSeek 余额（官方 API）          • 每 300 秒 GET /api/usage
   • 获取室外天气（open-meteo 免 key）      • 用 cJSON 解析 JSON
   • 抓取 OpenCode Go 配额                   • 单栏 UI（两套视图可切换）：
   • 缓存结果，在 :7777 提供 JSON 服务           ▸ DeepSeek：居中余额 + 今日/本月 Token
                                                ▸ OpenCode：5h / 周 / 月 三条进度条
                                              • 读取室内温湿度（SHTC3）
                                              • NTP 对时（CST-8）显示时间
                                              • GPIO18 按键 → 即时切换视图
```

bridge 以 `pythonw.exe` 后台进程形式运行在与 Claude Code 同一台机器上，开机自启。后台线程每 45 秒刷新一次缓存（通过 `RLCD_REFRESH_SEC` 环境变量可调），使 ESP32 的 HTTP 请求始终从缓存返回。

DeepSeek 的 `today_tokens` 来自 ccusage 解析 Claude Code 日志，`balance` 来自 DeepSeek 官方 API。OpenCode Go 配额通过抓取 Dashboard 页面获取（无公开 API）。

### 显示效果

**DeepSeek 模式（默认）：**

```
14:30 Overcast                    [≡]
FC 22-30°C / IN 26.3°C / RH 65%
           🐋 DEEPSEEK
              balance
              ¥ 70.79
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 TODAY              MONTH
 2.4M tokens       18.0M tokens
  ¥5.42              ¥38.15
 cache 96.8%        cache 95.2%
```

**OpenCode Go 模式（按 GPIO18 按键切换）：**

```
14:30 Overcast                    [≡]
FC 22-30°C / IN 26.3°C / RH 65%
           OPENCODE
             Go Plan
───────────────────────────────────────
5 HOUR  [████░░░░]  62%    reset in 2h 14m
WEEK    [████░░░░]  41%    reset in 2d 22h
MONTH   [████░░░░]  68%    reset in 12d 5h
```

按 GPIO18 按键在 DeepSeek ↔ OpenCode 之间即时切换，无需等待下一次 HTTP 轮询（缓存在 firmware 本地）。

## 硬件

- **[Waveshare ESP32-S3-RLCD-4.2](https://www.waveshare.com/wiki/ESP32-S3-RLCD-4.2)** — 4.2 英寸反射式 LCD（类纸面，400×300 1-bit），ESP32-S3，Wi-Fi，RTC，SHTC3 温湿度，SD 卡槽，音频。
  - 国内购买：[天猫链接](https://detail.tmall.com/item.htm?id=1010403328696)
- **USB-C 数据线**（用于烧录固件）

## 架构

```
Windows 主机 / Linux 主机                        ESP32-S3-RLCD-4.2
───────────────                                  ─────────────────
~/.claude/**/*.jsonl                              LVGL 单栏 UI
        │                                                ▲
        ▼             局域网 HTTP (300s)                  │
   bridge 守护进程 ── GET /api/usage ─────────────────────┘
   （ccusage + DeepSeek API + OpenCode 抓取）
   :7777

                                              ┌─ GPIO18 KEY ─────┐
                                              │ DeepSeek ↔ OpenCode│
                                              │ 即时切换，本地缓存  │
                                              └──────────────────┘
```

### 组件说明

| 组件 | 路径 | 说明 |
|:----|:-----|:------|
| **Bridge** | `bridge/` | Python FastAPI 守护进程，汇总所有数据在 `:7777` 提供 JSON |
| **固件** | `firmware/` | ESP-IDF + LVGL v9 项目，渲染双视图仪表盘 |
| **按键** | `user_app.cpp` | GPIO18 按键检测，切换视图 + 唤醒轮询任务 |
| **脚本** | `scripts/` | 启动脚本、图标生成工具、Linux systemd 安装脚本等 |

## 快速开始

### 1. 安装前置工具

- **ESP-IDF v5.x** — [下载 Universal Online Installer](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/get-started/)，目标芯片选 `esp32s3`
- **Node.js** — 可选，仅当需要 DeepSeek 今日/本月 Token 统计时需要 `ccusage`

### 2. 克隆项目

```powershell
git clone https://github.com/Kingstonecn/ESP32-S3-RLCD-Monitor.git
cd ESP32-S3-RLCD-Monitor
```

### 3. 启动 bridge 守护进程

```powershell
cd bridge

:: 安装依赖（二选一）
pip install fastapi uvicorn pydantic python-dotenv   # pip 直接装
:: 或：uv sync  （如果使用 uv 包管理器，自动从 pyproject.toml 安装）

:: 创建 .env 配置文件（已 gitignore）
:: 填写以下内容（详见环境变量章节）：
::   DEEPSEEK_API_KEY=sk-xxxxx
::   OPENCODE_GO_WORKSPACE_ID=wrk_xxxxx
::   OPENCODE_GO_AUTH_COOKIE=Fe26.2**...

:: 启动 bridge（前台调试用）
python bridge.py
```

验证数据：

```powershell
curl http://localhost:7777/api/usage          # 实时数据
curl "http://localhost:7777/api/usage?mock=1" # 模拟数据（不依赖 ccusage / DeepSeek API）
curl http://localhost:7777/healthz            # 健康检查
```

### 4. 配置开机自启（Windows）

项目提供两种启动方式：

```powershell
:: 方式一：后台启动（短暂黑框后消失）
scripts\start_bridge.bat

:: 方式二：完全无窗口启动（推荐）
scripts\start_bridge.vbs

:: 设置开机自启：将 start_bridge.vbs 放入启动文件夹
:: Win+R → shell:startup → 将 start_bridge.vbs 复制进去
```

### 5. 编译并烧录固件

#### 配置 secrets

```powershell
cd firmware
copy main\secrets.h.example main\secrets.h
# 编辑 secrets.h：填入 WiFi SSID/密码、bridge 地址
```

**secrets.h 各项说明：**

| 字段 | 示例 | 说明 |
|:-----|:-----|:------|
| `RLCD_WIFI_SSID` | `"MyNetwork"` | 仅支持 2.4 GHz（ESP32 不支持 5 GHz） |
| `RLCD_WIFI_PASSWORD` | `"password"` | WPA2 |
| `RLCD_BRIDGE_URL` | `"http://192.168.0.129:7777/api/usage"` | bridge 的局域网地址 |
| `RLCD_BRIDGE_TOKEN` | `""` | 与 `.env` 中 `RLCD_AUTH_TOKEN` 一致，未设置则留空 |
| `RLCD_POLL_SEC` | `300` | 轮询间隔（秒）；按 GPIO18 键会强制立即轮询 |

#### 编译烧录

```powershell
:: 仅首次需要 set-target
idf.py set-target esp32s3

:: 编译 → 烧录 → 串口监视器
idf.py build flash monitor
```

建议先用 mock 模式测试 UI：将 `RLCD_BRIDGE_URL` 末尾加上 `?mock=1`，确认屏幕正常渲染后再切回实时模式。

### 6. 验证

连接成功时串口打印 `connecting to <SSID>...` → `got IP`，随后仪表盘逐项填充数据。按 GPIO18 按键可在 DeepSeek / OpenCode 视图间切换。

## 环境变量

创建 `bridge/.env`（已在 `.gitignore` 中），按需填写：

```ini
# === 必填 ===
DEEPSEEK_API_KEY=sk-xxxxx          # DeepSeek 余额显示

# === OpenCode Go（可选，不设则 opencode 显示不可用） ===
OPENCODE_GO_WORKSPACE_ID=wrk_xxxxx           # 从 Dashboard URL 获取
OPENCODE_GO_AUTH_COOKIE=Fe26.2**...          # 浏览器 cookie，登录后抓取

# === 天气（可选，不设则自动用 open-meteo，免 key） ===
RLCD_WEATHER_LAT=31.2304           # 纬度（默认上海）
RLCD_WEATHER_LON=121.4737          # 经度
RLCD_WEATHER_CITY=Shanghai         # 设备上显示的城市名（≤8 字符）

# === 安全（可选，非局域网访问时必须设置） ===
RLCD_AUTH_TOKEN=<随机串>            # 与 secrets.h 中的 RLCD_BRIDGE_TOKEN 一致

# === 其他 ===
# RLCD_REFRESH_SEC=45              # 后台缓存刷新间隔（默认 45s）
# RLCD_USD_CNY=7.25                # USD→CNY 汇率
```

修改 `.env` 后重启 bridge 进程。

## 按键功能

| 操作 | 功能 |
|:----|:------|
| 按 GPIO18 | DeepSeek ↔ OpenCode 视图切换 |
| 按键时 | 同时重置 HTTP 失败计数，并唤醒轮询任务立即拉取新数据 |

切换是**即时的**——firmware 缓存了上一次的成功响应，不需要等待下一次 HTTP 轮询（最长 300s）。每次按键也会触发一次新的 HTTP 请求，结果在下一次 `ui_app_update` 时刷新。

## OpenCode Go 配额说明

OpenCode Go 没有公开 REST API，配额数据通过抓取 Web Dashboard 获取。需要从浏览器开发者工具中复制 **workspace ID** 和 **auth cookie**：

1. 登录 [OpenCode Go Dashboard](https://opencode.ai/workspace/你的workspace/go)
2. F12 → Network → 刷新页面 → 任意请求的 Cookie 头
3. 将 `workspace_id` 和完整 `Cookie` 值填入 `.env`

如果抓取失败，`opencode_go` 字段返回 null，设备保持上次有效数据，视图切换不受影响。

## 故障转移

| 场景 | 表现 |
|:----|:------|
| DeepSeek API 不可用 | `deepseek` 字段 null，余额区显示横杠 |
| 连续 3 次 HTTP 失败 | 轮询暂停，屏幕保持上次数据；按 GPIO18 立即恢复 |
| OpenCode 抓取失败 | 切换到此视图时显示空白，不崩溃 |
| WiFi 连接超时（20s） | 跳过省电配置，后台自动重连 |

## 省电特性

| 特性 | 状态 | 说明 |
|:----|:----|:------|
| Modem Sleep | ✅ | WiFi 空闲时关闭射频 |
| CPU 变频 | ✅ | 空闲 80 MHz / 峰值 240 MHz |
| Light Sleep | ✅ | 双核空闲时自动进入暂停模式 |
| HTTP 轮询 300s | ✅ | 从 60s 拉长至 300s，减少唤醒次数 |
| 电池采样稀疏化 | ✅ | 每 2 次轮询（≈10min）读一次 ADC，4 点平均 |
| SHTC3 降频 | ✅ | 每 60s 读一次温湿度 |
| 编译优化 -Os | ✅ | sdkconfig.defaults |
| 关闭 AMPDU | ✅ | 小 HTTP 请求无需聚合 |

**预估续航**（2500 mAh 锂电池）：~15–25 mA 平均 → **约 100–160 小时**

## 项目结构

```
ESP32-S3-RLCD-Monitor/
├── bridge/                          # Python FastAPI 守护进程
│   ├── bridge.py                    # 主程序 + 后台刷新缓存
│   ├── schema.py                    # Pydantic 响应模型（含 OpenCodeGo）
│   ├── .env                         # 本地配置（已 gitignore）
│   └── sources/
│       ├── deepseek.py              # DeepSeek 余额 API
│       ├── claude_local.py          # ccusage 集成（提取 DeepSeek Token）
│       ├── claude_limits.py         # 读取 /run/rlcd/claude-limits.json
│       ├── opencode.py              # OpenCode Go 配额抓取
│       └── weather.py               # open-meteo（免 API key）
├── firmware/                        # ESP-IDF v5 + LVGL v9 项目
│   ├── main/
│   │   ├── secrets.h.example        # → 复制为 secrets.h（已 gitignore）
│   │   ├── secrets.h                # WiFi/bridge 配置
│   │   ├── user_config.h            # 引脚定义
│   │   └── main.cpp                 # 入口：初始化显示、UI、WiFi、轮询
│   └── components/
│       ├── net_app/                 # Wi-Fi STA + NTP（CST-8）
│       ├── sensor/                  # SHTC3 温湿度 I2C
│       ├── usage_client/            # HTTP 轮询 + cJSON 解析
│       ├── ui_app/                  # LVGL 仪表盘 + 图标 + 视图切换
│       │   ├── ui_app.cpp           # DeepSeek / OpenCode 双视图渲染
│       │   ├── ui_app.h             # set_tracking_mode(0|1)
│       │   ├── icons.h/.c           # A8 图标（deepseek/opencode/天气/天气状况）
│       │   ├── icon_wifi*.c         # RGB565 图标（wifi/wifi_low/wifi_off）
│       │   ├── icon_bat*.c          # RGB565 电池图标（full/med/low/chg）
│       │   ├── font_amt14.c         # Arial-Bold 14px（含 ¥）
│       │   └── font_bal28.c         # DejaVuSans-Bold 28px（¥ 专用）
│       ├── user_app/                # 按键检测、电池 ADC、时钟、轮询调度
│       │   ├── user_app.cpp         # GPIO18 key_task + usage_poll_task
│       │   └── user_app.h
│       ├── app_bsp/                 # LVGL 平台移植（lvgl_bsp.c/.h）
│       └── port_bsp/                # 显示驱动（display_bsp.cpp/.h）
├── scripts/
│   ├── start_bridge.bat             # Windows 后台启动（短暂黑框）
│   ├── start_bridge.vbs             # 完全隐藏窗口启动（推荐开机启动用）
│   ├── install-bridge-linux.sh      # Linux systemd 安装脚本
│   ├── rlcd-claude-limits.py        # root 定时器：获取 5h/7d 限额
│   ├── rlcd-claude-limits.{service,timer}
│   ├── gen_icons.py                 # 图标生成
│   ├── copy_xiaozhi_icons.py        # 从 xiaozhi-esp32 复制图标
│   └── append_icons.py              # 图标追加工具
├── docs/
│   ├── mockup.png                   # UI 参考原型图
│   ├── mockup.py                    # 原型生成脚本
│   └── ui-mockup.txt                # ASCII 布局参考
└── device_photo.png                 # 实物效果图
```

## 已知问题

- **DeepSeek 数据为空**：若 `deepseek` 字段返回 null，检查 `DEEPSEEK_API_KEY` 有效性及 `sources/deepseek.py` 日志
- **OpenCode 抓取失败**：cookie 过期需重新从浏览器复制，检查 `sources/opencode.py` 日志
- **WiFi 连接阻塞**：DHCP 超时不再锁死设备（20s 超时保护），但会跳过省电配置直到下次重启
- **中文字体**：尝试过 SimHei/DengXian 均显示方框，当前天气用英文描述
- **视图切换**：DeepSeek ↔ OpenCode 切换立刻生效，但新数据需等到下一次 HTTP 响应到达

## 许可证

MIT
