#include "camera/interfaces/usb.hpp"

#include <opencv2/highgui.hpp>

namespace camera::usb
{

struct Camera::Handler
{
  public:
    Handler(std::shared_ptr<logging::LogIf> logIf,
            const std::array<uint32_t, 4>& params) :
        logIf{logIf}
    {
        auto [id, width, height, fps] = params;
        std::string camid{"/dev/video" + std::to_string(id)};
        camera = cv::VideoCapture(camid);
        if (!camera.isOpened())
        {
            throw std::runtime_error("Cannot open videocapture: " + camid);
        }
        camera.set(cv::CAP_PROP_FRAME_WIDTH, width);
        camera.set(cv::CAP_PROP_FRAME_HEIGHT, height);
        camera.set(cv::CAP_PROP_FPS, fps);
        camera.set(cv::CAP_PROP_BUFFERSIZE, 1);
        // camera.set(cv::CAP_PROP_AUTOFOCUS, true);
        // camera.set(cv::CAP_PROP_FOURCC,
        //         cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
        camera.set(cv::CAP_PROP_FOURCC,
                   cv::VideoWriter::fourcc('R', 'G', 'B', '3'));
        int codecRaw = static_cast<int>(camera.get(cv::CAP_PROP_FOURCC));
        char codec[] = {(char)(codecRaw & 0xFF),
                        (char)((codecRaw & 0xFF00) >> 8),
                        (char)((codecRaw & 0xFF0000) >> 16),
                        (char)((codecRaw & 0xFF000000) >> 24), 0};

        log(logging::type::info,
            "Streaming camera: " + camid + " [" +
                std::to_string(camera.get(cv::CAP_PROP_FRAME_WIDTH)) + "x" +
                std::to_string(camera.get(cv::CAP_PROP_FRAME_HEIGHT)) +
                "] w/ FPS: " + std::to_string(camera.get(cv::CAP_PROP_FPS)));
        log(logging::type::info,
            "Codec: " + std::string(codec) + ", buffer size: " +
                std::to_string(camera.get(cv::CAP_PROP_BUFFERSIZE)));
    }
    ~Handler()
    {}

    bool getframe(cv::Mat& frame)
    {
        return camera.read(frame);
    }

  private:
    std::string module{"libcamerausb"};
    cv::VideoCapture camera;
    std::shared_ptr<logging::LogIf> logIf;

    void log(logging::type type, const std::string& msg) const
    {
        if (logIf)
        {
            logIf->log(type, module, msg);
        }
    }
};

Camera::Camera(std::shared_ptr<logging::LogIf> logIf,
               const std::array<uint32_t, 4>& params) :
    handler{std::make_unique<Handler>(logIf, params)}
{}

Camera::~Camera() = default;

bool Camera::getframe(cv::Mat& frame)
{
    return handler->getframe(frame);
}

} // namespace camera::usb