#pragma once

#include <algorithm>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>

namespace streamer
{

std::string getIPAddress(const std::string&);

class TimeMonitor
{
  public:
    explicit TimeMonitor(uint32_t);
    void printfps();
    void printtime();
    void printtime(const std::string&);

  private:
    std::chrono::steady_clock::time_point start;
    uint32_t clientnum{};
};

} // namespace streamer
