#pragma once

#include <inference_engine.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <iostream>
#include <queue>
#include <ranges>

namespace ai::ncs
{

using namespace InferenceEngine;

template <typename T>
void matU8ToBlob(const cv::Mat& orig_image,
                 const InferenceEngine::Blob::Ptr& blob, int batchIndex = 0)
{
    InferenceEngine::SizeVector blobSize = blob->getTensorDesc().getDims();
    const size_t width = blobSize[3];
    const size_t height = blobSize[2];
    const size_t channels = blobSize[1];
    if (static_cast<size_t>(orig_image.channels()) != channels)
    {
        throw std::runtime_error(
            "The number of channels for net input and image must match");
    }
    InferenceEngine::LockedMemory<void> blobMapped =
        InferenceEngine::as<InferenceEngine::MemoryBlob>(blob)->wmap();
    T* blob_data = blobMapped.as<T*>();

    cv::Mat resized_image(orig_image);
    if (static_cast<int>(width) != orig_image.size().width ||
        static_cast<int>(height) != orig_image.size().height)
    {
        cv::resize(orig_image, resized_image, cv::Size(width, height));
    }

    int batchOffset = batchIndex * width * height * channels;

    if (channels == 1)
    {
        for (size_t h = 0; h < height; h++)
        {
            for (size_t w = 0; w < width; w++)
            {
                blob_data[batchOffset + h * width + w] =
                    resized_image.at<uchar>(h, w);
            }
        }
    }
    else if (channels == 3)
    {
        for (size_t c = 0; c < channels; c++)
        {
            for (size_t h = 0; h < height; h++)
            {
                for (size_t w = 0; w < width; w++)
                {
                    blob_data[batchOffset + c * width * height + h * width +
                              w] = resized_image.at<cv::Vec3b>(h, w)[c];
                }
            }
        }
    }
    else
    {
        throw std::runtime_error("Unsupported number of channels");
    }
}

struct BaseDetection
{
    static Core ie;
    ExecutableNetwork net;
    const std::string& model;
    const std::string& device;
    std::string topoName;
    Blob::Ptr inputBlob;
    std::string inputName;
    std::string outputName;
    const size_t maxBatch;
    class ReqGroup
    {
      private:
        std::queue<InferRequest::Ptr> idlereqs;
        std::queue<InferRequest::Ptr> pendingreqs;

        class Req
        {
          public:
            Req(ReqGroup* handler) : queue{handler->pendingreqs}
            {
                if (!handler->idlereqs.empty())
                {
                    ptr = handler->idlereqs.front();
                    handler->idlereqs.pop();
                }
            }

            Req(ReqGroup* handler, const auto query) : queue{handler->idlereqs}
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
            ~Req()
            {
                if (isvalid())
                    queue.push(ptr);
            }

            bool isvalid() const
            {
                return ptr != nullptr;
            }

            InferRequest::Ptr operator->()
            {
                return ptr;
            }

          private:
            std::queue<InferRequest::Ptr>& queue;
            InferRequest::Ptr ptr{nullptr};
        };

      public:
        explicit ReqGroup(ExecutableNetwork& net, size_t maxRequests)
        {
            uint32_t reqnum{1};
            std::cout << "Async requests in use: " << maxRequests << std::endl;
            std::ranges::for_each(
                std::views::iota(0, (int)maxRequests), [&, this](int) {
                    std::cout << "Deploying request #" << reqnum++ << std::endl;
                    idlereqs.push(net.CreateInferRequestPtr());
                });
        }
        ReqGroup() = default;
        ~ReqGroup() = default;

        std::shared_ptr<Req> getidle()
        {
            auto req = std::make_shared<Req>(this);
            return req->isvalid() ? req : nullptr;
        }

        std::shared_ptr<Req> getready()
        {
            auto req = std::make_shared<Req>(
                this, InferRequest::WaitMode::STATUS_ONLY);
            return req->isvalid() ? req : nullptr;
        }
    } reqgroup;

    BaseDetection(const std::string& model, const std::string& device,
                  const std::string& topoName) :
        model(model),
        device(device), topoName(topoName), maxBatch(1)

    {}

    virtual ~BaseDetection() = default;

    ExecutableNetwork* operator->()
    {
        return &net;
    }
    virtual CNNNetwork read(const Core& ie) = 0;

    virtual void enqueue(const cv::Mat& img)
    {
        if (auto req = reqgroup.getidle())
        {
            inputBlob = (*req)->GetBlob(inputName);
            matU8ToBlob<uint8_t>(img, inputBlob);
            (*req)->StartAsync();
        }
    }
};

} // namespace ai::ncs
