# tinyKVCache

> 这是一个基于 C++17 的零依赖简单内存键值缓存项目
> 包含：
> - 有界的 LRU 缓存淘汰机制
> - local cli | remote cli | server | client 包装
> - 网络服务/客户端封装
> - 服务端 AOF 缓存文件回放重建

## START 快速开始

### BUILD 构建

```sh
cmake -B build
cmake --build build
```

产物：
- `KVCache-LocalCli`    本地cli
- `KVCache-RemoteCli`   远程cli
- `KVCache-Client`      客户端
- `KVCache-Server`      服务端
- `kv_tests`            测试

### TEST 测试

```sh
ctest --test-dir build --output-on-failure
```

### USE 使用

#### LOCAL CLI 本地 CLI

```sh
./build/KVCache-LocalCli put name "hello world"
./build/KVCache-LocalCli get name
printf 'put a 1\nput b 2\nget a\nlst\n' | ./build/KVCache-LocalCli
```

- 参数：`KVCache-LocalCli <command> [args...]`
- 无 argv 时进入 stdin 会话：先打印命令列表，逐行执行，空行跳过；C-D EOF 退出
- 本地固定 `kv.aof`、fsync `always`；会话默认容量 64

#### REMOTE CLI 远程 CLI

```sh
printf 'put a 1\nget a\n' | ./build/KVCache-RemoteCli --port 7379
```

- 参数：`KVCache-RemoteCli [--host <ip>] [--port <n>]`
- 命令从 stdin 逐行读取；非 tty 时整批发送再统一收响应

#### CLIENT 客户端库（示例程序）

```sh
./build/KVCache-Client --port 7379 get a
```

- 参数：`KVCache-Client [--host <ip>] [--port <n>] <command> [args...]`

库接口（`client.hpp`）：

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

- `connect/close/is_open`；`send/recv` 原语；`send_batch/recv_batch` 流水线；`request/request_batch` 发+收
- 命令工厂：`cmd_put/cmd_get/cmd_del/cmd_clr/cmd_lst/cmd_help`
- 链接 `kvcache_net`；单线程、阻塞式、非线程安全

#### SERVER 服务端（可执行 + 库）

```sh
./build/KVCache-Server --port 7379 --capacity 64 --aof kv.aof --fsync always
```

- 参数：`KVCache-Server --port <n> [--bind <ip>] [--capacity <n>] [--aof <path>] [--fsync always|group|os] [--fsync-interval <ms>]`
  - `--bind` 默认 `127.0.0.1`；绑 `0.0.0.0` 会暴露到网络且无认证，谨慎使用
  - `--capacity` 默认 64；`--aof` 默认 `kv.aof`
  - `--fsync-interval` 仅 `group` 模式生效，默认 1ms

库接口（`server.hpp`）：

```cpp
struct ServerConfig { /* bind / port / capacity / aof_path / fsync / fsync_interval_ms */ };
class Server {
    explicit Server(ServerConfig config);
    bool run();
};
```

## COMMAND 命令

| 命令 | 说明 |
|---|---|
| `put k v` | 新增或覆盖；满员时淘汰最久未用 |
| `get k` | 读取并刷新 recency；存在输出 `k->v`，否则 `'k' not found` |
| `del k` | 删除（立即释放内存） |
| `clr` | 清空 |
| `lst` | 按键名列出全部 kv |

CLI 输入按空白分词，单/双引号内可含空格/逗号，`\` 转义；网络调用参数原样传递。
命令表是单一真相源：`regcmd.cpp` 的 `CommandSpec[]` 由 `commands()` 暴露，加/改命令只改命令表 + handler。

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

## PROTOCOL 协议

```
请求帧: [1B ver=1][4B body_len][body]
        body = [1B cmdargs][1B cmd_len][cmd][arg_i: 4B len + bytes] * cmdargs

响应帧: [1B ver=1][4B body_len][body]
        body = 结果文本字节 (长度 0 = 无输出)
```

- 参数原样字节、无转义、空值可表达；`cmdargs` 为命令参数个数（不含命令名）
- `body_len ≤ 1MB`；`cmd_len == 0` 非法；版本不匹配则关闭连接

## PERSIST 持久化

> 由 `kv::Journal` 管理

- **模块边界**：`Engine` 只持有 `Journal*`；文件 IO、记录编解码、回放都在独立 `Journal` 模块
- **记录格式**：`[1B op][4B klen][key][4B vlen][value]`，长度前缀，值可含任意字节
- **写路径**：`put/del/clr` 先 `append` 再改内存；append 失败则不修改内存，保证日志与内存一致
- **回放**：启动时逐条读取并回调重建缓存；写驱动语义，LRU 只按写序重建
- **坏尾**：末尾不完整记录自动截断恢复
- **fsync 策略**：
  - `always` 每次追加立即落盘
  - `group` 合并一批写：`dirty` 标记 + `last_fsync` 时间 + 写计数，超过 `--fsync-interval` 统一 fsync，响应按 `pending_sync` 放行
  - `os` 交给系统回写
  - fsync 失败：关闭待落盘连接，不返回假 ACK

## STRUCT 结构

```
include/kv/
  engine.hpp   kv::Engine：有界 LRU 缓存
  cli.hpp      tokenize/parse/CommandSpec/commands() 命令表视图
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

