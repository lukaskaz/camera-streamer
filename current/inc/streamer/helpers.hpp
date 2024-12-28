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
    void printtime();
    void printfps();

  private:
    std::chrono::steady_clock::time_point start;
    uint32_t clientnum{};
};

template <typename T>
class Observer
{
  public:
    using Func = std::function<void(const T&)>;
    static std::shared_ptr<Observer<T>> create(const Func& func)
    {
        return std::shared_ptr<Observer<T>>(new Observer<T>(func));
    }

    void operator()(const T& param)
    {
        func(param);
    }

  private:
    Observer(const Func& func) : func{func}
    {}
    Func func;
};

template <typename T>
class Observable
{
  public:
    void notify(const T& param)
    {
        cleanup();
        std::ranges::for_each(observers, [&param](auto obs) { (*obs)(param); });
    }

    void subscribe(std::shared_ptr<Observer<T>> obs)
    {
        if (!observers.insert(obs).second)
        {
            throw std::runtime_error(
                "Trying to subscribe already existing observer");
        }
    }

    void unsubscribe(std::shared_ptr<Observer<T>> obs)
    {
        if (!observers.contains(obs))
        {
            throw std::runtime_error(
                "Trying to unsubscribe not existing observer");
        }
        if (!unsubscribed.insert(obs).second)
        {
            throw std::runtime_error(
                "Trying to unsubscribe already added observer");
        }
    }

    bool empty() const
    {
        return observers.empty();
    }

  private:
    std::unordered_set<std::shared_ptr<Observer<T>>> observers, unsubscribed;

    void cleanup()
    {
        if (!unsubscribed.empty())
        {
            std::ranges::for_each(unsubscribed,
                                  [this](auto obs) { observers.erase(obs); });
            unsubscribed.clear();
        }
    }
};

template <typename T>
class Processor
{
  public:
    using Func = std::function<void(T&)>;
    static std::shared_ptr<Processor<T>> create(const Func& func)
    {
        return std::shared_ptr<Processor<T>>(new Processor<T>(func));
    }

    void operator()(T& param)
    {
        func(param);
    }

  private:
    Processor(const Func& func) : func{func}
    {}
    Func func;
};

template <typename T>
class Processable
{
  public:
    using processordata = std::pair<uint32_t, std::shared_ptr<Processor<T>>>;
    void process(T& param)
    {
        cleanup();
        std::ranges::for_each(processors,
                              [&param](auto prc) { (*prc.second)(param); });
    }

    void subscribe(const processordata& data)
    {
        if (!processors.emplace(data).second)
        {
            throw std::runtime_error(
                "Trying to subscribe already existing processor");
        }
    }

    void unsubscribe(const processordata& data)
    {
        if (!processors.contains(data))
        {
            throw std::runtime_error(
                "Trying to unsubscribe not existing processor");
        }
        if (!unsubscribed.insert(data).second)
        {
            throw std::runtime_error(
                "Trying to unsubscribe already added processor");
        }
    }

    bool empty() const
    {
        return processors.empty();
    }

  private:
    std::map<uint32_t, std::shared_ptr<Processor<T>>> processors, unsubscribed;

    void cleanup()
    {
        if (!unsubscribed.empty())
        {
            std::ranges::for_each(unsubscribed, [this](auto prc) {
                processors.erase(prc.first);
            });
            unsubscribed.clear();
        }
    }
};

} // namespace streamer
