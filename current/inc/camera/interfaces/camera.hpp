#pragma once

#include "helpers.hpp"

#include <opencv2/core/mat.hpp>

namespace camera
{
class CameraIf : public streamer::Observable<cv::Mat>
{
  public:
    virtual ~CameraIf() = default;
    virtual void run() = 0;
};
} // namespace camera
