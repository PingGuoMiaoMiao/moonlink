# libdatachannel 构建锁定

本项目的局域网 WebRTC 可行性测试使用 libdatachannel `v0.24.6`，主源码提交为 `d9617a53201615ef729e99dc39ad6736982d0346`。源码不提交到 MoonLink 仓库，必须放入 `third_party/paullouisageneau-libdatachannel-d9617a5`。

该提交的 Git 子模块必须使用下列精确提交：

| 路径 | 上游仓库 | 提交 |
| --- | --- | --- |
| `deps/json` | `https://github.com/nlohmann/json.git` | `55f93686c01528224f448c19128836e7df245f72` |
| `deps/libjuice` | `https://github.com/paullouisageneau/libjuice.git` | `b89c792e3612faf2f12cf35bcc56857313a06be3` |
| `deps/libsrtp` | `https://github.com/cisco/libsrtp.git` | `24b3bf8f19b6f5ab4cd2bcceb4f4064efca86fd5` |
| `deps/plog` | `https://github.com/SergiusTheBest/plog.git` | `94899e0b926ac1b0f4750bfbd495167b4a6ae9ef` |
| `deps/usrsctp` | `https://github.com/paullouisageneau/usrsctp.git` | `fec583d54493f879d2ae44a743423bf8a04371ab` |

在已启用 MSVC x64 开发环境且准备了 OpenSSL 开发根目录后，运行：

```powershell
cmake -S native -B native/build -G "Visual Studio 17 2022" -A x64 `
  -DMOONLINK_WITH_LIBDATACHANNEL=ON `
  -DMOONLINK_OPENSSL_ROOT="<OpenSSL 开发根目录>"
cmake --build native/build --config Debug
ctest --test-dir native/build -C Debug --output-on-failure
```

`MOONLINK_OPENSSL_ROOT` 只用于本机构建探针。发布前必须单独固定、审查并打包实际使用的 OpenSSL 版本与许可；不得依赖未声明的系统安装。
