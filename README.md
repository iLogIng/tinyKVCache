# KV Cache

> cpp 内存键值缓存（命令行版）

## BUILD 构建

```sh
cmake -B build
cmake --build build
```

产物：`build/KVCache`，C++17，编译带 `-Wall -Wextra`。

## USE 用法

单条命令: argv：

```sh
./build/KVCache put name "hello world"
./build/KVCache get name
```

多条命令: stdin 会话：

```sh
printf 'put a 1\nput b 2\nget a\nlst\n' | ./build/KVCache
```

无 argv 时进入 stdin 会话：先打印一次可用命令列表，再逐行执行，空行跳过。

## COMMAND 命令

| 命令 | 说明 |
|---|---|
| `put k v` | 新增或覆盖 |
| `get k` | 读取；不存在则输出 not found |
| `del k` | 删除 |
| `clr` | 清空 |
| `lst` | 按键名列出全部 kv |

输入按空白分词，双引号内可含空格/逗号，`\"` 转义。
解析/参数错误输出到 stderr 并返回退出码 2，其余返回 0。

## STRUCT 结构

```
include/kv/
  engine.hpp   kv::Engine：内存 kv（unordered_map）
  cli.hpp      Command/tokenize 与命令表 CommandSpec 声明
  regcmd.hpp   kv::exec：查表执行入口
src/kv/
  engine.cpp
  cli.cpp      分词/解析实现（不依赖 Engine）
  regcmd.cpp   命令 handler + 命令表定义 + 分发
  run.cpp      main：argv / stdin 输入循环
```

解析链单向：`tokenize -> parse(校验) -> exec(查表调 handler) -> Engine`。
命令表是单一真相源：`CommandSpec` 自带 handler。加/改命令只需改 regcmd.cpp（表加一行 + 定义 handler），cli 层零改动。空行跳过，坏命令仅报错不退出进程。
解析链单向：`tokenize -> parse(查表校验) -> dispatcher -> Engine`。
加命令只改两处：`cli.cpp` 命令表加一行，`run.cpp` 登记对应 handler。
解析层只产出结构化命令，不执行、不 exit，因此坏命令不会杀死会话。

## NEXT

- LRU + 容量上限（当前仅无界 unordered_map）
- AOF 追加日志持久化 + 启动回放
- 单元测试
