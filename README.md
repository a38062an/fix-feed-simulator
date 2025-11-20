# Market Data System

Low-latency market data capture and analysis system for processing FIX protocol messages over UDP multicast.

## Overview

This project provides two main components:

- **packet_analyzer**: Captures UDP packets from the network and extracts FIX market data
- **udp_sender**: Generates and broadcasts sample FIX messages for testing

The system uses libpcap for packet capture and handles the complete network stack (Ethernet/IP/UDP) to extract application-layer FIX protocol messages.

## Requirements

- C++20 compatible compiler (GCC 10+, Clang 12+)
- CMake 3.16 or higher
- libpcap development headers

### Install Dependencies

**Ubuntu/Debian:**

```bash
sudo apt-get install libpcap-dev build-essential cmake
```

**macOS:**

```bash
brew install libpcap cmake
```

## Building

Configure and build both executables:

```bash
cmake -S . -B build
cmake --build build
```

For optimized release build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
# Market Data System

Low-latency market data capture and analysis system for processing FIX protocol messages over UDP multicast.

## Overview

This project provides two main components:

- `packet_analyzer` — captures UDP packets and extracts FIX market data.
- `udp_sender_gbm` and `udp_sender_rw` — two explicit UDP senders for testing:
 - `udp_sender_gbm` uses a Geometric Brownian Motion price generator.
 - `udp_sender_rw` uses a Random Walk price generator.

The system uses `libpcap` for packet capture and processes Ethernet/IP/UDP to extract application-layer FIX messages.

## Requirements

- C++20 compatible compiler (GCC 10+, Clang 12+)
- CMake 3.20 or higher
- libpcap development headers
- Python 3 (optional, for the dashboard)
 - `matplotlib` (if you run `dashboard.py`)

### Install Dependencies

**Ubuntu / Debian**

```bash
sudo apt-get install libpcap-dev build-essential cmake python3 python3-venv
python3 -m pip install --user matplotlib
```

**macOS**

```bash
brew install libpcap cmake python
python3 -m pip install --user matplotlib
```

## Building

Configure and build:

```bash
cmake -S . -B build
cmake --build build -- -j$(nproc || sysctl -n hw.ncpu)
```

For a release build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -- -j$(nproc || sysctl -n hw.ncpu)
# Market Data System

Low-latency market data capture and analysis system for processing FIX protocol messages over UDP multicast.

## Overview

This repository implements a small market-data test harness composed of:

- `packet_analyzer` — captures raw packets (via libpcap), decodes Ethernet/IP/UDP and extracts FIX 4.2 messages.
- `udp_sender_gbm` — producer that generates FIX ticks using a Geometric Brownian Motion (GBM) price model.
- `udp_sender_rw` — producer that generates FIX ticks using a Random Walk model.
- `producer_gbm_nonblocking`, `producer_rw_nonblocking` — non-blocking SPSC producer variants using a lock-free ring buffer.

Additionally the repo contains a set of micro-benchmarks and stress tests (latency/throughput) under `tests/`.

## Key Changes (recent updates)

- Added two non-blocking producer mains: `producer_gbm_nonblocking` and `producer_rw_nonblocking` (run until signalled).
- Implemented and fixed a lock-free SPSC ring buffer in `include/core/nonblocking_ring_buffer.h` (bug fixes for atomics, masks, and capacity accessor).
- Added latency and throughput benchmarks under `tests/` (`latency_benchmark`, `benchmark_throughput`).
- Added a ring-buffer stress test (ad-hoc) used to compare blocking vs non-blocking behavior; sample results are recorded below.
- Made producer mains accept CLI args (destination IP and port) and default to loopback (`127.0.0.1:9999`) for safe local testing.
- Fixed FIX message formatting (checksum field now emits `10=NNN<SOH>`).
- Packet capture code (`include/network/packet_capturer.h`) was refactored to fix callback ordering and memory ownership issues.

## Requirements

- C++20 compatible compiler (Clang or GCC)
- CMake 3.20+
- `libpcap` development headers (for `packet_analyzer`) — optional for sender-only tests
- Python 3 + `matplotlib` (optional, for `dashboard.py`)

## Build

Configure and build everything:

```bash
cmake -S . -B build
cmake --build build -- -j$(nproc || sysctl -n hw.ncpu)
```

Produced binaries are placed in `build/`. Notable targets:

- `packet_analyzer`
- `udp_sender_gbm`, `udp_sender_rw`
- `producer_gbm_nonblocking`, `producer_rw_nonblocking`
- `latency_benchmark` (tests/benchmark_latency.cpp)
- `benchmark_throughput` and other tests under `tests/`

## Run

Packet analyzer (may require `sudo` on some platforms):

```bash
sudo ./build/packet_analyzer
```

UDP senders / producers (examples):

```bash
./build/udp_sender_gbm            # multicast by default (239.255.1.1:9999)
./build/udp_sender_rw

# Non-blocking producers (run until Ctrl+C). Accepts optional args: [dest_ip] [port]
./build/producer_gbm_nonblocking 127.0.0.1 9999
./build/producer_rw_nonblocking 127.0.0.1 9999
```

Benchmarks:

```bash
./build/latency_benchmark
./build/benchmark_throughput
```

## Testing & Results (summary)

Quantitative example (ring-buffer stress test, 5s run, tight producer):

- Blocking ring buffer: produced = 35,851,686; consumed = 35,851,686; dropped = 0
 	- Throughput ≈ 7.17M ops/sec
- Non-blocking ring buffer: produced = 43,554,484; consumed = 43,554,484; dropped = 610,135
 	- Total push attempts = 44,164,619; drop rate ≈ 1.38%; throughput ≈ 8.71M ops/sec

Qualitative observations:

- Blocking buffer provides backpressure and guarantees no drops, but limits observed throughput (producer blocks when buffer full).
- Non-blocking implementation allows higher producer throughput at the cost of occasional drops; observed low single-digit percentage drop rate in stress runs.
- Lock-free code requires careful memory ordering and per-slot handoff semantics for safety with non-trivial types (e.g., `std::string`). Use sanitizers or a proven library for production use.

## Libraries & References

- libpcap — packet capture and BPF filters.
- C++ standard library (C++20): `std::span`, `std::format`, atomics and threading primitives.
- Suggested third-party libs (for production): `rigtorp/spsc_queue` (SPSC lock-free queue), `moodycamel::ConcurrentQueue` (MPMC), QuickFIX (FIX engine).

## Network notes

- The senders default to multicast `239.255.1.1:9999`. Multicast may fail with `sendto: No route to host` if the OS routing or outgoing interface isn't configured. For development, use loopback (`127.0.0.1`) or configure `IP_MULTICAST_IF` and `IP_MULTICAST_LOOP` on the sending socket.
- Analyzer captures on interfaces; use `sudo tcpdump -i en0 udp port 9999` to validate multicast on `en0` or `lo0` for loopback testing.

## Development notes & recommended next steps

1. Run tests under sanitizers (TSan / ASan) to detect races or UB in lock-free sections.
2. Consider replacing the custom lock-free queue with a well-tested implementation (rigtorp/spsc_queue) if your system needs production-grade guarantees.
3. Add latency histograms (per-message timestamps) and CSV export for automated analysis.
4. Add CLI flags for producers to choose buffering policy (blocking vs nonblocking) and retry/backoff strategies.

## Project layout

```
market-data-system/
├── include/                # headers (core helpers, generators)
├── src/                    # producer and analyzer mains + helpers
├── tests/                  # latency/throughput benchmarks
├── build/                  # CMake output
├── dashboard.py            # optional Python visualizer
├── CMakeLists.txt
└── README.md
```

## License

See `LICENSE` for details.
