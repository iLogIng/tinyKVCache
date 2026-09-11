# RESP2

> tinyKVCache 支持的 RESP2 子集

## REQUEST 请求

数组 + 批量字符串：

```
*<n>\r\n
$<len>\r\n<arg>\r\n     * n
```

## RESPONSE 响应

```
+OK\r\n                 简单字符串
-ERR ...\r\n            错误
:1\r\n                  整数
$<len>\r\n<data>\r\n    批量字符串
$-1\r\n                 null
```

## COMMAND 命令

`PING` / `SET` / `GET` / `DEL` / `EXISTS`

## SERVER 服务

- `kv::Resp2Server` 由 `kv::Server` 复制并更改而来
- 可执行程序：`KVCache-Resp2Server`
- 测试：`tests/test_resp2.cpp`
- 参数：`KVCache-Resp2Server --port <n> [--bind <ip>] [--capacity <n>] [--aof <path>] [--fsync always|group|os] [--fsync-interval <ms>]`

## EXAMPLE 示例

请求 `SET k v`：

```
*3\r\n$3\r\nSET\r\n$1\r\nk\r\n$1\r\nv\r\n
```

响应：

```
+OK\r\n
```

请求 `GET k`（命中）：

```
*2\r\n$3\r\nGET\r\n$1\r\nk\r\n
```

响应：

```
$1\r\nv\r\n
```

## END

