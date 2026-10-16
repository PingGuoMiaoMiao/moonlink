# 第三方来源与许可边界

Git 跟踪内容当前没有第三方源码、二进制或运行时文件。下列项目是经确认的设计参考或后续依赖来源；实际引入前必须在构建清单中固定提交版本、记录来源 URL、随分发物提供所需许可文本和通知。

| 项目 | 用途 | 当前状态 | 许可边界 |
| --- | --- | --- | --- |
| MoonBit Proton | 两端桌面界面 | 尚未引入 | 上游 `LICENSE.md` 为 Apache-2.0；引入后遵守其通知要求。 |
| libdatachannel | Windows WebRTC 媒体与控制通道 | 仅本地忽略目录中的可行性构建；版本 `v0.24.6`，主提交 `d9617a53201615ef729e99dc39ad6736982d0346` | 上游 `LICENSE` 为 MPL-2.0；固定来源、子模块提交和构建方式见 `native/LIBDATACHANNEL_LOCK.md`。尚未随 MoonLink 分发源码、二进制或 OpenSSL。 |
| RustDesk | 模块职责设计参考 | 不引入、不复制代码 | 上游 `LICENCE` 为 AGPL-3.0-or-later；本项目不复制其代码。 |
| DeepSeek Harness | 生命周期、服务接口、事件和可追溯性设计参考 | 不引入、不复制实现 | 仅参考公开文档与行为模型。 |

在 2026-10-16，已通过各上游仓库的默认分支核验来源：Proton `main`、libdatachannel `master`、RustDesk `master`。该记录不是对未来上游版本的许可结论；每次升级都必须重新核验。
