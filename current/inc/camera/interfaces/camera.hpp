#pragma once

#include <opencv2/core/mat.hpp>

namespace camera
{
class CameraIf
{
  public:
    virtual ~CameraIf() = default;
    virtual bool getframe(cv::Mat&) = 0;
};
} // namespace camera
