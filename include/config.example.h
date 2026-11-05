#pragma once
#include "WolCore.h"
#include "BemfaRootCa.h"

// ==============================================================================
// 配置模板 / Configuration Template
// 复制此文件为 include/config.h 并填入您的真实信息（config.h 已被 .gitignore 忽略）
// Copy this file to include/config.h and fill in your credentials (config.h is gitignored)
// ==============================================================================

namespace wol {
static const Settings kSettings = {
  // 1. Wi-Fi 设置 / 2.4GHz Wi-Fi Credentials
  "YOUR_WIFI_SSID",                 // 2.4GHz Wi-Fi 名称 (ESP32 不支持 5GHz) / 2.4GHz SSID
  "YOUR_WIFI_PASSWORD",             // Wi-Fi 密码 / Wi-Fi Password

  // 2. 巴法云凭据 / Bemfa Cloud Credentials
  "YOUR_BEMFA_UID",                 // 巴法云控制台私钥 UID / User Key UID
  "YOUR_TOPIC",                     // 主题名称 (如 PcPower001, 必须为MQTT设备云主题) / Topic name

  // 3. 通信与加密 / Protocol & Transport
  Protocol::Mqtt,                   // 巴法云 MQTT 协议 / Bemfa MQTT protocol
  Transport::Tls,                   // 默认启用 TLS (端口 9503) 校验根证书 / TLS port 9503
  false,                            // 是否允许明文 (端口 9501, 生产环境强烈建议 false)
  kBemfaRootCa,                     // 内嵌 DigiCert 全球根证书 / Embedded Root CA

  // 4. 目标主机唤醒配置 / Target Computer Wake-on-LAN Settings
  "AA:BB:CC:DD:EE:FF",              // 目标电脑有线网卡 MAC 地址 / Target Wired Ethernet MAC
  "",                               // 广播地址 (留空则根据本地 IP 和子网掩码自动计算) / Broadcast IP
  9,                                // WOL UDP 目标端口 (默认 9) / UDP Port

  // 5. 唤醒与安全策略 / Wake & Security Policy
  3,                                // 每次触发发送的魔术包数量 (1~10) / Magic Packets count
  100,                              // 包发送间隔毫秒 (10~1000ms) / Packet interval in ms
  15000,                            // 唤醒冷却时间毫秒 (防止短时间重复轰炸) / Cooldown ms
  3000,                             // 连接云端后的保护等待时间毫秒 / Reconnect guard ms
  10000                             // 授权窗口毫秒 / Arm window ms
};
}
