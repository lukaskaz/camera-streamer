#include "ai/ncs/detectors/person.hpp"

#include "ai/ncs/factory.hpp"
#include "streamer/helpers.hpp"

#include <inference_engine.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <iostream>
#include <queue>
#include <ranges>

namespace ai::ncs::person
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
    Core ie;
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

    virtual void enqueue(const cv::Mat& frame)
    {
        if (auto req = reqgroup.getidle())
        {
            inputBlob = (*req)->GetBlob(inputName);
            matU8ToBlob<uint8_t>(frame, inputBlob);
            (*req)->StartAsync();
        }
    }
};

struct Detector::Handler : public BaseDetection
{
    Handler(Detector* iface, const std::string& model,
            const std::string& devicename) :
        BaseDetection(model, devicename, "person detector"),
        iface{iface}, maxProposalCount(0), objectSize(0)
    {
        auto cnn = read(ie);
        net = ie.LoadNetwork(cnn, device);
        reqgroup = ReqGroup(net, 3);
    }

    void process(cv::Mat& frame)
    {
        const auto& results = getResults(frame);
        processFrame(frame, results);
        iface->execute(results);
    }

  private:
    Detector* iface;
    int maxProposalCount{};
    int objectSize{};
    float width{};
    float height{};
    double confidtthreshold{0.75};

    void enqueue(const cv::Mat& frame) override
    {
        height = static_cast<float>(frame.rows);
        width = static_cast<float>(frame.cols);
        BaseDetection::enqueue(frame);
    }

    CNNNetwork read(const Core& ie) override
    {
        std::cout << "Loading network files for " << topoName << std::endl;
        /** Read network model **/
        auto network = ie.ReadNetwork(model);
        /** Set batch size to 1 **/
        std::cout << "Batch size is forced to  1" << std::endl;
        network.setBatchSize(maxBatch);
        // -----------------------------------------------------------------------------------------------------

        /** SSD-based network should have one input and one output **/
        // ---------------------------Check inputs
        // ------------------------------------------------------
        std::cout << "Checking " << topoName << " inputs" << std::endl;
        InputsDataMap inputInfo(network.getInputsInfo());
        if (inputInfo.size() != 1)
        {
            throw std::logic_error(topoName +
                                   " network should have only one input");
        }
        InputInfo::Ptr& inputInfoFirst = inputInfo.begin()->second;
        inputInfoFirst->setPrecision(Precision::U8);
        inputInfoFirst->getInputData()->setLayout(Layout::NCHW);
        inputName = inputInfo.begin()->first;
        // -----------------------------------------------------------------------------------------------------

        // ---------------------------Check outputs
        // ------------------------------------------------------
        std::cout << "Checking " << topoName << " outputs" << std::endl;
        OutputsDataMap outputInfo(network.getOutputsInfo());
        if (outputInfo.size() != 1)
        {
            throw std::logic_error(topoName +
                                   " network should have only one output");
        }
        DataPtr& _output = outputInfo.begin()->second;
        const SizeVector outputDims = _output->getTensorDesc().getDims();
        outputName = outputInfo.begin()->first;
        maxProposalCount = outputDims[2];
        objectSize = outputDims[3];
        if (objectSize != 7)
        {
            throw std::logic_error("Output should have 7 as a last dimension");
        }
        if (outputDims.size() != 4)
        {
            throw std::logic_error("Incorrect output dimensions for SSD");
        }
        _output->setPrecision(Precision::FP32);
        _output->setLayout(Layout::NCHW);

        std::cout << "Loading " << topoName << " model to the " << device
                  << " device" << std::endl;
        return network;
    }

    void reshape(const InferenceEngine::Core& ie, size_t rows, size_t cols)
    {
        // --------------------------- Resize network to match image sizes and
        // given batch----------------------
        auto network = ie.ReadNetwork(model);
        auto input_shapes = network.getInputShapes();
        auto [input_name, input_shape] = *input_shapes.begin();
        // cv::Mat image = cv::imread(input_image_path);
        input_shape[0] = maxBatch;
        input_shape[2] = rows;
        input_shape[3] = cols;
        input_shapes[input_name] = input_shape;
        std::cout << "Resizing network to the image size = [" << rows << "x"
                  << cols << "] "
                  << "with batch = " << maxBatch << std::endl;
        network.reshape(input_shapes);
        // detector.net = ie.LoadNetwork(network, deviceName);
        //   -----------------------------------------------------------------------------------------------------
    }

    cv::Rect getArea(const float* detections, int idx)
    {
        auto x = (int)(detections[idx * objectSize + 3] * width),
             y = (int)(detections[idx * objectSize + 4] * height),
             w = (int)(detections[idx * objectSize + 5] * width - x),
             h = (int)(detections[idx * objectSize + 6] * height - y);
        return cv::Rect{x, y, w, h};
    }

    std::vector<Result> getResults(cv::Mat& frame)
    {
        static std::vector<Result> results;
        if (auto req = reqgroup.getready())
        {
            static streamer::TimeMonitor timecheck(1);
            timecheck.printtime();
            results.clear();
            LockedMemory<const void> outputMapped =
                as<MemoryBlob>((*req)->GetBlob(outputName))->rmap();
            const auto detections = outputMapped.as<float*>();
            std::ranges::for_each(
                std::views::iota(0, maxProposalCount), [&](auto i) {
                    // in case of batch, end of detections if image_id < 0
                    if (detections[i * objectSize + 0] >= 0)
                    {
                        if (auto confid = detections[i * objectSize + 2];
                            confid >= confidtthreshold)
                        {
                            auto label = (int)(detections[i * objectSize + 1]);
                            auto area = getArea(detections, i);
                            results.emplace_back(label, confid, area);
                        }
                    }
                });
        }
        enqueue(frame);
        return results;
    }

    void processFrame(cv::Mat& frame, const std::vector<Result>& results)
    {
        std::ranges::for_each(results, [&frame](const auto& result) {
            auto confidence = (int32_t)(result.confidence * 100.);
            int32_t textwidth = confidence < 100 ? 125 : 145;
            cv::rectangle(frame, result.location, CV_RGB(0, 0, 255), 2);
            cv::rectangle(frame,
                          cv::Point(result.location.x - 1, result.location.y),
                          cv::Point(result.location.x + textwidth,
                                    result.location.y - 20),
                          CV_RGB(0, 0, 255), cv::FILLED);
            cv::putText(frame, "Human: " + std::to_string(confidence) + "%",
                        cv::Point(result.location.x, result.location.y - 3),
                        cv::FONT_HERSHEY_DUPLEX, 0.6, CV_RGB(255, 255, 255), 1);
        });
    }
};

Detector::Detector(const std::string& model, const std::string& devicename) :
    handler{std::make_unique<Handler>(this, model, devicename)}
{}

Detector::~Detector() = default;

void Detector::process(cv::Mat& frame)
{
    handler->process(frame);
}

} // namespace ai::ncs::person
