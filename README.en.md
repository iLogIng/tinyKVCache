# tinyKVCache

> A tiny, zero-dependency in-memory key-value cache in C++17.
> Features:
> - Bounded LRU eviction
> - local cli | remote cli | server | client
> - Network service / client library
> - AOF replay to rebuild the cache on the server

## START

### BUILD

```sh
cmake -B build
cmake --build build
```

Outputs:
- `KVCache-LocalCli`    local cli
- `KVCache-RemoteCli`   remote cli
- `KVCache-Client`      client
- `KVCache-Server`      server
- `kv_tests`            tests

### TEST

```sh
ctest --test-dir build --output-on-failure
```

### USE

#### LOCAL CLI

```sh
./build/KVCache-LocalCli put name "hello world"
./build/KVCache-LocalCli get name
printf 'put a 1\nput b 2\nget a\nlst\n' | ./build/KVCache-LocalCli
```

- Usage: `KVCache-LocalCli <command> [args...]`
- With no argv it enters a stdin session: prints the command list once, runs line by line, skips blank lines; exit with EOF (Ctrl-D)
- Uses `kv.aof` and fsync `always`; default capacity 64

#### REMOTE CLI

```sh
printf 'put a 1\nget a\n' | ./build/KVCache-RemoteCli --port 7379
```

- Usage: `KVCache-RemoteCli [--host <ip>] [--port <n>]`
- Reads commands from stdin line by line; on non-tty it sends the whole batch and then collects responses

#### CLIENT

```sh
./build/KVCache-Client --port 7379 get a
```

- Usage: `KVCache-Client [--host <ip>] [--port <n>] <command> [args...]`

Library API (`client.hpp`):

```cpp
#include "kv/client.hpp"

kv::Client client;
client.connect("127.0.0.1", 7379);
std::string payload;
client.request(kv::cmd_put("k", "v"), payload);
client.request(kv::cmd_get("k"), payload);

// batch (pipeline): send without waiting, then receive by count
client.send_batch({kv::cmd_put("a", "1"), kv::cmd_put("b", "2")});
std::vector<std::string> out;
client.recv_batch(2, out);
```

- `connect/close/is_open`; `send/recv` primitives; `send_batch/recv_batch` pipeline; `request/request_batch` send+receive
- Command factories: `cmd_put/cmd_get/cmd_del/cmd_clr/cmd_lst/cmd_help`
- Links against `kvcache_net`; single-threaded, blocking, not thread-safe

#### SERVER

```sh
./build/KVCache-Server --port 7379 --capacity 64 --aof kv.aof --fsync always
```

- Usage: `KVCache-Server --port <n> [--bind <ip>] [--capacity <n>] [--aof <path>] [--fsync always|group|os] [--fsync-interval <ms>]`
  - `--bind` defaults to `127.0.0.1`; binding `0.0.0.0` exposes it to the network with no authentication, use with care
  - `--capacity` defaults to 64; `--aof` defaults to `kv.aof`
  - `--fsync-interval` only applies to `group`, defaults to 1ms

Library API (`server.hpp`):

```cpp
struct ServerConfig { /* bind / port / capacity / aof_path / fsync / fsync_interval_ms */ };
class Server {
    explicit Server(ServerConfig config);
    bool run();
};
```

## COMMAND

| Command | Description |
|---|---|
| `put k v` | Insert or overwrite; evicts the least recently used key when full |
| `get k` | Read and refresh recency; prints `k->v`, or `'k' not found` |
| `del k` | Delete (frees memory immediately) |
| `clr` | Clear all |
| `lst` | List all keys sorted by key |

CLI input is split on whitespace; single/double quotes may contain spaces/commas, `\` escapes; network calls pass arguments verbatim.
The command table is the single source of truth: `CommandSpec[]` in `regcmd.cpp` is exposed by `commands()`; adding or changing a command only touches the table and its handler.

## LRU

**Layout** (array-based intrusive list, indices instead of pointers, `-1` means empty):

```cpp
struct Slot {
    std::string key;    // key
    std::string value;  // value
    int prev;           // previous index
    int next;           // next index
};
```

- `cache: vector<Slot>`: fixed-capacity slots
- `catalog: unordered_map<string,int>`: key -> slot index
- `frstk: vector<int>`: free slot stack, slots are recycled after erase/eviction
- `head_`: MRU end; `tail_`: LRU end

**Semantics**: on `get` hit or `put` of an existing key, unlink the slot and move it to `head_`; on `put` of a new key with no free slot, evict `tail_` and reuse it. Both erase and eviction update `catalog`.

## PROTOCOL

```
Request:  [1B ver=1][4B body_len][body]
          body = [1B cmdargs][1B cmd_len][cmd][arg_i: 4B len + bytes] * cmdargs
Response: [1B ver=1][4B body_len][body]
          body = result text bytes (length 0 = no output)
```

- Arguments are raw bytes, no escaping, empty values are representable; `cmdargs` is the number of command arguments (excluding the command name)
- `body_len <= 1MB`; `cmd_len == 0` is invalid; a version mismatch closes the connection

## PERSIST

> Managed by `kv::Journal`

- **Boundary**: `Engine` only holds a `Journal*`; file I/O, record encoding/decoding and replay all live in the `Journal` module
- **Record format**: `[1B op][4B klen][key][4B vlen][value]`, length-prefixed, values may contain arbitrary bytes
- **Write path**: `put/del/clr` append first and then mutate memory; if append fails, memory is not changed, keeping the log and memory consistent
- **Replay**: on startup, records are read one by one and replayed through a callback to rebuild the cache; write-driven, LRU is rebuilt in write order only
- **Bad tail**: an incomplete trailing record is truncated and recovered
- **fsync strategies**:
  - `always` fsync on every append
  - `group` batch writes: `dirty` flag + `last_fsync` time + write count; a single fsync after `--fsync-interval`, responses gated by `pending_sync`
  - `os` let the system write back
  - On fsync failure, pending connections are closed instead of returning a false ACK

## STRUCT

```
include/kv/
  engine.hpp   kv::Engine: bounded LRU cache
  cli.hpp      tokenize/parse/CommandSpec/commands() view
  regcmd.hpp   kv::exec(..., out): table lookup and dispatch
  journal.hpp  persistence: op log read/write and replay
  net.hpp      frame codec and socket primitives
  client.hpp   kv::Client client library
  server.hpp   kv::Server network service
src/kv/
  engine.cpp  slot pool + intrusive-list LRU
  cli.cpp     tokenizer / parser
  regcmd.cpp  command handlers, table, dispatch
  journal.cpp log codec + file I/O + bad-tail truncation
  net.cpp     frame codec
  client.cpp  Client implementation
  server.cpp  Server event loop
example/
  local_cli_main.cpp    local CLI
  remote_cli_main.cpp   remote CLI
  client_main.cpp       client library example
  server_main.cpp       server entry
tests/
  test_engine.cpp  LRU semantics
  test_cli.cpp     tokenize/parse/exec
  test_journal.cpp log/bad-tail/engine hook
  test_net.cpp     frame codec
  test_client.cpp  client + fake server integration
```
