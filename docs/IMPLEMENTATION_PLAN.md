# MoonLink v1 实施计划

本计划以 `ARCHITECTURE.md`、`FFI_BINDINGS.md`、`SECURITY.md` 和 `TEST_PLAN.md` 为准。

1. 建立纯 MoonBit 核心及自动化测试。
2. 建立独立 CMake 原生桥和逐项可行性探针。第一轮探针已通过；下一步是连续采集、实际编解码与会话句柄，不得将对象创建、轨道协商视作帧通路交付。
3. 接入 Proton 双端界面和原生查看窗口。
4. 执行两机局域网性能验收。

RustDesk 仅用于学习模块职责划分，不复制 AGPL-3.0 代码。DeepSeek Harness 仅用于学习静态能力注册、事件和可追溯性模型，不复制实现。第三方依赖的版本、许可和来源必须在原生桥构建清单中锁定并记录。
