# MoonLink FFI 边界

## 规则

MoonBit 仅看到不透明句柄、固定宽度数值、字节数组和受控回调。Windows、FFmpeg、libdatachannel 和 D3D 的结构体、所有权规则、线程对象与异常不得泄漏进 MoonBit 包。

原生桥通过 C ABI 提供采集、编码、传输、输入、查看与性能事件。句柄必须有明确创建、停止和释放路径；回调必须在会话结束后停止；任何输入执行都必须由已授权会话句柄守卫。

## 当前 ABI：修订 1

`native/include/moonlink_native_bridge.h` 是当前唯一已实现的 C 头文件。它定义了 `moonlink_native_session_state`、`moonlink_native_bridge_abi_version` 和 `moonlink_native_input_allowed`。后者只对 `MOONLINK_NATIVE_CONNECTED` 与 `MOONLINK_NATIVE_DEGRADED` 返回允许值；等待确认和结束状态均不可执行输入。

该修订尚未暴露采集、编解码、传输、D3D 查看或遥测句柄。六类完整能力的句柄、回调线程和释放语义必须先由各自的可行性测试确定，再扩展此头文件和 MoonBit FFI 声明。

## 计划中的原生实现

- 采集：单显示器 Windows 桌面采集。
- 编码：H.264，优先硬件编码；不可用时 720p 30fps 软件回退。
- 传输：`libdatachannel` 承载局域网 WebRTC 媒体与控制数据。
- 查看：独立 D3D 窗口。
- 输入：仅在被控端明确授权期间执行 Windows 输入操作。

上述实现仍需在具备 MSVC 工具链的环境中分别构建和测试；在通过原生可行性门槛前，它们不是已交付能力。
