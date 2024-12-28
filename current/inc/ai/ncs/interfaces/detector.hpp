#pragma once

#include "ai/ncs/helpers.hpp"

#include <opencv2/core/mat.hpp>

#include <vector>

namespace ai::ncs
{

struct Result
{
    int label;
    float confidence;
    cv::Rect location;
};

class DetectorIf : public ai::Executable<std::vector<Result>>
{
  public:
    virtual ~DetectorIf() = default;
    virtual void process(cv::Mat&) = 0;
};

} // namespace ai::ncs
