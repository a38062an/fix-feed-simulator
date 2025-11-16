#include <iostream>
#include <string>
#include <thread>    // For std::this_thread
#include <chrono>    // For std::chrono
#include <stdexcept> // For std::runtime_error
#include <iomanip>   // For std::setprecision

// --- Project headers ---
#include <market/random_walk_generator.h>
#include <network/udp_sender.h>
#include <fix/message.h>

// --- Configuration ---
const std::string MULTICAST_IP = "239.255.1.1";
const uint16_t PORT = 9999;
const std::string SYMBOL = "ESZ5";

int main()
{

    std::cout << "Starting market data server... " << std::endl;
    std::cout << "Sending data to ip: " << MULTICAST_IP << ":" << PORT << std::endl;

    try
    {
        // 1. Initialise components
        UDPMulticastSender sender(MULTICAST_IP, PORT);

        RandomWalkGenerator<double> priceGenerator(100.0, 0.01);

        FIXMessage fixMessage("FIX.4.2");

        // Set floating point
        std::cout << std::fixed << std::setprecision(2);

        // 2. Main generation loop
        while (true)
        {
            // Get new price
            double bidPrice = priceGenerator.getNextPrice();
            double askPrice = bidPrice + 0.25; // Spread

            fixMessage.clearBody();

            // Build CME-style snapshot message
            fixMessage.addField(35, "W") // MsgType = Snapshot
                .addField(55, SYMBOL)    // Symbol
                .addField(268, "2");     // NoMDEntries = 2(1 bid, 1 ask)

            // Add Bid
            fixMessage.addField(269, "0")                // MDEntryType = Bid
                .addField(270, std::to_string(bidPrice)) // Bid Price
                .addField(271, "100");                   // MDEntrySize

            // Add Ask
            fixMessage.addField(269, "1")                // MDEntryType = Ask
                .addField(270, std::to_string(askPrice)) // Ask Price
                .addField(271, "75");                    // MDEntrySize

            // Add header information + checksum
            std::span<const uint8_t> completeMessage = fixMessage.finalize();

            // Send message over network
            sender.send(completeMessage);

            std::cout << "Sent " << SYMBOL << " - Bid " << bidPrice
                      << " | Ask: " << askPrice << std::endl;

            // Rate-limit the feed
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
