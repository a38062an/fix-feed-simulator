#include <iostream>
#include <network/packet_capturer.h>

const std::string BPF_FILTER = "udp port 9999";
const std::string CAPTURE_DEVICE = "en0";

/**
 * This is our C++ callback function.
 * It will be called by PacketCapturer for every packet.
 */
void onPacketReceived(const uint8_t *data, size_t size)
{
    std::cout << "--- PACKET RECEIVED (" << size << " bytes) ---" << std::endl;

    // Print the raw FIX message
    std::string fixMessage(reinterpret_cast<const char *>(data), size);
    std::cout << fixMessage << std::endl;
}

int main()
{
    std::cout << "Starting Packet Analyzer..." << std::endl;
    std::cout << "Device: " << CAPTURE_DEVICE << std::endl;
    std::cout << "Filter: " << BPF_FILTER << std::endl;

    try
    {
        PacketCapturer capturer(CAPTURE_DEVICE, BPF_FILTER);

        std::cout << "Capture loop starting. Waiting for packets..." << std::endl;

        // This is a blocking call. It will run forever.
        capturer.startCapture(onPacketReceived);
    }
    catch (const std::exception &e)
    {
        std::cerr << "FATAL ERROR: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}