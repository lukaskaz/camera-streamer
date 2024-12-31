#include "ai/ncs/common.hpp"

#include <algorithm>
#include <iostream>
#include <ranges>

namespace ai::ncs
{

BaseDetection::BaseDetection(const std::string& model,
                             const std::string& device,
                             const std::string& topoName) :
    model(model),
    device(device), topoName(topoName + "detector"), maxBatch(1)
{}

BaseDetection::~BaseDetection() = default;

BaseDetection::ReqGroup::Req::Req(BaseDetection::ReqGroup* handler) :
    queue{handler->pendingreqs}
{
    if (!handler->idlereqs.empty())
    {
        ptr = handler->idlereqs.front();
        handler->idlereqs.pop();
    }
}

BaseDetection::ReqGroup::Req::Req(BaseDetection::ReqGroup* handler,
                                  InferRequest::WaitMode query) :
    queue{handler->idlereqs}
{
    if (!handler->pendingreqs.empty())
    {
        auto ptr = handler->pendingreqs.front();
        if (ptr->Wait(query) == StatusCode::OK)
        {
            handler->pendingreqs.pop();
            this->ptr = ptr;
        }
    }
}

BaseDetection::ReqGroup::Req::~Req()
{
    if (isvalid())
        queue.push(ptr);
}

InferRequest::Ptr BaseDetection::ReqGroup::Req::operator->()
{
    return ptr;
}

bool BaseDetection::ReqGroup::Req::isvalid() const
{
    return ptr != nullptr;
}

BaseDetection::ReqGroup::ReqGroup(ExecutableNetwork& net, int32_t reqs)
{
    uint32_t reqnum{1};
    std::cout << "Num of async requests to use: " << reqs << std::endl;
    std::ranges::for_each(std::views::iota(0, reqs), [&, this](uint32_t) {
        std::cout << "Deploying request #" << reqnum++ << std::endl;
        idlereqs.push(net.CreateInferRequestPtr());
    });
}

std::shared_ptr<BaseDetection::ReqGroup::Req> BaseDetection::ReqGroup::getidle()
{
    auto req = std::make_shared<Req>(this);
    return req->isvalid() ? req : nullptr;
}

std::shared_ptr<BaseDetection::ReqGroup::Req>
    BaseDetection::ReqGroup::getready()
{
    auto req = std::make_shared<Req>(this, InferRequest::WaitMode::STATUS_ONLY);
    return req->isvalid() ? req : nullptr;
}

BaseDetection::ReqGroup::ReqGroup() = default;
BaseDetection::ReqGroup::~ReqGroup() = default;

ExecutableNetwork* BaseDetection::operator->()
{
    return &net;
}

void BaseDetection::enqueue(const cv::Mat& img)
{
    if (auto req = reqgroup.getidle())
    {
        inputBlob = (*req)->GetBlob(inputName);
        matU8ToBlob<uint8_t>(img, inputBlob);
        (*req)->StartAsync();
    }
}

Core ai::ncs::BaseDetection::ie;

} // namespace ai::ncs
