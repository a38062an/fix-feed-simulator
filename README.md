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
```

Produced executables (in `build/`):

- `packet_analyzer`
- `udp_sender_gbm`
- `udp_sender_rw`

## Running

### Packet analyzer

Requires raw socket access on most systems:

```bash
sudo ./build/packet_analyzer
```

### UDP senders (test data)

Run a sender in a separate terminal:

```bash
./build/udp_sender_gbm   # GBM generator
./build/udp_sender_rw    # Random walk generator
```

Both senders broadcast FIX market data to `239.255.1.1:9999` by default.

### Dashboard (optional)

Use the included Python dashboard to visualize feed data:

```bash
# start a sender first
python3 dashboard.py
```

Install `matplotlib` if required.

## FIX message format

The project produces and consumes FIX 4.2 Market Data Snapshot messages (MsgType = W). Key tags:

- `8`  BeginString (FIX.4.2)
- `35` MsgType (W)
- `55` Symbol (e.g., ESZ5)
- `268` NoMDEntries
- `269` MDEntryType (0=Bid, 1=Offer)
- `270` MDEntryPx (price)
- `271` MDEntrySize (quantity)

## Project layout

```
market-data-system/
├── include/                # headers (core helpers, generators)
├── src/
│   ├── analyzer_main.cpp
│   ├── producer_gbm.cpp
│   └── producer_rw.cpp
├── tests/                  # stress/benchmarks
├── dashboard.py            # optional Python visualizer
├── CMakeLists.txt
└── README.md
```

## Notes

- The repository now exposes two explicit UDP senders; there is no single generic `udp_sender` target.
- To restore any deleted legacy files, run:

```bash
git checkout -- include/market/market_data_system.h src/producer_main.cpp src/main.cpp
```

## Development

After source changes run:

```bash
cmake --build build -- -j$(nproc || sysctl -n hw.ncpu)
```

Run `cmake -S . -B build` only when `CMakeLists.txt` changes.

## License

See `LICENSE` for details.
