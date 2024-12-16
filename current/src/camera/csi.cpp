#include "camera/interfaces/csi.hpp"

#include <lccv.hpp>

namespace camera::csi
{

struct Camera::Handler
{
  public:
    Handler(Camera* iface, std::shared_ptr<logging::LogIf> logIf,
            const std::array<uint32_t, 4>& params) :
        iface{iface},
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

    void run()
    {
        if (running)
            while (true)
                ;
        running = true;
        while (true)
        {
            cv::Mat frame;
            if (camera.getVideoFrame(frame, timeoutms))
            {
                iface->process(frame);
                iface->notify(frame);
            }
            else
            {
                log(logging::type::warning,
                    "Cannot get frame within expected timeslot");
            }
        }
    }

  private:
    std::string module{"libcameracsi"};
    lccv::PiCamera camera;
    Camera* iface;
    std::shared_ptr<logging::LogIf> logIf;
    uint32_t timeoutms{100};
    std::atomic<bool> running{false};

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
    handler{std::make_unique<Handler>(this, logIf, params)}
{}

Camera::~Camera() = default;

void Camera::run()
{
    return handler->run();
}

} // namespace camera::csi
