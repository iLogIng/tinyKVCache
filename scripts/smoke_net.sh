#!/bin/sh
# 冒烟: 起 server -> 跨连接增删查 -> 退出。用法: ./scripts/smoke_net.sh [port]
set -e
PORT="${1:-17391}"
BIN=build
PORT_ARG="${PORT}"

"$BIN/KVCacheServer" "$PORT_ARG" 8 >/dev/null 2>&1 &
SRV=$!
sleep 0.3
trap 'kill "$SRV" 2>/dev/null || true' EXIT

echo "-- 会话1: put --"
printf 'put a 1\nput b 2\n' | "$BIN/KVCacheClient" "$PORT_ARG"

echo "-- 会话2: get/lst (跨连接) --"
printf 'get a\nlst\nget z\n' | "$BIN/KVCacheClient" "$PORT_ARG"

echo "-- 会话3: 引号值 + 坏命令 + quit --"
printf 'put k "hello world"\nget k\nnope x\nquit\n' | "$BIN/KVCacheClient" "$PORT_ARG"

kill "$SRV" 2>/dev/null || true
wait "$SRV" 2>/dev/null || true
trap - EXIT
echo "OK"
