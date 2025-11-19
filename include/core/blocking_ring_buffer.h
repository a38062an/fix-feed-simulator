//
// Created by Anthony Nguyen on 13/11/2025.
//

#ifndef MARKET_DATA_SYSTEM_BLOCKING_RING_BUFFER_HPP
#define MARKET_DATA_SYSTEM_BLOCKING_RING_BUFFER_HPP
#include <cstddef>
#include <array>
#include <mutex>
#include <condition_variable>

template <typename T, size_t Capacity>
class BlockingRingBuffer
{
public:
    /**
     *
     * @brief stop function
     *
     * Making sure that on stoppage of main thread      *
     * doesn't sleep forever in ring buffer
     */
    void stop()
    {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            stopped_ = true;
        }
        // Wake up EVERYONE so they can check the stopped_ flag and exit
        notFullCv_.notify_all();
        notEmptyCv_.notify_all();
    }

    bool push(const T &item)
    {
        // 1. Acquire the lock
        std::unique_lock lock(mtx_);

        // 2. Wait until the condition is true (there is space)
        notFullCv_.wait(lock, [this]
                        { return count_ < Capacity || stopped_; });

        // --- Given that we own the lock AND there is space ---

        // 3. Add the item to the buffer
        buffer_[writeIndex_] = item;

        // 4. Update the write pointer and count (Round robin)
        writeIndex_ = (writeIndex_ + 1) % Capacity;
        count_++;

        // 5. Manually unlock
        lock.unlock();

        // 6. Notify a waiting consumer (Potential pop thread)
        notEmptyCv_.notify_one();

        // 7. Return
        return true;
    }

    bool pop(T &item)
    {
        // 1. Acquire lock
        std::unique_lock lock(mtx_);

        // 2. Wait until there is an item to pop
        notEmptyCv_.wait(lock, [this]
                         { return count_ > 0 || stopped_; });

        // --- Given that we own the lock there is an item to pop ---

        // 3. Read item to pop
        item = buffer_[readIndex_];

        // 4. Update the readIndex_ and count
        readIndex_ = (readIndex_ + 1) % Capacity;
        count_--;

        // 5. Unlock
        lock.unlock();

        // 6. Notify a push thread
        notFullCv_.notify_one();

        // 7. return
        return true;
    }

private:
    std::array<T, Capacity>
        buffer_;
    size_t writeIndex_ = 0;
    size_t readIndex_ = 0;
    size_t count_ = 0; // Keep track of how many items are in

    std::mutex mtx_;
    std::condition_variable notFullCv_;
    std::condition_variable notEmptyCv_;

    bool stopped_{false}; // Shut down flag
};
#endif // MARKET_DATA_SYSTEM_BLOCKING_RING_BUFFER_HPP