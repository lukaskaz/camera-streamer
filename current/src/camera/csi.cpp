#include "camera/interfaces/csi.hpp"

#include <lccv.hpp>

namespace camera::csi
{

struct Camera::Handler
{
  public:
    Handler(std::shared_ptr<logging::LogIf> logIf,
            const std::array<uint32_t, 4>& params) :
        logIf{logIf}
    {
        auto [id, width, heigh, fps] = params;
        camera.options->camera = id;
        camera.options->video_width = width;
        camera.options->video_height = heigh;
        camera.options->framerate = fps;
        camera.options->verbose = false;
        camera.startVideo();
    }
    ~Handler()
    {
        camera.stopVideo();
    }

    bool getframe(cv::Mat& frame)
    {
        return camera.getVideoFrame(frame, timeoutms);
    }

  private:
    std::string module{"libcameracsi"};
    lccv::PiCamera camera;
    std::shared_ptr<logging::LogIf> logIf;
    uint32_t timeoutms{100};

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

} // namespace camera::csi