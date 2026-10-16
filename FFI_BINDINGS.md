# MoonLink FFI 边界

## 规则

MoonBit 仅看到不透明句柄、固定宽度数值、字节数组和受控回调。Windows、FFmpeg、libdatachannel 和 D3D 的结构体、所有权规则、线程对象与异常不得泄漏进 MoonBit 包。

原生桥通过 C ABI 提供采集、编码、传输、输入、查看与性能事件。句柄必须有明确创建、停止和释放路径；回调必须在会话结束后停止；任何输入执行都必须由已授权会话句柄守卫。

## 当前 ABI：修订 1

`native/include/moonlink_native_bridge.h` 是当前唯一已实现的 C 头文件。它定义了 `moonlink_native_session_state`、`moonlink_native_bridge_abi_version` 和下列输入闸门与可行性探针；这些探针均为无状态调用，不创建可供 MoonBit 持有的会话资源。

| 符号 | 已验证的边界 |
| --- | --- |
| `moonlink_native_input_allowed` | 仅 `MOONLINK_NATIVE_CONNECTED` 与 `MOONLINK_NATIVE_DEGRADED` 返回允许值；等待确认和结束状态不可执行输入。 |
| `moonlink_native_display_count` | 通过 `EnumDisplayMonitors` 统计可见显示器。 |
| `moonlink_native_desktop_duplication_probe` | 创建硬件 D3D11 设备，对第一个 DXGI 输出调用 `IDXGIOutput1::DuplicateOutput` 后立即释放。它不采集或保存帧。 |
| `moonlink_native_d3d_viewer_probe` | 创建 1×1 原生窗口与 D3D11 交换链，清屏并 `Present` 后销毁。它不接收或呈现 H.264 帧。 |
| `moonlink_native_h264_software_available` | 枚举并激活软件 H.264 Media Foundation 编码 MFT。 |
| `moonlink_native_h264_hardware_available` | 枚举并激活硬件 H.264 Media Foundation 编码 MFT。 |

`native/bridge_ffi.mbt` 是该头文件唯一的 MoonBit 封装：它以 `native/moon.pkg` 的 `native-stub` 构建 `src/bridge.c`，将 C 枚举映射为 `CaptureProbeResult` 和 `ViewerProbeResult`，将 H.264 探测映射为 `Bool`，并把 `core.SessionPhase` 显式转换为 C 会话状态。`core/` 不导入 `native/`，因此会话核心不依赖 Windows 类型或原生实现。

MoonBit 不会把包的 `cc-link-flags` 自动传递给导入它的可执行包。因此每个导入 `native/` 的 Windows 可执行包必须在自身 `moon.pkg` 显式链接 `user32.lib d3d11.lib dxgi.lib dxguid.lib mfplat.lib mf.lib mfuuid.lib ole32.lib`。`cmd/diagnostics` 是这一规则的最小可运行验证。

`native/tests/transport_loopback.c` 直接使用固定版本 libdatachannel 的 C API 验证本机 WebRTC 协商，不属于该头文件的已导出会话 ABI，也尚未由 MoonBit 调用。

该修订尚未暴露采集、编解码、传输、D3D 查看或遥测句柄。六类完整能力的句柄、回调线程和释放语义必须先由各自的可行性测试确定，再扩展此头文件和 MoonBit FFI 声明。

## 计划中的原生实现

- 采集：单显示器 Windows 桌面采集。
- 编码：H.264，优先硬件编码；不可用时 720p 30fps 软件回退。
- 传输：`libdatachannel` 承载局域网 WebRTC 媒体与控制数据。
- 查看：独立 D3D 窗口。
- 输入：仅在被控端明确授权期间执行 Windows 输入操作。

上述实现仍需在具备 MSVC 工具链的环境中分别构建和测试；在通过原生可行性门槛前，它们不是已交付能力。
