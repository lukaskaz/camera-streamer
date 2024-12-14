#include "helpers.hpp"

#include <arpa/inet.h>
#include <ifaddrs.h>

#include <chrono>
#include <cstring>
#include <iostream>

namespace streamer
{

std::string getIPAddress(const std::string& iface)
{
    std::string ip;
    struct ifaddrs* interfaces = NULL;

    if (getifaddrs(&interfaces) == 0)
    {
        struct ifaddrs* temp_addr = interfaces;
        while (temp_addr)
        {
            if (temp_addr->ifa_addr->sa_family == AF_INET)
            {
                if (strncmp(temp_addr->ifa_name, iface.c_str(), iface.size()) ==
                    0)
                {
                    ip = inet_ntoa(
                        ((struct sockaddr_in*)temp_addr->ifa_addr)->sin_addr);
                    break;
                }
            }
            temp_addr = temp_addr->ifa_next;
        }
    }
    freeifaddrs(interfaces);
    return ip;
}

void showFps()
{
    static std::chrono::steady_clock::time_point begin =
        std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point end =
        std::chrono::steady_clock::now();
    uint32_t diff_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - begin)
            .count();
    uint32_t fps = diff_ms == 0 ? 0 : 1000 / diff_ms;

    begin = end;
    std::cout << "          \r"
              << "[FPS: " << fps << "] " << std::flush;
}

} // namespace streamer
