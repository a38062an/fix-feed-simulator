#ifndef MARKET_DATA_SYSTEM_MESSAGE_H
#define MARKET_DATA_SYSTEM_MESSAGE_H

#include <vector>
#include <string>
#include <string_view>
#include <cstdint>
#include <span>
#include <format>
#include <iterator>
#include <format>
#include <numeric>

// FIX protocol uses 0x01 (Start of Heading) as the separator (pipe operator)
constexpr char SOH = '\x01';

class FIXMessage
{
public:
    FIXMessage(std::string_view beginString = "FIX.4.2")
        : beginString_(beginString)
    {
    }

    FIXMessage &addField(int tag, std::string_view value)
    {
        // 1. Convert tag to string
        std::string tagStr = std::to_string(tag);

        // 2. Append: "tag="
        bodyBuffer_.insert(bodyBuffer_.end(), tagStr.begin(), tagStr.end());
        bodyBuffer_.push_back('=');

        // 3. Append:"value" and seperator
        bodyBuffer_.insert(bodyBuffer_.end(), value.begin(), value.end());
        bodyBuffer_.push_back(SOH);

        // Return *this to allow chaining (pointer to the FIXMessage)
        return *this;
    }

    std::span<const uint8_t> finalize()
    {
        finalMessageBuffer_.clear();
        // --- 1. Build header ---

        // Append "8=FIX.4.2<SOH>" (TYPE OF FIX)
        std::string_view tag8 = "8=";
        finalMessageBuffer_.insert(finalMessageBuffer_.end(), tag8.begin(), tag8.end());
        finalMessageBuffer_.insert(finalMessageBuffer_.end(), beginString_.begin(), beginString_.end());
        finalMessageBuffer_.push_back(SOH);

        // Append "9=SIZE<SOH>"  (BODY LENGTH)
        std::format_to(
            std::back_inserter(finalMessageBuffer_), // Where to write to
            "9={}{}",                                // f"string"
            bodyBuffer_.size(),
            SOH);

        // 2. --- Append Body ---

        finalMessageBuffer_.insert(finalMessageBuffer_.end(), bodyBuffer_.begin(), bodyBuffer_.end());

        // 3. --- Calculate and Append Checksum

        // Summing the combination of bytes of header + body
        unsigned int sum = std::accumulate(finalMessageBuffer_.begin(),
                                           finalMessageBuffer_.end(),
                                           0);

        // Calculating remainder of summed bytes
        unsigned int checkSum = sum % 256;

        // Append Tag for example: "10=021<SOH>"
        // Append "10=XXX<SOH>" — include the '=' after the tag number
        std::format_to(
            std::back_inserter(finalMessageBuffer_),
            "10={:03}{}", // include '=' and :03 zero-pads to width 3
            checkSum,
            SOH);

        return {finalMessageBuffer_.data(), finalMessageBuffer_.size()};
    }

    // Default move semantics
    FIXMessage(FIXMessage &&other) noexcept = default;
    FIXMessage &operator=(FIXMessage &&other) noexcept = default;

    // Delete copy operations
    FIXMessage(const FIXMessage &) = delete;
    FIXMessage &operator=(const FIXMessage &) = delete;

    std::span<const uint8_t> data() const
    {
        return {bodyBuffer_.data(), bodyBuffer_.size()};
    }

    void clearBody()
    {
        bodyBuffer_.clear();
    }

private:
    std::vector<uint8_t> bodyBuffer_;

    std::vector<uint8_t> finalMessageBuffer_;

    std::string beginString_;
};

#endif // MARKET_DATA_SYSTEM_MESSAGE_H