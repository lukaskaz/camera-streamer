#include "streaming/server.h"

#include "camera/interfaces/csi.hpp"
#include "helpers.hpp"
#include "log/interfaces/console.hpp"

#include <boost/program_options.hpp>
#include <opencv2/highgui.hpp>
// #include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <sstream>

int main(int argc, char* argv[])
{
    std::unordered_map<std::string, uint32_t> args = {
        {"videoCamNum", 0}, {"videoWidth", 800},  {"videoHeight", 600},
        {"videoFps", 60},   {"videoQuality", 80}, {"streamPort", 8001},
    };
    std::string module{"libstreamer"};
    auto loglvl = logging::type::info;
    auto logIf = logging::LogFactory::create<logging::console::Log>(loglvl);

    boost::program_options::options_description desc("Allowed options");
    desc.add_options()("help,h", "produce help message")(
        "logs,l", boost::program_options::value<uint32_t>(),
        "enable debug logs")("port,p",
                             boost::program_options::value<uint32_t>(),
                             "set streaming port")(
        "lccv,l", "use opencv support for new libcamera stack");

    boost::program_options::variables_map vm;
    boost::program_options::store(
        boost::program_options::parse_command_line(argc, argv, desc), vm);
    boost::program_options::notify(vm);

    if (vm.contains("help"))
    {
        std::ostringstream oss;
        oss << desc;
        logIf->log(logging::type::info, module, oss.str());
        return 0;
    }

    if (vm.contains("logs"))
    {
        logIf->log(logging::type::debug, module,
                   "Set logging level: " + vm.at("logs").as<int>());
    }

    auto& port = args["streamPort"];
    if (vm.contains("port"))
    {
        port = vm.at("port").as<uint32_t>();
        logIf->log(logging::type::debug, module, "Set streaming port: " + port);
    }

    if (vm.contains("lccv"))
    {
        logIf->log(logging::type::debug, module, "Using new libcam stack");
    }

    http::Server s(port);
    std::string ip = streamer::getIPAddress("eth0");
    if (!ip.empty())
    {
        logIf->log(logging::type::info, module,
                   "Reach streaming under url: http://" + ip + ":" +
                       std::to_string(port));
    }
    else
    {
        logIf->log(logging::type::critical, module,
                   "Cannot detect ip address for target interface, aborting");
        return 5;
    }

    auto camera = camera::Factory::create<camera::csi::Camera>(
        logIf, {args["videoCamNum"], args["videoWidth"], args["videoHeight"],
                args["videoFps"]});

    s.get(
         "/img",
         [quality = args["videoQuality"], camera, logIf, &module](auto,
                                                                  auto res) {
             res.headers.push_back("Connection: close");
             res.headers.push_back("Max-Age: 0");
             res.headers.push_back("Expires: 0");
             res.headers.push_back("Cache-Control: no-cache, private");
             res.headers.push_back("Pragma: no-cache");
             res.headers.push_back(
                 "Content-Type: multipart/x-mixed-replace;boundary=--boundary");

             if (!res.send_header())
             {
                 return;
             }

             static uint32_t clinetnum{1};
             auto fps{streamer::FpsMonitor{clinetnum++}};
             camera->Processable::subscribe(
                 streamer::Processor<cv::Mat>::create(
                     [quality, logIf, &module, &res, &fps](auto& frame) {
                         cv::putText(frame, "#EXAMINED#", cv::Point(10, 30),
                                     cv::FONT_HERSHEY_DUPLEX, 1.0,
                                     CV_RGB(0, 0, 255), 2);
                     }));
             camera->Observable::subscribe(streamer::Observer<cv::Mat>::create(
                 [quality, logIf, &module, &res, &fps](auto& frame) {
                     std::vector<uchar> buffer;
                     cv::imencode(".jpg", frame, buffer,
                                  {cv::IMWRITE_JPEG_QUALITY, (int32_t)quality});
                     std::string image(buffer.begin(), buffer.end());

                     if (!res.send_msg("--boundary\r\n"
                                       "Content-Type: image/jpeg\r\n"
                                       "Content-Length: " +
                                       std::to_string(image.size()) +
                                       "\r\n\r\n" + std::move(image)))
                     {
                         throw std::runtime_error(
                             "Cannot stream image content");
                     }

                     logIf->log(logging::type::debug, module,
                                "New frame was streamed");
                     fps.print();
                 }));
             camera->run();
         })
        .get("/", [ip, port, width = args["videoWidth"],
                   height = args["videoHeight"]](auto, auto res) {
            res >> "<html>"
                   "    <body>"
                   "        <h1>Camera streaming</h1>"
                   "        <img src='http://" +
                       ip + ":" + std::to_string(port) + "/img'/ width='" +
                       std::to_string(width) + "' height='" +
                       std::to_string(height) +
                       "'>"
                       "    </body>"
                       "</html>";
        });

    try
    {
        s.listen();
    }
    catch (std::runtime_error& ex)
    {
        logIf->log(logging::type::error, module,
                   "[Runtime error] " + std::string(ex.what()));
    }
    catch (std::exception& ex)
    {
        logIf->log(logging::type::error, module,
                   "[Generic error] " + std::string(ex.what()));
    }
    catch (...)
    {
        logIf->log(logging::type::error, module, "[Undefined error]");
    }

    return 0;
}
