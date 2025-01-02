#pragma once

#include "ai/ncs/factory.hpp"

namespace ai::ncs::person
{

class Detector : public ProcessorIf
{
  public:
    ~Detector();
    void process(const cv::Mat&, std::vector<cv::Mat>&) override;

  private:
    friend class ai::ncs::Factory;
    Detector(const std::string&, const std::string&);

  private:
    struct Handler;
    std::unique_ptr<Handler> handler;
};

} // namespace ai::ncs::person
