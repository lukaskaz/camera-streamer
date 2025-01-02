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
    explicit TimeMonitor();
    void print(const std::string& = "");

  private:
    static uint32_t instance;
    uint32_t clientnum;
    std::chrono::steady_clock::time_point start;
};

} // namespace streamer
