#ifndef MARKET_DATA_SYSTEM_PACKET_CAPTURER_H
#define MARKET_DATA_SYSTEM_PACKET_CAPTURER_H

#include <string>
#include <functional> // For std::function (our callback)
#include <memory>     // For std::unique_ptr
#include <stdexcept>
#include <vector>
#include <pcap/pcap.h> // The main libpcap header

inline void pcapCallback(u_char *userData,
                         const struct pcap_pkthdr *header,
                         const u_char *packetData)
{
    // Cast user data back to PacketCapturer class
    auto capturer = reinterpret_cast<PacketCapturer *>(userData);

    capturer->handlePacket(header, packetData);
}

class PacketCapturer
{
public:
    using PacketCallback = std::function<void(const uint8_t *, size_t)>;

private:
    struct PcapDelete
    {
        void operator()(pcap_t *p) const
        {
            if (p)
            {
                pcap_close(p);
            }
        }
    }

    std::unique_ptr<pcap_t, PcapDeleter>
        pcapHandle_;
};

#endif // MARKET_DATA_SYSTEM_PACKET_CAPTURER_H