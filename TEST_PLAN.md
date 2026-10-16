# MoonLink 测试计划

纯 MoonBit 测试覆盖会话状态、一次性验证码的错误/过期/单次消费、授权前置条件、等比留边坐标映射、过期鼠标事件、按键与鼠标按键的可靠有序类别、能力替换、降级状态和技术轨迹的类型级脱敏。

原生桥当前测试覆盖 ABI 修订值、显示器枚举、未授权输入拒绝、桌面复制对象创建/释放、一次 Desktop Duplication 帧获取及释放、1×1 D3D11 清屏并呈现，以及软件与硬件 H.264 Media Foundation 编码 MFT 的枚举和激活。它没有连续采集桌面帧、编码 720p 帧或解码/呈现远程 H.264 帧。

MoonBit 原生包测试会编译同一份 `native/src/bridge.c`，验证值型 FFI 可调用、C 探针结果被映射为 MoonBit 枚举、H.264 能力被映射为布尔值，以及 `core.SessionPhase` 到 C 输入闸门的转换。该测试不使 `core/` 依赖 `native/`。

`moon run cmd/diagnostics --target native` 验证独立 MoonBit 可执行包能链接原生桥并调用 ABI、显示器、一次帧获取、查看与 H.264 探针。它只输出诊断状态；一帧在本机内存中被立即释放，不启动共享、连续帧通路、网络传输或输入执行。

固定版本 libdatachannel 的本机环回测试创建两个 `PeerConnection`、可靠控制数据通道 `moonlink-control` 和一个 H.264 `sendonly` 轨道；测试验证两端控制通道打开、接收端收到视频轨道协商，并收到 12 字节 `control-ping`。该测试不发送 H.264 帧，因此不是媒体编码、解码或 60fps 性能结论。

端到端验收仅在两台实体 Windows 10/11 x64、千兆有线网络和硬件 H.264 可用时评估。拥塞时优先降低码率和帧率，保留控制响应。
