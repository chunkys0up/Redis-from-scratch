# MiniRedis: A Custom Redis Clone in C++

This is a custom Redis server implementation written in C++. It supports a wide range of commands—from basic strings and lists to stream and transaction handling—modeled closely after real Redis behavior.

---

### String Commands

- `SET key value [PX milliseconds]`
- `GET key`
- `DEL key`
- `INCR key`

### List Commands

- `RPUSH key value [value ...]`
- `LPUSH key value [value ...]`
- `LPOP key [count]`
- `RPOP key`
- `LLEN key`
- `LRANGE key start stop`
- `BLPOP key timeout`
- `BRPOP key timeout`

### Transaction Commands

- `MULTI`
- `EXEC`
- `DISCARD`

### Stream Commands

- `XADD stream *|id field value [field value ...]`
- `XREAD [BLOCK ms] STREAMS key id [key2 id2 ...]`
- `XRANGE key start end`

### Utility Commands

- `PING`
- `ECHO message`
- `TYPE key`

---

### Prerequisites

- `cmake` installed
- C++ compiler (e.g., `g++`)
- Unix-like system (Linux/macOS recommended)
- Installing vcpkg package
```
git clone https://github.com/microsoft/vcpkg.git
```
- Installing redis, so we can use redis-cli
```
brew install redis
```

---

### Start the Server

```bash
./your_program.sh
```

### Connect from Client

```bash
redis-cli
PING
```


