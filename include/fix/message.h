#ifndef MARKET_DATA_SYSTEM_MESSAGE_H
#define MARKET_DATA_SYSTEM_MESSAGE_H

#include <vector>
#include <string>
#include <string_view>
#include <cstdint>

// FIX protocol uses 0x01 (Start of Heading) as the separator
constexpr char SOH = '\x01';

class FIXMessage
{
public:
    FIXMessage() = default;

private:
    std::vector<uint8_t> buffer_;
};

#endif // MARKET_DATA_SYSTEM_MESSAGE_H