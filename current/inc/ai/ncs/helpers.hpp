#pragma once

#include <opencv2/core/mat.hpp>

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <unordered_set>

namespace ai
{

template <typename T>
class Executor
{
  public:
    using Func = std::function<void(const T&)>;
    static std::shared_ptr<Executor<T>> create(const Func& func)
    {
        return std::shared_ptr<Executor<T>>(new Executor<T>(func));
    }

    void operator()(const T& param)
    {
        func(param);
    }

  private:
    Executor(const Func& func) : func{func}
    {}
    Func func;
};

template <typename T>
class Executable
{
  public:
    void execute(const T& param)
    {
        cleanup();
        std::ranges::for_each(executors,
                              [&param](auto exec) { (*exec)(param); });
    }

    void subscribe(std::shared_ptr<Executor<T>> exec)
    {
        if (!executors.insert(exec).second)
        {
            throw std::runtime_error(
                "Trying to subscribe already existing executor");
        }
    }

    void unsubscribe(std::shared_ptr<Executor<T>> exec)
    {
        if (!executors.contains(exec))
        {
            throw std::runtime_error(
                "Trying to unsubscribe not existing executor");
        }
        if (!unsubscribed.insert(exec).second)
        {
            throw std::runtime_error(
                "Trying to unsubscribe already added executor");
        }
    }

    bool empty() const
    {
        return executors.empty();
    }

  private:
    std::unordered_set<std::shared_ptr<Executor<T>>> executors, unsubscribed;

    void cleanup()
    {
        if (!unsubscribed.empty())
        {
            std::ranges::for_each(unsubscribed,
                                  [this](auto exec) { executors.erase(exec); });
            unsubscribed.clear();
        }
    }
};

} // namespace ai
