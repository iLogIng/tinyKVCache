# KV Cache

> cpp 内存键值缓存：有界 LRU 淘汰；命令行与网络（S/C）两种入口

## BUILD 构建

```sh
cmake -B build
cmake --build build
```

产物：`build/KVCache`、`build/KVCacheServer`、`build/KVCacheClient`，C++17，编译 `-Wall -Wextra`。测试框架 Catch2 由 CMake FetchContent 拉取。

## TEST 测试

```sh
cmake --build build
ctest --test-dir build --output-on-failure
```

`kv_tests` 覆盖：LRU 淘汰顺序、get/覆盖刷新 recency、cap=1 边界、erase/clear、随机混合操作对照模型、tokenize/parse/exec 会话（15 用例）。

## USE 用法

单条命令：argv：

```sh
./build/KVCache put name "hello world"
./build/KVCache get name
```

多条命令：stdin 会话：

```sh
printf 'put a 1\nput b 2\nget a\nlst\n' | ./build/KVCache
```

无 argv 时进入 stdin 会话：先打印一次可用命令列表，再逐行执行，空行跳过。
会话默认缓存容量 64。退出靠 EOF（Ctrl-D）；`exit/quit` 已移出命令表（仅网络下作为断开连接的控制词）。

## NET 网络

启动服务（默认 127.0.0.1:7379，容量 64；可用参数改端口与容量）：

```sh
./build/KVCacheServer            # 或 KVCacheServer 17379 8
```

客户端连接（逐行发 stdin 的命令，打印响应）：

```sh
printf 'put a 1\nget a\n' | ./build/KVCacheClient
```

服务进程内单个 Engine 跨连接共享；单线程 select 多连接，无并发竞争。

**行协议**：
- 请求：一行命令（同 CLI 空白/引号语法），`\n` 结尾，单行上限 64KB
- 响应：若干文本行 + 一个空行作为帧尾；错误文本进响应帧
- 连接 keep-alive；`exit/quit` 触发断开（服务端不执行）；EOF 即断开

## COMMAND 命令

| 命令 | 说明 |
|---|---|
| `put k v` | 新增或覆盖；满员时淘汰最久未用 |
| `get k` | 读取并刷新 recency；存在输出 `k->v`，否则 `'k' not found` |
| `del k` | 删除（立即释放内存） |
| `clr` | 清空 |
| `lst` | 按键名列出全部 kv |

输入按空白分词，单/双引号内可含空格/逗号，`\` 转义。
解析/参数错误输出到 stderr 并返回退出码 2，其余返回 0。

处理链：`stdin/socket -> tokenize -> parse(查表校验) -> exec(调 handler) -> Engine`

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
  engine.hpp   kv::Engine：有界 LRU 缓存（cache/catalog/frstk）
  cli.hpp      Command/tokenize/CommandSpec + commands() 命令表视图
  regcmd.hpp   kv::exec(…, out, err)：查表执行入口
  net.hpp      网络小工具：read_line / send_all / recv_frame
src/kv/
  engine.cpp   槽位池 + 侵入链 LRU 实现
  cli.cpp      分词/解析实现
  regcmd.cpp   命令 handler、命令表定义、commands()、分发
  run.cpp      KVCache main：argv / stdin
  server.cpp   KVCacheServer main：select 多连接
  client.cpp   KVCacheClient main：请求-响应
  net.cpp      read_line/send_all/recv_frame 实现
tests/
  test_engine.cpp  LRU 语义用例
  test_cli.cpp     tokenize/parse/exec 用例
```

命令表是单一真相源：`regcmd.cpp` 的 `CommandSpec kSpecs[]`，由 `kv::commands()` 以范围视图暴露（无空名哨兵，全部消费方 range-for）。加/改命令只需改 regcmd.cpp（表加一行 + 定义 handler），cli/net 层零改动。坏命令仅报错不退出进程。

## NEXT

- M3：AOF 追加日志持久化 + 启动回放
- v0.1 发布：GitHub + CI（gcc/clang + ctest）+ benchmark
- 后续评估：TTL、类型系统、RESP 完整协议
