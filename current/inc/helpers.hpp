#pragma once

#include <algorithm>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace streamer
{

std::string getIPAddress(const std::string&);

class FpsMonitor
{
  public:
    explicit FpsMonitor(uint32_t);
    void print();

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
        if (!observers.erase(obs))
        {
            throw std::runtime_error(
                "Trying to unsubscribe not existing observer");
        }
    }

  private:
    std::unordered_set<std::shared_ptr<Observer<T>>> observers;
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
    void process(T& param)
    {
        std::ranges::for_each(processors,
                              [&param](auto prc) { (*prc)(param); });
    }

    void subscribe(std::shared_ptr<Processor<T>> prc)
    {
        if (!processors.insert(prc).second)
        {
            throw std::runtime_error(
                "Trying to subscribe already existing observer");
        }
    }

    void unsubscribe(std::shared_ptr<Processor<T>> prc)
    {
        if (!processors.erase(prc))
        {
            throw std::runtime_error(
                "Trying to unsubscribe not existing observer");
        }
    }

  private:
    std::unordered_set<std::shared_ptr<Processor<T>>> processors;
};

} // namespace streamer
