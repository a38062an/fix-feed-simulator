# market-data-system

Small market data system library and utilities.

Quick start

1. Create a build directory and run CMake:

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -- -j
```

2. The project uses C++20 features (concepts, std::span). Ensure your compiler supports C++20.

Simple build & run (minimal)

1) Configure (only needed once or after changing CMakeLists):

```bash
cmake -S . -B build
```

2) Build (after editing sources/headers):

```bash
cmake --build build
```

3) Run the program:

```bash
./build/market_data_system
```

One-liner to build then run:

```bash
cmake --build build && ./build/market_data_system
```

Alternatively, for a very quick test you can compile a single file directly with g++:

```bash
g++ -std=c++20 -Iinclude src/producer_main.cpp -o build/market_data_system && ./build/market_data_system
```

Notes:

- Re-run the configure step (`cmake -S . -B build`) only when you change `CMakeLists.txt`.
- The g++ line is for quick experiments; prefer the CMake workflow for the full project.
