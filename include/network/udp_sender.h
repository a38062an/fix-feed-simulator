#ifndef MARKET_DATA_SYSTEM_UDP_SENDER_H
#define MARKET_DATA_SYSTEM_UDP_SENDER_H

#include <string>
#include <stdexcept>
#include <span>
#include <iostream>
#include <cstring>
#include <cerrno>

// --- POSIX/BSD Socket Headers ---
#include <sys/socket.h> // For socket(), sendto()
#include <arpa/inet.h>  // For sockaddr_in, inet_pton()
#include <unistd.h>     // For close()
#include <cstring>      // For memset()

/*
 * Wrapper around c-style networking    threads
 * Ensures RAII principles
 */
class UDPMulticastSender
{

public:
    UDPMulticastSender(const std::string &multicast_ip, uint16_t port)
        // 1. Create a UDP socket
        // AFINET = IPv4, SOCK_DGRAM = UDP
        : sockfd_{socket(AF_INET, SOCK_DGRAM, 0)}
    {
        if (sockfd_ < 0)
        {
            throw std::runtime_error("Failed to create socket");
        }

        // 2. Set socket options (optional but good practise)
        // Picks setting level SOL_SOCKET, change the REUSEADDR setting and sets it to true (1)
        // This means that this program can take over the socket instantly rather than waiting
        int reuse = 1;
        if (setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
        {
            // We'll throw an error, but first we must release the socket!
            close(sockfd_); // Clean up before throwing
            throw std::runtime_error("Failed to set SO_REUSEADDR");
        }

        // Increase kernel send buffer
        int sendBufferSize = 4 * 1024 * 1024;
        if (setsockopt(sockfd_, SOL_SOCKET, SO_SNDBUF, &sendBufferSize, sizeof(sendBufferSize)) < 0)
        {
            std::cerr << "Warning: Could not increase socket buffer size. "
                      << "You might see packet drops under load." << std::endl;
        }

        // 3. Setup the destination address structure (UPDATING PROPERTIES OF addr_)
        std::memset(&addr_, 0, sizeof(addr_)); // Zero out addr_
        addr_.sin_family = AF_INET;            // We are using IPv4
        addr_.sin_port = htons(port);          // Use htons() to convert port to network-byte-order (normalisation)

        // convert IP string to binary
        if (inet_pton(AF_INET, multicast_ip.c_str(), &addr_.sin_addr) <= 0)
        {
            close(sockfd_);
            throw std::runtime_error("invlaid multicast IP address");
        }
    }

    void send(std::span<const uint8_t> data)
    {
        ssize_t bytesSent = sendto(sockfd_,
                                   data.data(), // Pointer to data
                                   data.size(), // Size of data
                                   0,           // Flags
                                   (struct sockaddr *)&addr_,
                                   sizeof(addr_));

        // Error handling
        if (bytesSent < 0)
        {
            // Check specifically for "Buffer Full" errors (ENOBUFS)
            if (errno == ENOBUFS || errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // THROW exception so the caller can catch it and retry!
                throw std::runtime_error("ENOBUFS");
            }
            else
            {
                // Hard error (bad network, bad address, etc.) - Just print
                std::cerr << "sendto failed: " << std::strerror(errno) << std::endl;
            }
        }
        // Partial packet send
        else if (bytesSent != static_cast<ssize_t>(data.size()))
        {
            std::cerr << "Partial packet sent. Sent " << bytesSent << " but expected "
                      << data.size() << std::endl;
        }
    }

    /**
     * Rule of Five: Destructor, Copy (assign / constructor), Move (assign / Constructor)
     * Delete copy, allow move
     * You can't copy a socket descriptor, but you can move ownership
     */

    // Destructor has to close socket
    ~UDPMulticastSender()
    {
        if (sockfd_ >= 0)
        {
            close(sockfd_);
        }
    }

    UDPMulticastSender(const UDPMulticastSender &) = delete;
    UDPMulticastSender &operator=(const UDPMulticastSender &) = delete;

    UDPMulticastSender(UDPMulticastSender &&other) noexcept
        : sockfd_(other.sockfd_), addr_(other.addr_)
    {
        other.sockfd_ = -1; // Invalidate the other descriptor after moving
    }

    UDPMulticastSender &operator=(UDPMulticastSender &&other) noexcept
    {
        if (this != &other)
        {
            if (sockfd_ >= 0)
            {
                close(sockfd_);
            }
            // Steal the others socket
            sockfd_ = other.sockfd_;
            addr_ = other.addr_;
            other.sockfd_ = -1; // Invalidate the others
        }
        return *this;
    }

private:
    int sockfd_;         // Socket file descriptor (Integer acts as ID for given socket)
    sockaddr_in addr_{}; // Desination address structure
};
#endif // MARKET_DATA_SYSTEM_UDP_SENDER_H