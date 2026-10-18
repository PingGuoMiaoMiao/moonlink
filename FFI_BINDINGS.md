# MoonLink FFI 边界

## 规则

MoonBit 仅看到不透明句柄、固定宽度数值、字节数组和受控回调。Windows、FFmpeg、libdatachannel 和 D3D 的结构体、所有权规则、线程对象与异常不得泄漏进 MoonBit 包。

原生桥通过 C ABI 提供采集、编码、传输、输入、查看与性能事件。句柄必须有明确创建、停止和释放路径；回调必须在会话结束后停止；任何输入执行都必须由已授权会话句柄守卫。

## ABI 修订 2

`native/include/moonlink_native_bridge.h` 是当前唯一已实现的 C 头文件。ABI v2 在保留全部 v1 探针的基础上，新增了五类会话级句柄 API 和一套整数句柄 FFI 层。

### v1 探针（不变）

| 符号 | 已验证的边界 |
| --- | --- |
| `moonlink_native_input_allowed` | 仅 `MOONLINK_NATIVE_CONNECTED` 与 `MOONLINK_NATIVE_DEGRADED` 返回允许值。 |
| `moonlink_native_display_count` | 通过 `EnumDisplayMonitors` 统计可见显示器。 |
| `moonlink_native_desktop_duplication_probe` | 创建硬件 D3D11 设备，对第一个 DXGI 输出调用 `DuplicateOutput` 后立即释放。 |
| `moonlink_native_desktop_frame_acquire_probe` | 获取一帧并释放后返回。 |
| `moonlink_native_desktop_texture_probe` | 确认桌面帧暴露 BGRA D3D11 纹理。 |
| `moonlink_native_d3d_viewer_probe` | 创建 1×1 窗口 + D3D11 交换链，清屏并 Present 后销毁。 |
| `moonlink_native_h264_software_available` | 枚举并激活软件 H.264 MFT。 |
| `moonlink_native_h264_software_encode_probe` | 以合成 NV12 样本驱动编码器，确认非空输出。 |
| `moonlink_native_h264_hardware_available` | 枚举并激活硬件 H.264 MFT。 |

### v2 会话句柄 API

每类会话遵循统一生命周期：`create → (use) → stop → destroy`。

| 会话类型 | 创建 | 核心操作 | 停止/释放 |
| --- | --- | --- | --- |
| **Capture** | `moonlink_capture_create` | `moonlink_capture_next_frame`（阻塞等待桌面帧，BGRA→NV12 转换） | `capture_stop`, `capture_destroy` |
| **Encoder** | `moonlink_encoder_create` | `moonlink_encoder_encode`（NV12→H.264，返回编码包数组） | `encoder_stop`, `encoder_destroy` |
| **Transport** | `moonlink_transport_create` | `start`, `send_video`, `send_control`, SDP/candidate 交换 | `transport_stop`, `transport_destroy` |
| **Decoder** | `moonlink_decoder_create` | `moonlink_decoder_decode`（H.264→BGRA） | `decoder_stop`, `decoder_destroy` |
| **Viewer** | `moonlink_viewer_create` | `moonlink_viewer_present`（BGRA→D3D11 窗口），`pump_messages` | `viewer_stop`, `viewer_destroy` |

### 整数句柄 FFI 层

MoonBit 原生 FFI 仅支持值类型（Int、Bool）。`handle_table.c` 实现了 C 侧句柄表，将不透明指针映射为整数槽位索引（0 = 无效）。所有 `*_ffi` 后缀函数接受和返回整数句柄，供 `bridge_ffi.mbt` 直接调用。

`bridge_ffi.mbt` 提供安全封装类型：`CaptureSession`、`EncoderSession`、`TransportSession`、`DecoderSession`、`ViewerSession`，以及 `NativeResult` 枚举。`core/` 不导入 `native/`，会话核心保持不依赖 Windows 类型。

### 源文件结构

| 文件 | 职责 |
| --- | --- |
| `src/bridge.c` | v1 探针 + 采集会话实现 |
| `src/encoder_session.c` | H.264 编码会话（Media Foundation MFT） |
| `src/decoder_session.c` | H.264 解码会话（Media Foundation Decoder MFT） |
| `src/transport_session.c` | WebRTC 传输会话（libdatachannel），仅在 `MOONLINK_WITH_LIBDATACHANNEL=ON` 时编译 |
| `src/viewer_session.c` | D3D11 查看器窗口会话 |
| `src/handle_table.c` | 整数句柄表和 `*_ffi` 包装函数 |
| `src/sessions_internal.h` | 内部结构定义（不对外暴露） |

### 链接要求

MoonBit 不会把包的 `cc-link-flags` 自动传递给导入它的可执行包。每个导入 `native/` 的 Windows 可执行包必须在自身 `moon.pkg` 显式链接 `user32.lib d3d11.lib dxgi.lib dxguid.lib mfplat.lib mf.lib mfuuid.lib ole32.lib`。`cmd/diagnostics` 是这一规则的最小可运行验证。

### 传输依赖

`transport_session.c` 依赖 libdatachannel v0.24.6（提交 d9617a5）。构建时需启用 `-DMOONLINK_WITH_LIBDATACHANNEL=ON` 并提供 OpenSSL 开发根目录。详见 `native/LIBDATACHANNEL_LOCK.md`。未启用时，传输相关 FFI 函数不可用。
