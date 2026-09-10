# KV Cache

> cpp 内存键值缓存：有界 LRU 淘汰；本地 CLI / 网络服务 / 客户端库

## BUILD 构建

```sh
cmake -B build
cmake --build build
```

产物：`KVCache-LocalCli`、`KVCache-RemoteCli`、`KVCache-Client`、`KVCache-Server`，C++17，编译 `-Wall -Wextra`。测试框架 Catch2 由 CMake FetchContent 拉取。

## TEST 测试

```sh
cmake --build build
ctest --test-dir build --output-on-failure
```

`kv_tests` 覆盖：LRU 淘汰顺序、get/覆盖刷新 recency、cap=1 边界、erase/clear、随机混合操作对照模型、日志编解码/坏尾、帧编解码、客户端请求与批量。

## USE 用法

本地 CLI（不经网络）：

```sh
./build/KVCache-LocalCli put name "hello world"
./build/KVCache-LocalCli get name
printf 'put a 1\nput b 2\nget a\nlst\n' | ./build/KVCache-LocalCli
```

无 argv 时进入 stdin 会话：先打印一次命令列表，再逐行执行，空行跳过；退出靠 EOF（Ctrl-D）。会话默认容量 64。

网络服务：

```sh
./build/KVCache-Server --port 7379 --capacity 64 --aof kv.aof --fsync always
printf 'put a 1\nget a\n' | ./build/KVCache-RemoteCli --port 7379
./build/KVCache-Client --port 7379 get a
```

`KVCache-RemoteCli` 把 stdin 的命令行解析后发送；非 tty 时整批发送再统一收响应。`KVCache-Client` 是客户端库的示例程序。
客户端可用 `--host` 指定服务端地址。服务端 `--bind` 默认 `127.0.0.1`；绑 `0.0.0.0` 会暴露到网络且无认证，谨慎使用。

## PROTOCOL 协议

```
请求: [1B ver=1][4B body_len][body]
      body = [1B cmdargs][1B cmd_len][cmd][arg_i: 4B len + bytes] × cmdargs
响应: [1B ver=1][4B body_len][body]
      body = 结果文本字节 (长度 0 = 无输出)
```

- 参数原样字节、无转义、空值可表达；`cmdargs` 为命令参数个数（不含命令名）
- `body_len ≤ 1MB`；`cmd_len == 0` 非法；版本不匹配则关闭连接
- 解码读完 `cmdargs` 个参数后忽略尾部多余字节
- 连接 keep-alive；`exit/quit` 触发断开（服务端不执行）；请求与响应按序一一对应
- 命令解析错误仅在服务端日志，客户端收到空响应体

## CLIENT 客户端库

```cpp
#include "kv/client.hpp"

kv::Client client;
client.connect("127.0.0.1", 7379);
std::string payload;
client.request(kv::cmd_put("k", "v"), payload);
client.request(kv::cmd_get("k"), payload);

// 批量(流水线): 发送不等待, 再按条数收
client.send_batch({kv::cmd_put("a", "1"), kv::cmd_put("b", "2")});
std::vector<std::string> out;
client.recv_batch(2, out);
```

链接 `kvcache_net`。`send/recv` 为原语，`send_batch/recv_batch` 支持流水线，`request/request_batch` 为发+收便捷封装。单线程、阻塞式、非线程安全。

## PERSIST 持久化

- 写操作（put/del/clr）由 Engine 钩子先追加到操作日志再改内存（Engine 只持 `Journal*`，文件管理在独立 `Journal` 模块）
- 记录为领域无关的 Op（二进制长度前缀，值可含任意字节）
- 启动时回放历史写序列重建缓存，随后继续追加
- 语义：**写驱动回放**，LRU 只按写序重建，不还原被读取影响的淘汰次序
- 参数：`KVCache-Server --port <n> [--bind <ip>] [--capacity <n>] [--aof <path>] [--fsync always|os]`；本地 CLI 固定 `kv.aof`、`always`
- 尾部不完整记录自动截断；日志会持续增长，压缩（数据文件快照 + 截断）为后续项

## COMMAND 命令

| 命令 | 说明 |
|---|---|
| `put k v` | 新增或覆盖；满员时淘汰最久未用 |
| `get k` | 读取并刷新 recency；存在输出 `k->v`，否则 `'k' not found` |
| `del k` | 删除（立即释放内存） |
| `clr` | 清空 |
| `lst` | 按键名列出全部 kv |

CLI 输入按空白分词，单/双引号内可含空格/逗号，`\` 转义；网络调用参数原样传递。
CLI 解析/参数错误输出到 stderr 并返回退出码 2，其余返回 0。

处理链：本地 `tokenize -> parse -> exec -> Engine`；网络 `tokenize/参数 -> 编码 -> 服务端解码 -> exec -> Engine`。

## LRU 机制

**布局**（数组式侵入链，下标代替指针，`-1` 为空）：

```cpp
struct Slot {
    std::string key;    // 键
    std::string value;  // 值
    int prev;           // 前驱下标
    int next;           // 后继下标
};
```

- `cache: vector<Slot>`：固定容量槽
- `catalog: unordered_map<string,int>`：键 -> 槽下标
- `frstk: vector<int>`：空闲槽栈，删除/淘汰后回收槽
- `head_`：MRU 端；`tail_`：LRU 端

**语义**：`get` 命中 / `put` 已存在键 -> 摘下挂到 `head_`；`put` 新键无空闲槽 -> 淘汰 `tail_` 槽再复用。删除与淘汰都同步删 `catalog`。

## STRUCT 结构

```
include/kv/
  engine.hpp   kv::Engine：有界 LRU 缓存
  cli.hpp      tokenize/parse/CommandSpec + commands() 命令表视图
  regcmd.hpp   kv::exec(…, out)：查表执行入口
  journal.hpp  持久化：操作日志读写与回放
  net.hpp      帧编解码与 socket 原语
  client.hpp   kv::Client 客户端库
  server.hpp   kv::Server 网络服务
src/kv/
  engine.cpp  槽位池 + 侵入链 LRU
  cli.cpp     分词/解析
  regcmd.cpp  命令 handler、命令表、分发
  journal.cpp 日志编解码 + 文件 IO + 坏尾截断
  net.cpp     帧编解码
  client.cpp  Client 实现
  server.cpp  Server 事件循环
example/
  local_cli_main.cpp    本地 CLI
  remote_cli_main.cpp   网络 CLI
  client_main.cpp       客户端库示例
  server_main.cpp       服务端入口
tests/
  test_engine.cpp  LRU 语义
  test_cli.cpp     tokenize/parse/exec
  test_journal.cpp 日志/坏尾/引擎钩子
  test_net.cpp     帧编解码
  test_client.cpp  客户端与假服务端集成
```

命令表是单一真相源：`regcmd.cpp` 的 `CommandSpec kSpecs[]`，由 `kv::commands()` 以范围视图暴露。加/改命令只需改 regcmd.cpp（表加一行 + 定义 handler），cli/net 层零改动。坏命令仅报错不退出进程。

## NEXT

- v0.1 发布：GitHub + CI（gcc/clang + ctest）+ benchmark
- AOF 压缩 → 数据文件快照 + 截断日志（构想）
- 后续评估：TTL、类型系统、RESP 完整协议
