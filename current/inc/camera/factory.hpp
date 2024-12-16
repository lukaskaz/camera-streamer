#pragma once

#include "camera/interfaces/camera.hpp"
#include "log/interfaces/logging.hpp"

#include <array>
#include <memory>

namespace camera
{
class Factory
{
  public:
    template <typename T>
    static std::shared_ptr<CameraIf>
        create(std::shared_ptr<logging::LogIf> logIf,
               const std::array<uint32_t, 4>& params)
    {
        return std::shared_ptr<T>(new T(logIf, params));
    }
};
} // namespace camera
