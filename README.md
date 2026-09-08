# KV Cache

> cpp 内存键值缓存（命令行版），有界容量 + 精确 LRU 淘汰

## BUILD 构建

```sh
cmake -B build
cmake --build build
```

产物：`build/KVCache`，C++17，编译 `-Wall -Wextra`。测试框架 Catch2 由 CMake FetchContent 拉取。

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
会话默认缓存容量 64（run.cpp 的 `kDefaultCapacity`，可改）。

## COMMAND 命令

| 命令 | 说明 |
|---|---|
| `put k v` | 新增或覆盖；满员时淘汰最久未用 |
| `get k` | 读取并刷新 recency；不存在则输出 not found |
| `del k` | 删除（立即释放内存） |
| `clr` | 清空 |
| `lst` | 按键名列出全部 kv |

输入按空白分词，双引号内可含空格/逗号，`\"` 转义。
解析/参数错误输出到 stderr 并返回退出码 2，其余返回 0。

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

**语义**：
`get` 命中 / `put` 已存在键 -> 摘下并挂到 `head_`；  
`put` 新键且无空闲槽 -> 淘汰 `tail_` 槽再复用。删除与淘汰都同步删 `catalog`。

## STRUCT 结构

```
include/kv/
  engine.hpp   kv::Engine：有界 LRU 缓存（cache/catalog/frstk）
  cli.hpp      Command/tokenize 与命令表 kSpace 声明
  regcmd.hpp   kv::exec：查表执行入口
src/kv/
  engine.cpp   基于 槽位池 与 侵入链 的缓存 LRU 实现
  cli.cpp      分词/解析实现
  regcmd.cpp   命令 handler、命令表定义、分发
  run.cpp      main：argv / stdin 输入循环
tests/
  test_engine.cpp  LRU 语义用例
  test_cli.cpp     tokenize/parse/exec 用例
```

解析链单向
`tokenize -> parse(查表校验) -> exec(调用 handler) -> Engine`

命令表控制命令配置：
`kSpace` 引用 handler
增/改命令只需改动 `regcmd.cpp:kv::kSpace`（表加一行 + 定义 handler），无 cli 层改动。
空行跳过，坏命令仅报错不退出进程。

## NEXT

- M3：AOF 追加日志持久化 + 启动回放
- v0.1 后评估：网络 server / TTL / 类型系统
