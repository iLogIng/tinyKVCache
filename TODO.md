# TODO

> 开发路线与任务清单
> 主线：M1 -> M2 -> M4(网络 S/C) -> M3 -> v0.1 发布
> 配套：README.md(项目) / docs/ABOUT.md(学习清单) / docs/INTERVIEW.md(面试问答)

## 主线 ROADMAP

| 里程碑 | 内容 | 估时 | 状态 |
|---|---|---|---|
| M1 | CLI 命令框架：tokenize/parse/regcmd/run + Engine | ~8h | 完成 |
| M2 | 缓存语义：exact LRU + 容量 + 测试 | 10-15h | 完成 |
| M3 | AOF 持久化 + 启动回放 | 10-12h | 未开始 |
| M4 | 简单网络 S/C：select 多连接 + 行协议（协议实践平台） | 10-15h | 完成 |
| v0.1 | 发布：GitHub + CI + README benchmark | 4-6h | 未开始 |

## 原则 PRINCIPLES

- 完成度优先于广度；每里程碑到"可验收、可讲解"即止，再进下一步
- 测试先行；编译 `-Wall -Wextra` 零警告
- 小步提交，不跨里程碑留半成品
- 时间盒：M2/M3 各约 2-3 次会话，超时收尾发布
- 不加范围外功能（见"范围外"）

## 现状 STATE

- M1 已完成提交：分层 CLI + regcmd 单一命令表 + help/exit/quit
- 未提交：`.gitignore`(+docs/)、README 措辞微改
- 待定：docs/ 学习文档是否入库（当前 gitignore 中，不入库）

## [x] M2 · LRU + 测试

- [x] Engine 改 exact LRU（数组式侵入链 + 目录），构造带 capacity（已提交 6b052ff）
- [x] 语义：put 超容淘汰、覆盖刷新、get 命中刷新、erase/clear/size/items 适配
- [x] run.cpp 定义默认容量常量（64）
- [x] CMake 拆 `kvcache_core` 静态库（engine/cli/regcmd）；Catch2（FetchContent）
- [x] tests/test_engine.cpp：淘汰顺序、get 刷新、cap=1 边界、erase、随机对照
- [x] tests/test_cli.cpp：tokenize/parse/exec 往返、坏命令返回 2
- [x] `ctest` 全绿（15 用例）；手动：超容 put 后 get 最早键 → not found
- [x] README 同步 LRU 语义与 ctest 用法

## M3 · AOF 持久化

- [ ] 自描述记录格式（长度前缀）定案
- [ ] put/del 追加写；fsync 策略；clr 语义（截断）
- [ ] 启动回放；坏尾截断容错
- [ ] 测试：回放一致性、模拟损坏
- [ ] README 同步 + 提交

> 是否需要添加内存持久化文件转换的功能？

## v0.1 · 发布

- [ ] GitHub 建 repo 并推送（iLogIng）
- [ ] GitHub Actions：gcc + clang，build + ctest 全绿
- [ ] README：benchmark 数据 + "为何精确 LRU 而非 Redis 采样式"对照论述
- [ ] tag v0.1

## 面试关联 INTERVIEW LINK

- 汰换机制 <-> M2 exact LRU；README 写与 Redis 近似 LRU 的取舍
- 单线程原子性 / 高并发网络 ↔ M4 简单 S/C（select 多连接 + 共享单 Engine）
- 类型系统 <-> stretch（value 用 variant）
- 问答见 docs/INTERVIEW.md

## M4 · 网络 S/C（协议实践平台）

> 定位：作品 + 实用接口；CLI 退为调试壳。协议独立可扩展，不做完整 RESP。

**决策**
- 请求 = 一行命令（复用 CLI 空白/引号语法）；响应 = 文本行 + 空行终止
- `exit/quit` 移出命令表，server 层拦截为断开该连接；CLI 退出靠 EOF
- 默认端口 7379、监听 127.0.0.1，argv\[1\] 可改
- Engine 单实例共享；单线程 select 多连接 = 无竞争（面试点）

**阶段 1 · 输出注入重构（2-3h）**
- [x] `CommandSpec.handler` / `kv::exec` 增 `std::ostream& out`(err)
- [x] run.cpp 传 cout/cerr，CLI 行为不变
- [x] exec 测试改 ostringstream 断言输出；ctest 全绿
- [x] 提交

**阶段 2 · S/C（6-9h）**
- [x] `src/kv/server.cpp`：select 多连接 + 行协议（先顺序版跑通再升级）
- [x] `src/kv/client.cpp`：逐行转发 + 响应去空行帧尾
- [x] 边界：半行请求 / 一读多请求 / 半包写 / 断连 / 多连接
- [x] 冒烟联调 + 提交

**阶段 3 · 收尾（1h）**
- [x] 协议文档 + README 网络示例 + 冒烟脚本
- [x] 提交 / 更新 TODO

**明确不做**：RESP、认证、pipeline、多线程、AOF、socket 自动化测试

## 范围外 OUT OF SCOPE（暂不做）

- TTL / 过期
- 类型系统 / variant 值类型（stretch，M2.5 备选）
- RESP / 完整协议、认证、pipeline、socket 自动化测试（M4 已排除）
- 旧 README 的 mmap / 备份 / 六类型

## 下次开工顺序 NEXT SESSION

1. M3 阶段 1：AOF 记录格式定案 + 追加写
2. M3 阶段 2：启动回放 + 坏尾容错
3. M3 阶段 3：回放一致性测试 + README
4. v0.1 发布：GitHub + CI
