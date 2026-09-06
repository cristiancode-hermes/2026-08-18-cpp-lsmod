# cpp-lsmod

Linux kernel module list viewer — reads `/proc/modules` and displays a formatted table of loaded modules with size, usage count, dependencies, and state.

## Features

- **Formatted table** with color-coded module names (highlighted by use count)
- **JSON output** (`--json`) for scripting and integration
- **Dependency tree** (`--tree`) showing module relationships
- **Watch mode** (`--watch N`) for real-time monitoring
- **Filter** (`--filter NAME`) to find specific modules
- **Sort** (`--sort size|deps|count|name`) by various fields
- **No-color mode** (`--no-color`) for piping and scripting
- **Self-test** (`--selftest`) and demo mode (`--demo`)

## Build

```bash
g++ -std=c++23 -O2 -Wall -Wextra src/main.cpp -o lsmod
```

## Usage

```bash
# Default: table sorted by size
./lsmod

# JSON output
./lsmod --json

# Filter by name
./lsmod --filter nf

# Sort by use count
./lsmod --sort count

# Dependency tree
./lsmod --tree

# Watch mode (refresh every 2 seconds)
./lsmod --watch 2

# Pipe-friendly (no ANSI codes)
./lsmod --no-color | grep drm
```

## Sample output

```
Module                     Size Used  Dependencies                  State
────────────────────────────────────  ─────────────────────────────────────
nf_tables             372.0 KiB  302  nft_chain_nat,nft_compat      Live
nf_conntrack          192.0 KiB    6  nf_conntrack_netlink,xt_M...  Live
raid6_pq              124.0 KiB    4  btrfs,raid456,async_raid6_r   Live
x_tables               64.0 KiB   10  xt_set,xt_MASQUERADE,xt_na   Live
-----------------------------------------------------------------------------
Total: 70 modules, 24.8 MiB loaded
```

## License

MIT
