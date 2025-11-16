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
cmake --build build -j
```

## Running

### Start the Packet Analyzer

The analyzer requires raw socket access and typically needs elevated privileges:

```bash
sudo ./build/packet_analyzer
```

### Send Test Data

In a separate terminal, run the UDP sender:

```bash
./build/udp_sender
```

The sender broadcasts FIX market data snapshots to the configured multicast address. The analyzer will capture and display the parsed messages.

## FIX Message Format

The system handles FIX 4.2 Market Data Snapshot messages (MsgType=W):

| Tag | Field           | Example     | Description                    |
|-----|-----------------|-------------|--------------------------------|
| 8   | BeginString     | FIX.4.2     | Protocol version               |
| 9   | BodyLength      | 75          | Message body length in bytes   |
| 35  | MsgType         | W           | Market Data Snapshot           |
| 55  | Symbol          | ESZ5        | Instrument identifier          |
| 268 | NoMDEntries     | 2           | Number of market data entries  |
| 269 | MDEntryType     | 0           | 0=Bid, 1=Offer                 |
| 270 | MDEntryPx       | 99.78       | Price                          |
| 271 | MDEntrySize     | 100         | Quantity                       |
| 10  | CheckSum        | 009         | Message integrity check        |

## Project Structure

```
market-data-system/
├── include/
│   └── market/
│       └── PacketCapturer.h    # Packet capture wrapper
├── src/
│   ├── analyzer_main.cpp       # Packet analyzer entry point
│   └── producer_main.cpp       # UDP sender entry point
├── CMakeLists.txt
└── README.md
```

## Development

After modifying source files, rebuild with:

```bash
cmake --build build
```

Only re-run the configure step if CMakeLists.txt changes:

```bash
cmake -S . -B build
```

The build generates `compile_commands.json` for IDE and clangd integration.

## License

See LICENSE file for details.
