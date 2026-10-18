# MoonLink 路线图

## 阶段 0：文档与纯核心

完成架构、FFI、安全和测试文档；实现并测试状态机、授权、坐标映射、能力注册、控制事件类别与无内容技术轨迹。当前已完成此阶段的纯核心与最小 C ABI 输入闸门；完整 Windows 能力尚未交付。

## 阶段 1：原生可行性

已在一台 Windows x64 主机完成以下探针：显示器枚举；DXGI Desktop Duplication 对象创建与释放；一次桌面帧获取及释放；1×1 D3D11 交换链清屏和呈现；软件 H.264 Media Foundation 编码器以合成 1280×720 NV12 样本产出非空编码样本；软件及硬件 H.264 编码 MFT 的枚举和激活；libdatachannel 的本机控制通道与 H.264 轨道协商；输入状态闸门。值型原生探针已通过 `native/bridge_ffi.mbt` 接入 MoonBit，且会话核心保持不依赖原生包。`cmd/diagnostics` 已验证跨包链接和只读探针调用。

本阶段尚未完成：连续桌面帧采集、将桌面帧交给编码器、实际 H.264 包发送、解码、远程 D3D 呈现、码率/帧率自适应和两机性能测量。因此尚不进入端到端性能承诺。

## 阶段 2：端到端媒体管线（进行中）

已完成原生会话级 ABI v2 扩展，实现了五类会话句柄的完整生命周期：

- **采集会话**：连续 DXGI Desktop Duplication + BGRA→NV12 转换，通过整数句柄 FFI 暴露给 MoonBit。
- **编码会话**：Media Foundation H.264 MFT 编码器，支持硬件优先 / 软件回退，ProcessInput/ProcessOutput 循环。
- **传输会话**：libdatachannel WebRTC PeerConnection + H.264 sendonly 轨道 + 控制数据通道，支持 SDP/candidate 交换。
- **解码会话**：Media Foundation H.264 解码器 MFT，输出 BGRA 帧供查看器使用。
- **查看器会话**：独立 D3D11 窗口 + 交换链，接收 BGRA 帧并 Present。

整数句柄表（`handle_table.c`）解决了 MoonBit FFI 仅支持值类型的限制。所有会话结构定义集中在 `sessions_internal.h`，保持 `core/` 不依赖 `native/` 的架构约束。

### 阶段 2 已完成的工作

- ✅ **管线编排状态机**：`core/pipeline.mbt` 实现 Uninitialized→Ready→Connecting→Streaming→Stopped 生命周期。
- ✅ **输入执行器**：`handle_table.c` 中实现 Windows SendInput（鼠标移动/按键、键盘），通过整数句柄 FFI 暴露。
- ✅ **TCP 信令**：`signaling.c` 实现长度前缀 TCP 协议，支持 listen/accept/connect/send/recv，用于局域网 SDP 交换。
- ✅ **被控端应用骨架**：`cmd/host/` 串联 capture→encode→transport 管线。
- ✅ **控制端应用骨架**：`cmd/controller/` 串联 transport→decode→viewer 管线。
- ✅ **E2E 测试**：`cmd/e2e_test/` 验证 capture→encode 管线、viewer 窗口、TCP 信令创建/销毁。
- ✅ **编码包长度**：编码器输出改为读取 `IMFMediaBuffer_GetCurrentLength`，不再把 MFT 输出缓冲区容量当作编码长度（此前每帧错误上报约 1.7MB，实际为 1–50KB）。
- ✅ **视频改走 H.264 轨道**：编码帧通过 `rtcSendMessage` 送到协商出的 sendonly 轨道，由 libdatachannel 的 H.264 打包器分片为 RTP；不再用 1KB 分片挤占控制数据通道（此前会导致 SCTP 背压失败）。
- ✅ **RTP 解包**：`h264_depacketizer.c` 把收到的 RTP 还原为 Annex-B 访问单元（单 NAL / STAP-A / FU-A、CSRC/扩展/填充），因为 libdatachannel 的 C API 只提供打包器、不提供解包器。
- ✅ **解码器启动契约**：补发 `MFT_MESSAGE_NOTIFY_START_OF_STREAM`、输入样本时间戳、SPS/PPS 序列头，并优先使用系统软件 H.264 解码器。

### 阶段 2 剩余工作

- **解码输出**：本机唯一的 H.264 解码器 MFT 是 DXVA 变体，其输出流声明 `MFT_OUTPUT_STREAM_PROVIDES_SAMPLES`，未挂接 D3D11 设备管理器时只接收输入、不产出样本。`moonlink_decoder_roundtrip` 目前在本机解码 0 帧。下一步接入 `MFCreateDXGIDeviceManager` + `MFT_MESSAGE_SET_D3D_MANAGER`，把解码输出（GPU 纹理）复制到 staging 纹理后映射为 BGRA，或直接交给查看器呈现。
- 通过 TCP 信令完成 SDP 交换后，在真实两机上验证 720p 视频流。
- 接入 Proton 双端界面（地址、验证码、授权、状态显示）。
- 控制端鼠标键盘事件捕获、序列化、通过 WebRTC 控制通道发送、被控端执行。
- 码率/帧率自适应和拥塞控制。
- 本地技术轨迹记录。


## 阶段 3：两机验收

在两台实体 Windows 10/11 x64、千兆有线网络、硬件 H.264 可用的条件下验证 720p 60fps、视频 P95 不高于 150ms、控制 P95 不高于 80ms。
