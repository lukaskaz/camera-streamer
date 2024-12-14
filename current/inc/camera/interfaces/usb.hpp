#pragma once

#include "camera/factory.hpp"
#include "log/interfaces/logging.hpp"

#include <array>
#include <cstdint>
#include <memory>

namespace camera::usb
{

class Camera : public CameraIf
{
  public:
    ~Camera();
    bool getframe(cv::Mat&) override;

  private:
    friend class camera::Factory;
    Camera(std::shared_ptr<logging::LogIf>, const std::array<uint32_t, 4>&);

  private:
    struct Handler;
    std::unique_ptr<Handler> handler;
};

} // namespace camera::usb