#include "ai/ncs/detectors/face.hpp"

#include "ai/ncs/common.hpp"
#include "ai/ncs/factory.hpp"
#include "streamer/helpers.hpp"

namespace ai::ncs::face
{

struct Detector::Handler : public BaseDetection
{
    Handler(Detector* iface, const std::string& model,
            const std::string& devicename) :
        BaseDetection(model, devicename, "face detector"),
        iface{iface}, maxProposalCount(0), objectSize(0)
    {
        auto cnn = read(ie);
        net = ie.LoadNetwork(cnn, device);
        reqgroup = ReqGroup(net, 3);
    }

    void process(const cv::Mat& orig, cv::Mat& mod)
    {
        const auto& results = getResults(orig);
        processFrame(mod, results);
        iface->execute(results);
    }

  private:
    Detector* iface;
    int maxProposalCount{};
    int objectSize{};
    float width{};
    float height{};
    double confidtthreshold{0.75};

    void enqueue(const cv::Mat& img) override
    {
        height = static_cast<float>(img.rows);
        width = static_cast<float>(img.cols);
        BaseDetection::enqueue(img);
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

    std::vector<Result> getResults(const cv::Mat& img)
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
        enqueue(img);
        return results;
    }

    void processFrame(cv::Mat& img, const std::vector<Result>& results)
    {
        std::ranges::for_each(results, [&img](const auto& result) {
            auto confidence = (int32_t)(result.confidence * 100.);
            int32_t textwidth = confidence < 100 ? 125 : 145;
            cv::rectangle(img, result.location, CV_RGB(0, 0, 255), 2);
            cv::rectangle(img,
                          cv::Point(result.location.x - 1, result.location.y),
                          cv::Point(result.location.x + textwidth,
                                    result.location.y - 20),
                          CV_RGB(0, 0, 255), cv::FILLED);
            cv::putText(img, "Face: " + std::to_string(confidence) + "%",
                        cv::Point(result.location.x, result.location.y - 3),
                        cv::FONT_HERSHEY_DUPLEX, 0.6, CV_RGB(255, 255, 255), 1);
        });
    }
};

Detector::Detector(const std::string& model, const std::string& devicename) :
    handler{std::make_unique<Handler>(this, model, devicename)}
{}

Detector::~Detector() = default;

void Detector::process(const cv::Mat& img, std::vector<cv::Mat>& out)
{
    out.push_back(std::move(img.clone()));
    handler->process(img, out.back());
    ai::ncs::ProcessorIf::process(img, out);
}

} // namespace ai::ncs::face
