#ifndef MARKET_DATA_SYSTEM_NONBLOCKING_RING_BUFFER_H
#define MARKET_DATA_SYSTEM_NONBLOCKING_RING_BUFFER_H

#include <cstddef>
#include <array>
#include <atomic>
#include <condition_variable>
#include <bit>
#include <concepts>
#include <new>

// Default to 64 cache-line padding where available
constexpr std::size_t CACHE_LINE_SIZE =
    (std::hardware_destructive_interference_size > 0 ? std::hardware_destructive_interference_size : 64);

template <typename T, size_t Capacity>
    requires(std::has_single_bit(Capacity))
class LockFreeRingBuffer
{
public:
    LockFreeRingBuffer() : writeIndex_{0},
                           readIndex_{0}
    {
    }

    // Disable copy / move operations to prevent state corruption
    LockFreeRingBuffer(const LockFreeRingBuffer &) = delete;
    LockFreeRingBuffer &operator=(const LockFreeRingBuffer &) = delete;
    LockFreeRingBuffer(const LockFreeRingBuffer &&) = delete;
    LockFreeRingBuffer &operator=(const LockFreeRingBuffer &&) = delete;

    [[nodiscard]] bool push(const T &item) noexcept
    {
        const auto currentWrite = writeIndex_.load(std::memory_order_relaxed);
        const auto nextWrite = currentWrite + 1;
        const auto currentRead = readIndex_.load(std::memory_order_acquire);

        // If the gap between the to write curosor and current read cursor is greater than capacity
        // then capacity has been reached (monotonic counters)
        if (nextWrite - currentRead > Capacity)
        {
            return false;
        }

        // Bit-wise mask optimisation that only works because Capacity is a power of two
        buffer_[currentWrite & (Capacity - 1)] = item;
        writeIndex_.store(nextWrite, std::memory_order_release);

        return true;
    }

    [[nodiscard]] bool pop(T &value) noexcept
    {
        const auto currentRead = readIndex_.load(std::memory_order_relaxed);
        const auto currentWrite = writeIndex_.load(std::memory_order_acquire);

        // Means queue is empty
        if (currentRead == currentWrite)
        {
            return false;
        }

        value = buffer_[currentRead & (Capacity - 1)];

        const auto nextRead = currentRead + 1;
        readIndex_.store(nextRead, std::memory_order_release);

        return true;
    }

    std::size_t size() const noexcept
    {
        size_t head = readIndex_.load(std::memory_order_acquire);
        size_t tail = writeIndex_.load(std::memory_order_acquire);

        return tail - head;
    }

    std::size_t capacity() const noexcept
    {
        return Capacity;
    }

private:
    // Putting alignas on their own cache lines (64 bit per line)
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> writeIndex_;
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> readIndex_;
    std::array<T, Capacity> buffer_;
};

#endif // MARKET_DATA_SYSTEM_NONBLOCKING_RING_BUFFER_H