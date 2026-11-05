# ESP32-C3 Bemfa Wake-on-LAN (WOL) Boot Card / 远程开机卡

<p align="center">
  <img src="https://img.shields.io/badge/PlatformIO-Core%206.2+-orange.svg" alt="PlatformIO">
  <img src="https://img.shields.io/badge/Hardware-ESP32--C3%20SuperMini-blue.svg" alt="ESP32-C3">
  <img src="https://img.shields.io/badge/Protocol-MQTT%20TLS%209503-green.svg" alt="MQTT TLS">
  <img src="https://img.shields.io/badge/WOL-Magic%20Packet%20UDP%209-purple.svg" alt="Wake-on-LAN">
  <img src="https://img.shields.io/badge/License-MIT-yellow.svg" alt="License">
</p>

---

<p align="center">
  <a href="#english">English</a> • <a href="#中文说明">中文说明</a>
</p>

---

<a name="english"></a>
## English

A rock-solid, secure, ultra-low-power remote computer boot card firmware based on **ESP32-C3 SuperMini** and **Bemfa Cloud (巴法云 MQTT TLS)**. It bridges mobile phone app commands to local Wake-on-LAN (WOL) Magic Packets over your home network, allowing you to power on your desktop computer from anywhere in the world.

### Architecture

```
Mobile App (Bemfa / XiaoAi / Siri)
         │
         ▼ (MQTT over TLS 9503 - Encrypted)
    Bemfa Cloud
         │
         ▼ (Wi-Fi 2.4GHz)
ESP32-C3 SuperMini (Standalone 5V Charger)
         │
         ▼ (UDP Broadcast on Port 9 - 102-byte Magic Packet)
Target PC Wired Ethernet (Intel I226-V / Realtek PCIe NIC)
```

### Key Features

- ⚡ **Direct One-Tap Wake (点动唤醒)**: Tap "ON" in the mobile app to immediately send standard Wake-on-LAN magic packets.
- 🔄 **2-Second Auto-Reset (点动状态回弹)**: Automatically publishes an `"off"` payload to Bemfa Cloud 2 seconds after waking, resetting the switch back to "OFF" (momentary switch behavior).
- 🚀 **Bemfa Cloud Wireless OTA Updates (远程空中固件升级)**: Built-in dual-partition OTA (`min_spiffs.csv`). Send `"update"` or `"ota"` via MQTT to trigger seamless over-the-air firmware updates from Bemfa Cloud (`bin.bemfa.com`) without touching the USB cable!
- 💡 **GPIO 8 Status LED Indication**:
  - **Solid Blue**: Connected to Wi-Fi and Bemfa Cloud (Ready).
  - **3x Fast Blinks**: Wake command received, Magic Packet emitted.
  - **OFF**: Connecting or Offline.
- ❄️ **Low Heat & Maximum Reliability**:
  - CPU downclocked to **80 MHz** and RF transmit power tuned to **15 dBm** to slash peak current by ~40mA and keep the chip cool 24/7.
  - FreeRTOS idle yielding (`delay(20)`) allows CPU sleep while keeping **`WiFi.setSleep(false)`** active to prevent router DTIM packet loss or disconnects.
- 🔒 **Enterprise-Grade TLS Security**:
  - Communicates over secure MQTT port **9503** with DigiCert Global Root CA verification and SNTP time checks.
  - Prevents replay attacks, stale commands, and unauthorized triggering.
- 🔌 **Non-Invasive Hardware**:
  - No PCIe slot required, no messy jumper wires to the motherboard `PWR_SW` header, zero motherboard warranty issues.

### Hardware Pinout & Specs

| Component | Pin / Specification | Notes |
|---|---|---|
| **MCU** | ESP32-C3 SuperMini (RISC-V 32-bit, 4MB Flash) | Built-in native USB-Serial/JTAG CDC |
| **User LED** | GPIO 8 (Active-Low) | Blue LED: Online status & Wake blinks |
| **Power LED** | Hardwired to 3.3V rail | Red LED: Constant power indicator |
| **Power Supply** | USB Type-C (5V / ~0.15W) | Must be connected to a standalone 5V charger |

---

### Quick Start Guide

#### 1. Configure Credentials
Copy the example config:
```bash
cp include/config.example.h include/config.h
```
Edit `include/config.h` and fill in your details:
```cpp
namespace wol {
static const Settings kSettings = {
  "YOUR_WIFI_SSID",          // 2.4GHz Wi-Fi SSID
  "YOUR_WIFI_PASSWORD",      // Wi-Fi Password
  "YOUR_BEMFA_UID",          // Bemfa Cloud User Key (UID)
  "YOUR_TOPIC",              // Topic name (e.g. PcPower001)
  Protocol::Mqtt,            // Protocol
  Transport::Tls,            // TLS port 9503
  false,                     // Plaintext disabled
  kBemfaRootCa,              // DigiCert Root CA
  "AA:BB:CC:DD:EE:FF",       // Target PC Wired Ethernet MAC
  "",                        // Auto broadcast IP
  9,                         // UDP Port 9
  3,                         // 3 packets per wake
  100,                       // 100ms interval
  15000,                     // 15s cooldown
  3000,                      // 3s guard after reconnect
  10000                      // 10s arm window
};
}
```
*(Note: `include/config.h` is strictly excluded by `.gitignore` to protect your privacy).*

#### 2. Build and Flash
Using [PlatformIO](https://platformio.org/):
```powershell
# Compile firmware
python -m platformio run -e esp32c3_supermini

# Upload firmware (Auto-reset via native USB-CDC)
python -m platformio run -e esp32c3_supermini -t upload
```

#### 3. Target PC Configuration (Crucial!)
1. **Windows Device Manager**:
   - Open Network Adapters -> Your Wired Ethernet (e.g., Intel I226-V / Realtek) -> **Properties** -> **Advanced**.
   - Enable **Wake on Magic Packet** (唤醒魔包).
   - Enable **Wake on Magic Packet from S5** / PME Turn On (S5 魔包唤醒).
   - Disable **Energy Efficient Ethernet** (节能以太网) to prevent link drops during sleep.
2. **Windows Fast Startup**:
   - Disable "Turn on fast startup" in Windows Power Options (prevents Windows 11 hybrid sleep locking the NIC).
3. **Motherboard BIOS**:
   - Reboot into BIOS -> `Advanced` -> `APM Configuration`.
   - Enable **Power On By PCI-E**.
   - Disable **ErP Ready** (ensures standby power remains supplied to the Ethernet chip in S5 shutdown).

---

<a name="中文说明"></a>
## 中文说明

一款基于 **ESP32-C3 SuperMini** 和 **巴法云 (Bemfa Cloud) MQTT TLS** 的高可靠、超低功耗远程电脑开机卡固件。通过手机 App 远程触发，经过公网安全加密链路下发到局域网内的 ESP32，再由 ESP32 通过 UDP 广播向目标台式机发送标准 Wake-on-LAN (WOL) 网络魔术包，实现全球任意地点一键远程开机。

### 核心特性

- ⚡ **点动一键开机**：手机 App（巴法云/小爱同学/Siri快捷指令）点击一次“开”，板子立即向局域网广播发送 3 组魔术包，无需先关后开。
- 🔄 **2秒自动回弹（点动模式）**：唤醒发送后，固件在 2 秒后自动向巴法云发布 `off` 消息，将手机端开关自动复位回“关”，模拟瞬态点动按钮效果。
- 🚀 **支持巴法云 OTA 远程空中升级**：内置双镜像分区表（`min_spiffs.csv`），当向主题发送 `"update"` 或 `"ota"` 消息时，自动从巴法云（`bin.bemfa.com`）下载新版本固件并无感重启，后续升级彻底告别数据线！
- 💡 **GPIO 8 智能指示灯**：
  - **蓝灯常亮**：已成功连接家庭 Wi-Fi 并接入巴法云（就绪）。
  - **蓝灯快闪 3 下**：接收到开机指令，已成功发射 WOL 唤醒包。
  - **蓝灯熄灭**：正在连接网络或处于离线状态。
  - **红灯常亮**：纯硬件电源指示灯，只要供电正常就常亮。
- ❄️ **低温低功耗与极致稳定**：
  - CPU 降频至 **80 MHz**，且 Wi-Fi 功放发射功率优化至 **15 dBm**，峰值电流降低约 40mA，芯片长年 24 小时待机手感仅微温。
  - 采用 FreeRTOS 调度让渡 (`delay(20)`)，同时严格保持 **`WiFi.setSleep(false)`** 禁用 Wi-Fi 射频休眠，彻底避免路由器 DTIM 丢包或断流。
- 🔒 **企业级 TLS 9503 加密**：
  - 固件内嵌 DigiCert 全球根证书，强制校准 SNTP 网络授时，杜绝明文传输与重放拦截。
- 🔌 **无损免接线**：
  - 无需占用主板 PCIe 插槽，无需接跳线到主板开关针脚，不破坏机箱美观与主板原厂质保。

### 硬件与引脚说明

| 组件 | 引脚 / 参数 | 说明 |
|---|---|---|
| **主控芯片** | ESP32-C3 SuperMini (RISC-V 32位单核, 4MB Flash) | 板载原生 USB-Serial/JTAG CDC |
| **状态指示灯** | GPIO 8 (低电平点亮) | 蓝灯：网络状态指示与唤醒快闪 |
| **电源指示灯** | 3.3V 硬件直连 | 红灯：供电状态指示 |
| **供电方式** | Type-C 独立供电 (5V, 待机功耗仅约 0.15W) | **必须插在独立充电头上，切勿插在关机会断电的电脑口** |

---

### 快速部署上手

#### 1. 复制与填写配置文件
复制配置模板：
```bash
cp include/config.example.h include/config.h
```
在 `include/config.h` 中填入您自己的信息：
```cpp
namespace wol {
static const Settings kSettings = {
  "你的2.4G_WIFI名称",          // 必须为 2.4GHz Wi-Fi (不支持5GHz)
  "你的WIFI密码",               // Wi-Fi 密码
  "你的巴法云UID",              // 巴法云控制台私钥 UID
  "PcPower001",                 // 自定义主题名称 (必须在巴法云创建为 MQTT 设备云)
  Protocol::Mqtt,               // MQTT 协议
  Transport::Tls,               // 默认 TLS 9503
  false,                        // 禁止明文降级
  kBemfaRootCa,                 // 内嵌根证书
  "AA:BB:CC:DD:EE:FF",          // 目标电脑有线网卡物理 MAC 地址
  "",                           // 局域网广播 IP (留空则根据子网掩码自动计算)
  9,                            // UDP 端口 9
  3,                            // 发送 3 包
  100,                          // 间隔 100ms
  15000,                        // 15秒防刷冷却
  3000,                         // 3秒重连安全保护
  10000                         // 10秒授权窗口
};
}
```
> [!IMPORTANT]
> `include/config.h` 已被 `.gitignore` 彻底排除，您的 Wi-Fi 密码和私钥 UID 绝对不会被提交或上传到 Git 仓库中。

#### 2. 一键编译与烧录
使用 [PlatformIO](https://platformio.org/)（VS Code 插件或命令行）：
```powershell
# 编译固件
python -m platformio run -e esp32c3_supermini

# 烧录固件到板子 (通过原生 USB 虚拟串口自动握手复位，无需手动按键)
python -m platformio run -e esp32c3_supermini -t upload
```

#### 3. 目标电脑设置（关键步骤）
远程开机依赖目标电脑有线网卡与主板待机监听，请完成以下三步：
1. **Windows 网卡高级设置**（设备管理器 $\to$ 网络适配器 $\to$ 你的有线网卡如 Intel I226-V / Realtek $\to$ 属性 $\to$ 高级）：
   - `唤醒魔包` (Wake on Magic Packet)：**启用**
   - `S5 魔包唤醒` (Wake on Magic Packet from S5)：**启用**
   - `节能以太网` (Energy Efficient Ethernet)：**关闭**（防止关机后 PHY 芯片深度睡眠断开局域网监听）
2. **关闭 Windows 快速启动**：
   - 控制面板 $\to$ 电源选项 $\to$ 选择电源按钮的功能 $\to$ 取消勾选“启用快速启动”（Windows 11 混合关机会导致网卡被锁定在 D3cold 深度睡眠态）。
3. **主板 BIOS 设置**：
   - 重启按 Del 进 BIOS $\to$ `Advanced`（高级） $\to$ `APM Configuration`（高级电源管理）：
   - `Power On By PCI-E`：设为 **Enabled**（开启）
   - `ErP Ready`：设为 **Disabled**（关闭，确保关机后网卡仍有 5VSB 待机供电，网线水晶头微亮）

---

### 指示灯排障速查表

| 红灯 | 蓝灯 | 状态解释 | 处理建议 |
|:---:|:---:|---|---|
| 灭 | 灭 | **彻底无供电** | 检查 Type-C 线缆是否插好，确认充电头通电 |
| 亮 | 灭 | **供电正常，但未连上网络** | 检查 2.4G Wi-Fi 密码是否正确，路由器是否断网 |
| 亮 | 常亮 | **工作正常，云端在线** | 系统就绪，随时可以在手机 App 点击开机 |
| 亮 | 快闪 3 下 | **接收到开机指令** | 正在发射 WOL 封包并自动回弹状态 |

---

### 开源协议
本项目采用 [MIT License](LICENSE) 开源协议，ArduinoMqttClient 与 Arduino 核心分别遵循其对应开源协议。
