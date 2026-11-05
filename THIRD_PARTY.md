# 第三方依赖与参考

本项目的业务代码、Magic Packet 构造、校验和状态机为独立实现，没有把主要参考工程的源码复制进固件。

| 项目 | 使用方式 | 许可 / 版本 |
|---|---|---|
| [ArduinoMqttClient](https://github.com/arduino-libraries/ArduinoMqttClient/tree/0.1.8) | 固件直接链接未修改的上游版本；本机测试在忽略目录生成 MSVC 语法适配副本，保留原许可头，解析逻辑不变 | LGPL-2.1-or-later，0.1.8 |
| [Arduino-ESP32](https://github.com/espressif/arduino-esp32/tree/2.0.17) | Arduino、WiFi、WiFiUDP、WiFiClientSecure、FreeRTOS | LGPL-2.1 及各组件自身许可，2.0.17 |
| [PlatformIO Espressif32](https://github.com/platformio/platform-espressif32/tree/v6.12.0) | 构建平台，固定版本 | Apache-2.0，6.12.0 |
| [Mozilla / certifi CA bundle](https://github.com/certifi/python-certifi) | 只提取公开 Certum Trusted Root CA 证书作信任锚，不复制 bundle 的程序代码 | 证书是公开信任数据，原 bundle 的 MPL-2.0 见上游 |
| [ESP32-Bemfa-WOL-Tool](https://github.com/yeyt97/ESP32-Bemfa-WOL-Tool) | 只研究 TCP/WOL 思路；未复用代码 | 审阅提交 `1d9cd4df471a179daf2b4f33b8b7fb68df1d1bda` |
| [WakeOnLan](https://github.com/a7md0/WakeOnLan) | 只对比数据包及广播计算；未添加依赖 | MIT，上游保留自身许可 |

项目 MIT 许可只适用于本项目原创部分。若以后分发链接 LGPL 组件的固件，需随分发方式履行相应源码、修改声明和重新链接等许可义务；本次仅同步原创源码、文档及文本验证记录，依赖和固件不入库。
