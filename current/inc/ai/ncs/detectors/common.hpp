#pragma once

#include "ai/ncs/helpers.hpp"

#include <inference_engine.hpp>
#include <opencv2/imgproc.hpp>

#include <queue>

namespace ai::ncs
{

using namespace InferenceEngine;

template <typename T>
void matU8ToBlob(const cv::Mat& orig_image, const Blob::Ptr& blob,
                 int batchIndex = 0)
{
    SizeVector blobSize = blob->getTensorDesc().getDims();
    const size_t width = blobSize[3];
    const size_t height = blobSize[2];
    const size_t channels = blobSize[1];
    if (static_cast<size_t>(orig_image.channels()) != channels)
    {
        throw std::runtime_error(
            "The number of channels for net input and image must match");
    }
    LockedMemory<void> blobMapped = as<MemoryBlob>(blob)->wmap();
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
  public:
    BaseDetection(const std::string&, const std::string&, const std::string&);
    virtual ~BaseDetection();
    virtual CNNNetwork read(const Core& ie) = 0;
    virtual void enqueue(const cv::Mat&);
    ExecutableNetwork* operator->();

  protected:
    static Core ie;
    ExecutableNetwork net;
    const std::string& model;
    const std::string& device;
    std::string topoName;
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
            Req(ReqGroup*);
            Req(ReqGroup*, InferRequest::WaitMode);
            ~Req();

            InferRequest::Ptr operator->();
            bool isvalid() const;

          private:
            std::queue<InferRequest::Ptr>& queue;
            InferRequest::Ptr ptr{nullptr};
        };

      public:
        ReqGroup();
        explicit ReqGroup(ExecutableNetwork&, int32_t);
        ~ReqGroup();

        std::shared_ptr<Req> getidle();
        std::shared_ptr<Req> getready();
    } reqgroup;

  private:
    Blob::Ptr inputBlob;
};

} // namespace ai::ncs
