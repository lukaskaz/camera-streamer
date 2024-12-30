#pragma once

#include "ai/ncs/helpers.hpp"

#include <opencv2/core/mat.hpp>

#include <memory>
#include <vector>

namespace ai::ncs
{

struct Result
{
    int label;
    float confidence;
    cv::Rect location;
};

class ProcessorIf : public ai::Executable<std::vector<Result>>
{
  public:
    ProcessorIf() = default;
    virtual ~ProcessorIf() = default;

    virtual std::shared_ptr<ProcessorIf>
        setnext(std::shared_ptr<ProcessorIf> ptr)
    {
        nextptr = ptr;
        return nextptr;
    }

    virtual void process(const cv::Mat& img, std::vector<cv::Mat>& output)
    {
        if (nextptr)
        {
            nextptr->process(img, output);
        }
    }

  private:
    std::shared_ptr<ProcessorIf> nextptr;
};

} // namespace ai::ncs
