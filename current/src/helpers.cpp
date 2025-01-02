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

TimeMonitor::TimeMonitor() :
    clientnum{++instance}, start{std::chrono::steady_clock::now()}
{}
uint32_t TimeMonitor::instance = 0;

void TimeMonitor::print(const std::string& name)
{
    auto current = std::chrono::steady_clock::now();
    uint32_t diffms =
        std::chrono::duration_cast<std::chrono::milliseconds>(current - start)
            .count();
    start = current;
    uint32_t fps = diffms == 0 ? 0 : 1000 / diffms;

    std::cout << "\r";
    for (uint32_t tabsnum = 1; tabsnum < clientnum; tabsnum++)
        std::cout << "\t\t\t\t";
    for (uint32_t rownum = 1, rowsmax = 30; rownum <= rowsmax; rownum++)
        std::cout << " ";
    std::cout << "\r";
    for (uint32_t tabsnum = 1; tabsnum < clientnum; tabsnum++)
        std::cout << "\t\t\t\t";
    std::cout << "[FPS/LAT_" << clientnum << "@" << name << ": " << fps << "/"
              << diffms << "] " << std::flush;
}

} // namespace streamer
