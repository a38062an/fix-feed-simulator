#ifndef MARKET_DATA_SYSTEM_PACKET_CAPTURER_H
#define MARKET_DATA_SYSTEM_PACKET_CAPTURER_H

#include <string>
#include <functional> // For std::function (our callback)
#include <memory>     // For std::unique_ptr
#include <stdexcept>
#include <vector>
#include <pcap/pcap.h> // The main libpcap header
#include <iostream>

/**
 * @file PacketCapturer.h
 * @brief Header-only packet capture wrapper around libpcap
 *
 * DECLARATION ORDER EXPLANATION:
 *
 * This file has a careful ordering to resolve circular dependencies between
 * the PacketCapturer class and the pcapCallback trampoline function:
 *
 * 1. Forward declare PacketCapturer (tells compiler the class exists)
 * 2. Declare pcapCallback signature (so PacketCapturer can reference it)
 * 3. Define PacketCapturer class fully (uses pcapCallback in startCapture)
 * 4. Define pcapCallback body (uses PacketCapturer methods like handlePacket)
 *
 * Why? PacketCapturer needs to know pcapCallback exists to pass it to pcap_loop(),
 * and pcapCallback needs to know PacketCapturer's full definition to call its methods.
 * Forward declaration + separate declaration/definition breaks this cycle.
 */

// Forward declare PacketCapturer so the C-style callback can be declared
class PacketCapturer;

// Declare the C-style callback so it can be referenced inside the class
inline void pcapCallback(u_char *userData, const struct pcap_pkthdr *header, const u_char *packetData);

class PacketCapturer
{
public:
    using PacketCallback = std::function<void(const uint8_t *, size_t)>;

    /**
     * @brief Constructor to create Packet Capturer
     *
     * - Creates a handle to a packet capturing session that is almost like an id.
     * - We create the handle, set handle options, create filter, setfilter, and finally
     * encapsulate handle in a unique_ptr for resource management.
     */
    PacketCapturer(const std::string &device, const std::string &filter)
    {
        char errbuf[PCAP_ERRBUF_SIZE]; // Error Buffer

        // 1. Open the device for live capture
        pcap_t *handle = pcap_create(device.c_str(), errbuf);
        if (!handle)
        {
            throw std::runtime_error("pcap_create() failed: " + std::string(errbuf));
        }

        // Set options
        pcap_set_snaplen(handle, 1518);
        pcap_set_promisc(handle, 1);
        pcap_set_timeout(handle, 1000);

        // Activate the handle
        if (pcap_activate(handle) != 0)
        {
            pcap_close(handle);
            throw std::runtime_error("pcap_activate() failed: " + std::string(pcap_geterr(handle)));
        }

        // 2. Compile the BPF filter
        bpf_program filterProgram;
        if (pcap_compile(handle, &filterProgram, filter.c_str(), 0, PCAP_NETMASK_UNKNOWN) == -1)
        {
            throw std::runtime_error("pcap_compile() failed: " + std::string(pcap_geterr(handle)));
        }

        if (pcap_setfilter(handle, &filterProgram) == -1)
        {
            pcap_freecode(&filterProgram);
            throw std::runtime_error("pcap_setfilter() failed: " + std::string(pcap_geterr(handle)));
        }

        pcap_freecode(&filterProgram);

        // Store handle in unique_ptr with custom deleter
        pcapHandle_.reset(handle);
    }

    /**
     * @brief Starts the blocking packet capture loop.
     */
    void startCapture(PacketCallback cb)
    {
        userCallback_ = std::move(cb);
        pcap_loop(pcapHandle_.get(), -1, pcapCallback, reinterpret_cast<u_char *>(this));
    }

    /**
     * @brief C++ internal packet handler, called by the pcap_loop C-style trampoline.
     *
     * [ ETHERNET (14) | IP (Variable) | UDP (8) | PAYLOAD (Your FIX) ]
     *
     * @param header A pointer to the pcap packet header, which contains
     * metadata like the captured length (`caplen`). This is used
     * to prevent buffer overruns on truncated packets.
     * @param packetData A pointer to the raw byte buffer of the captured
     * packet, starting from the Ethernet (Layer 2) header.
     */
    void handlePacket(const struct pcap_pkthdr *header, const u_char *packetData)
    {
        const u_char *ipPacket = packetData + 14;

        // Extract IHL Field that stores n.o 32 bit words, doing * 4 will find total n.o bits -> size of ip header
        uint8_t ipHeaderLength = (*ipPacket & 0x0F) * 4;

        const u_char *udpPacket = ipPacket + ipHeaderLength;
        const u_char *payload = udpPacket + 8;

        size_t ethHeaderLen = (ipPacket - packetData);
        size_t udpHeaderLen = 8;
        size_t totalHeaderLen = ethHeaderLen + ipHeaderLength + udpHeaderLen;

        if (header->caplen < totalHeaderLen)
        {
            std::cerr << "Captured truncated packet." << std::endl;
            return;
        }

        size_t payloadSize = header->caplen - totalHeaderLen;
        if (userCallback_ && payloadSize > 0)
        {
            userCallback_(payload, payloadSize);
        }
    }

private:
    struct PcapDeleter
    {
        void operator()(pcap_t *pcapHandle) const
        {
            if (pcapHandle)
            {
                pcap_close(pcapHandle);
            }
        }
    };

    std::unique_ptr<pcap_t, PcapDeleter> pcapHandle_;
    PacketCallback userCallback_;
};

// Define the C-style callback after the class so PacketCapturer is a complete type
inline void pcapCallback(u_char *userData, const struct pcap_pkthdr *header, const u_char *packetData)
{
    auto capturer = reinterpret_cast<PacketCapturer *>(userData);
    capturer->handlePacket(header, packetData);
}

#endif // MARKET_DATA_SYSTEM_PACKET_CAPTURER_H