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

无 argv 时逐行读取 stdin 执行，空行跳过。

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
  cli.hpp      命令表 CommandSpec + 解析接口
src/kv/
  engine.cpp
  cli.cpp      分词/命令表解析实现
  run.cpp      handler 注册、命令分发、main
```

解析链单向：`tokenize -> parse(查表校验) -> dispatcher -> Engine`。
加命令只改两处：`cli.cpp` 命令表加一行，`run.cpp` 登记对应 handler。
解析层只产出结构化命令，不执行、不 exit，因此坏命令不会杀死会话。

## NEXT

- LRU + 容量上限（当前仅无界 unordered_map）
- AOF 追加日志持久化 + 启动回放
- 单元测试
