#pragma once

#include "ai/ncs/interfaces/detector.hpp"

#include <memory>

namespace ai::ncs
{
class Factory
{
  public:
    template <typename T>
    static std::shared_ptr<DetectorIf> create(std::string& model,
                                              const std::string& dev)
    {
        return std::shared_ptr<T>(new T(model, dev));
    }
};
} // namespace ai::ncs
