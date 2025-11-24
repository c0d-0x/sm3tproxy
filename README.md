# sm3tproxy

A lightweight programmable TCP proxy server.

> [!NOTE]
> This project is experimental, developed to explore C23 features and network programming concepts.

## Quick Start

### 1. Build

```bash
make
```

### 2. Create proxy group

```bash
sudo groupadd sm3tproxy
sudo usermod -aG sm3tproxy $USER
```

### 3. Setup iptables

```bash
sudo make setup-nat
```

### 4. Run

```bash
sudo make run
```

Or manually:

```bash
sudo sg sm3tproxy bin/sm3tproxy
```

### 5. Test

```bash
curl http://example.com

## Test in theory
## curl → iptables REDIRECT → sm3tproxy (port 8080) → real server
```

### 6. Cleanup

```bash
# Stop the proxy (Ctrl+C)

# Remove iptables rules
sudo make clean-nat

# Clean build files
make clean
```

## Roadmap

- [x] Event driven processing with epoll.
- [x] Transparent TCP proxy.
- [ ] HTTP request and response parsing.
- [ ] Logging of requests, headers, and payloads.
- [ ] Rule-based traffic filtering (domain, keyword, method) with cli/ a config file.
- [ ] Optional HTTPS interception using OpenSSL.
- [ ] Lua scripting support for dynamic filtering rules.
