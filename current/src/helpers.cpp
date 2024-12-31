#include "streamer/helpers.hpp"

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

TimeMonitor::TimeMonitor(uint32_t num) :
    start{std::chrono::steady_clock::now()}, clientnum{num}
{}

void TimeMonitor::printtime()
{
    auto current = std::chrono::steady_clock::now();
    uint32_t diffms =
        std::chrono::duration_cast<std::chrono::milliseconds>(current - start)
            .count();
    start = current;

    // std::cout << "\r";
    // for (uint32_t tabsnum = 0; tabsnum < clientnum; tabsnum++)
    //     std::cout << "\t\t";
    // std::cout << "          \r";
    // for (uint32_t tabsnum = 0; tabsnum < clientnum; tabsnum++)
    //     std::cout << "\t\t";
    // std::cout << "[TIM_" << clientnum << ": " << diffms << "] " <<
    // std::flush;
    std::cout << "[TIM_" << clientnum << ": " << diffms << "]\n";
}

void TimeMonitor::printtime(const std::string& name)
{
    auto current = std::chrono::steady_clock::now();
    uint32_t diffms =
        std::chrono::duration_cast<std::chrono::milliseconds>(current - start)
            .count();
    start = current;

    // std::cout << "\r";
    // for (uint32_t tabsnum = 0; tabsnum < clientnum; tabsnum++)
    //     std::cout << "\t\t";
    // std::cout << "          \r";
    // for (uint32_t tabsnum = 0; tabsnum < clientnum; tabsnum++)
    //     std::cout << "\t\t";
    // std::cout << "[TIM_" << name << ": " << diffms << "] " << std::flush;
    std::cout << "[TIM_" << name << ": " << diffms << "]\n";
}

void TimeMonitor::printfps()
{
    auto current = std::chrono::steady_clock::now();
    uint32_t diffms =
        std::chrono::duration_cast<std::chrono::milliseconds>(current - start)
            .count();
    start = current;
    uint32_t fps = diffms == 0 ? 0 : 1000 / diffms;

    // std::cout << "\r";
    // for (uint32_t tabsnum = 1; tabsnum < clientnum; tabsnum++)
    //     std::cout << "\t\t";
    // std::cout << "          \r";
    // for (uint32_t tabsnum = 1; tabsnum < clientnum; tabsnum++)
    //     std::cout << "\t\t";
    // std::cout << "[FPS_" << clientnum << ": " << fps << "] " << std::flush;
    std::cout << "[FPS_" << clientnum << ": " << fps << "]\n";
}

} // namespace streamer
