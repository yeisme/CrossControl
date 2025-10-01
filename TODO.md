## CrossControl - 详细开发与交付 TODO 清单

目标：将 CrossControl 定位为一款以“设备大规模管理”为核心的跨平台管理应用，便于快速接入、管理与运维大量嵌入式设备（如 STM32F4xx 系列）。系统仍保留门禁与多媒体能力，但首要目标变为：设备注册/分组/授权/批量运维/远程诊断与可扩展的事件总线。

说明与假设：

- 假设大量嵌入式设备通过多种传输通道（串口/UDP/TCP/MQTT/CoAP）上报遥测与抓拍；管理端需支持批量下发命令、OTA、配置下发与设备状态监控。
- 项目采用 CMake 构建、Qt Widgets 做桌面管理 UI，后端核心模块可独立运行（支持单机或分布式部署）。MQTT/HTTP/REST 将作为首选的接入/消息总线方案。

使用约定：在本文件中，任务均以可验收的小故事（story）+ 验收标准列出，优先级用 P0/P1/P2 表示（P0 为最紧急）。每项任务建议拆成 1-5 天开发任务（视团队规模）。

## 快速合约（Contract）

- 输入：来自设备（串口/UDP/TCP/RTSP）的事件或媒体流；来自 UI 的控制命令。
- 输出：本地动作（门锁开/关、提示音、录像）、上报到后端的事件（JSON），以及 UI 状态更新。
- 数据形状（事件示例）：
  {
  "event_id": "uuid",
  "timestamp": 1690000000,
  "device_id": "stm32-01",
  "type": "face_capture",
  "payload": { "image_ref": "file://...", "confidence": 0.92 }
  }
- 错误模式：网络超时、设备断连、流解码失败、推理超时。

## 里程碑建议（以设备管理为核心）

- M1 (2 周)：Device Registry + Device Gateway（串口/UDP/TCP/MQTT 基础）+ 最小管理 UI（设备列表、在线状态）
- M2 (4 周)：批量操作（分组、批量下发命令）、Provisioning 与 LWT 健康上报、基本 OTA 流程示例
- M3 (4 周)：策略/规则引擎（Policy Engine）与告警/审计管道、MQTT Adapter 完整实现（高可用部署方案）
- M4 (4 周)：多租户/角色权限（RBAC）、规模化性能测试（模拟数千设备）、运维监控/报警（Prometheus + Grafana）

## 优先级 P0 - 最小可交付核心（设备管理优先）

1. Device Registry & Provisioning

   - [ ] 设备注册/元数据存储（device_id / hw_info / firmware_version / owner / group） — 验收：UI/REST API 能新增/修改/查询设备并导出 CSV/JSONND
   - [ ] 设备预配/入网流程（支持自动注册 token、设备证书签发/绑定）— 验收：新设备在首次上线时能完成自动注册并出现在 Registry
   - [ ] 证书/凭据管理（支持轮换） — 验收：支持推送新的凭据并让设备重连
   - 优先级：P0

2. Device Gateway / Ingest（高并发可扩展）

   - [ ] 支持 MQTT/UDP/TCP/串口 上报，多协议可并行接入 — 验收：同机至少模拟 100 台设备并稳定接入
   - [ ] Telemetry 聚合与时间序列写入（本地/轻量 DB 或转发到 TSDB）— 验收：存储并能按设备/时间区间查询
   - 优先级：P0

3. Bulk Operations & Commanding

   - [ ] 批量下发命令（按 group / filter）与回执跟踪（request_id -> status）— 验收：批量操作成功率可统计并可回滚
   - [ ] 命令幂等与去重机制（保证重复消息不会重复执行关键操作，如 unlock）— 验收：重复消息被正确忽略
   - 优先级：P0

4. Management UI（设备管理视角）

   - [ ] 设备列表与分组管理（过滤、搜索、批量操作） — 验收：能对分组执行批量命令
   - [ ] 设备详情页（运行指标、日志、最近事件、固件版本、在线历史） — 验收：可查看并导出设备诊断信息
   - 优先级：P0

5. Security & Audit（基础）

   - [ ] 强制设备认证（证书/JWT/API token），并记录审计（谁做了什么、何时）— 验收：审计日志可查询且只对授权用户返回
   - 优先级：P0

6. MQTT Adapter（作为首选接入层）

   - [ ] 实现可插拔的 `mqtt_adapter/`（见下方 MQTT 集成章节），支持高并发订阅与 ACL 集成 — 验收：Core 能通过 adapter 与 broker 正常交互并处理 events
   - 优先级：P0

7. Device Simulator / Test Harness

   - [ ] 一个可配置的设备模拟器（支持并发模拟、多协议）用于 E2E 测试与性能测试 — 验收：能模拟 500+ 设备并生成 telemetry
   - 优先级：P0

## 优先级 P1 - 核心增强（增强用户体验与健壮性）

1. AudioVideo 模块

   - [ ] RTSP/HTTP/本地摄像头接入（支持多路） — 验收：可同时打开至少 4 路流并显示
   - [ ] 播放控制 API（play/pause/stop/seek/record） + 状态回调 — 验收：UI 能操作并收到状态事件
   - [ ] 录像与关键帧抓取（按事件/定时） — 验收：录像文件可回放且含关键帧缩略图
   - 优先级：P1

2. Storage Adapter

   - [ ] 持久化策略/配置（JSON/YAML） — 验收：重启后配置保持
   - [ ] 审计与运行日志分离（带轮转） — 验收：大于设定大小后轮转
   - 优先级：P1

3. Plugin 管理
   - [ ] 插件接口定义（C++ 抽象类 + 插件元数据） — 验收：能加载/卸载单一插件并和 Core 通信
   - 优先级：P1

## 优先级 P2 - 可选与长期（扩展性、性能优化）

- Web UI（WebSocket 实时推送）
- 硬件加速（GPU 推理 / 硬解码）
- 多实例部署/分布式策略引擎

### MQTT 集成

说明：MQTT 适合设备事件上报与下发控制，能在不可靠网络中利用 QoS 与 LWT 提高可靠性。推荐作为 Device Gateway 与 Core 之间的可选传输层，或用于跨机房的事件总线。

目标：提供一个可插拔的 MQTT Adapter，负责与 broker 通信、topic→ 内部 event 的转换、重试/缓冲、以及安全配置（TLS/ACL）。

任务清单：

- [ ] 选择 broker（开发：Mosquitto / 生产：EMQX）并编写部署与监控说明 — 验收：能在本地启动并接收测试消息
- [ ] 在 `src/` 添加 `mqtt_adapter/` 模块（C++/Qt 兼容接口），实现 publish/subscribe、LWT、retain、QoS 策略 — 验收：Core 能通过 Adapter 订阅 devices/+/events 并收到消息
- [ ] 主题与消息规范文档化（见下方主题示例与 payload 模板） — 验收：开发者可根据模板互通测试客户端
- [ ] 安全配置：TLS、客户端证书或 token、broker ACL 示例 — 验收：非授权 client 无法 publish/subscribe 到受限 topic
- [ ] 测试：单元（Adapter 行为）、集成（本地 mosquitto + Core + Device 模拟器）、性能（1000 events/min）— 验收：延迟与丢包率符合预期
- [ ] UI/Control 路径测试：命令下发（commands/{device_id}/control） -> 设备 ACK（core/ack/{request_id}）— 验收：命令往返 latency 可测且可记录

主题设计（建议）：

- devices/{device_id}/telemetry （周期性心跳或传感器数据，retained 可选）
- devices/{device_id}/events （抓拍/告警事件，重要事件使用 QoS1）
- devices/{device_id}/images （图片元信息或缩略图 base64，推荐只用于小图）
- commands/{device_id}/control （从 Core/UI 下发到设备的控制命令）
- core/ack/{request_id} （命令应答或处理回执）
- status/{device_id} （在线/离线，使用 LWT + retained）

消息设计要点：

- 使用 JSON payload，包含 request_id/event_id、timestamp、device_id、type、payload（尽量传引用而非大二进制）
- 对关键事件使用 QoS1（确保至少一次投递）；对低重要性 telemetry 使用 QoS0/QoS1
- 使用 LWT 标记离线：当设备异常断开时 broker 自动 publish 预定义 offline 消息到 status/{device_id}（retained）
- 对于大图片：建议设备上传到对象存储（S3/MinIO/HTTP endpoint），MQTT 仅发送 image_ref；如必须使用 MQTT 传图，需实现分片合并协议与幂等机制

示例 payload：
事件：
{
"event_id":"uuid-v4",
"timestamp":1690000000,
"device_id":"stm32-01",
"type":"face_capture",
"payload": {
"image_ref":"http://gateway.local/storage/2025/09/23/abc.jpg",
"thumbnail_b64":"...",
"confidence":0.92
}
}

命令：
{
"request_id":"uuid",
"command":"unlock",
"params": { "duration_ms":5000 },
"reply_topic":"core/ack/uuid"
}

验收标准（MQTT 相关）

- Core 能订阅 devices/+/events 并在 P95 < 300ms 内对 95% 的事件完成内部路由（本地网络测试）
- 使用 LWT 后，设备异常断开 5s 内在 status/{device_id} 被标记为 offline（retained）
- 重要事件使用 QoS1 且 Core 能在 reply_topic 上返回 ACK（用于幂等与确认链路）
- 图片采用引用方式，Core/Ui 能下载并关联到事件

安全与运维（MQTT）

- 强制 TLS，推荐使用客户端证书或短期 token（JWT）
- Broker 使用 ACL 控制 topic publish/subscribe 权限
- 对重要操作（unlock、erase）增加二次确认或策略签名
- Broker 监控：prometheus 导出器 + 日志轮转

## 每个模块的接口契约（示例）

- Device Gateway -> Core Event (C++ struct / JSON):
  {
  "event_id": "uuid",
  "device_id": "stm32-01",
  "type": "sensor" | "face" | "door_button",
  "payload": { ... }
  }
- HumanRecognition 返回：
  {
  "request_id": "uuid",
  "timestamp": 1690000000,
  "results": [{ "subject_id":"alice","confidence":0.92 }],
  "meta": { "processing_ms": 120 }
  }
- AudioVideo 控制 API:
  enum Command { Play, Pause, Stop, Record, Snapshot }
  struct AVRequest { string stream_uri; Command cmd; map<string,string> args }

## 验收标准（Quality Gates）

- 构建：通过 CMake configure + build（MSVC/Clang/GCC）
- Lint/Typecheck：代码风格与警告清理，关键模块加静态分析（clang-tidy）
- 单元测试：每个模块至少一个单元测试（GoogleTest / QTest）
- 集成 smoke 测试：Device Gateway -> Core -> HumanRecognition -> UI 显示（模拟器驱动）
- 性能：门禁响应（从设备事件到门锁动作）P95 < 300ms（本地网络、同机部署）

## 测试策略（建议）

- 单元测试：逻辑分支、边界值（空输入、大批量事件）、错误恢复路径
- 集成测试：使用设备模拟器（串口模拟或 UDP/TCP 模拟），保证 End-to-End 流程
- 性能测试：模拟高并发事件（1000 events/min）并测量延迟与内存/CPU 占用
- 回归测试：关键场景录像回放与识别结果重复性检查

## 安全与运维

- TLS/HTTPS for remote management endpoints
- API 鉴权（JWT/互信证书）与 RBAC 基础
- 敏感数据加密存储（如录像、证据）
- 审计链：每次策略决策写入审计日志并支持导出（CSV/JSON）
- 健康上报：prometheus metrics 或简易 HTTP /metrics

## DevOps / CI 建议

- CI：GitHub Actions / Azure Pipelines，步骤：format -> build -> unit tests -> artifacts
- 发布包：使用 CPack 生成跨平台安装包（Windows installer / macOS dmg / Linux tar）
- 快速本地运行：提供 CMake Presets（项目已有），并在 README 指示如何运行 M1/M2

## 风险与应对（Edge cases）

- 设备离线/网络分区：设计事件缓存并重试上报
- 视频流长时间丢帧：自动重连策略并上报告警
- 识别误报/拒识：提供人工复核流程与二次判定链路

## 小型交付建议（Proactive extras）

- 初始提交包含：最小 Device Gateway、Core Controller、简易 HumanRecognition adapter（调用远端 API）和 Qt 最小 UI（事件列表）。
- 为降低上线风险，先以本地单机模式实现（所有服务进程同机），后续拆分为微服务/远端识别。
