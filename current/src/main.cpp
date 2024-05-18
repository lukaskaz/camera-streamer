#include "server.h"

#include <ifaddrs.h>

#include <boost/program_options.hpp>
#include <lccv.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include <chrono>
#include <iostream>
#include <thread>
#include <unordered_map>

enum LOGLEVEL
{
    ERROR = 0,
    WARN,
    INFO,
    DEBUG
};

constexpr LOGLEVEL usedLogLevel = LOGLEVEL::INFO;

template <typename T>
void printLog(LOGLEVEL lvl, const std::string& id, const T& msg)
{
    static const std::unordered_map<int, std::string> logToName = {
        {LOGLEVEL::ERROR, "ERR"},
        {LOGLEVEL::WARN, "WARN"},
        {LOGLEVEL::INFO, "INFO"},
        {LOGLEVEL::DEBUG, "DBG"},
    };

    if (usedLogLevel >= lvl)
    {
        std::cerr << "[" << logToName.at(lvl) << ":" << id << "] " << msg
                  << std::endl;
    }
}

std::string getIPAddress(const std::string& iface)
{
    std::string ip;
    struct ifaddrs* interfaces = NULL;

    if (getifaddrs(&interfaces) == 0)
    {
        struct ifaddrs* temp_addr = interfaces;
        while (temp_addr)
        {
            if (temp_addr->ifa_addr->sa_family == AF_INET)
            {
                if (strncmp(temp_addr->ifa_name, iface.c_str(), iface.size()) ==
                    0)
                {
                    ip = inet_ntoa(
                        ((struct sockaddr_in*)temp_addr->ifa_addr)->sin_addr);
                    break;
                }
            }
            temp_addr = temp_addr->ifa_next;
        }
    }
    freeifaddrs(interfaces);
    return ip;
}

void showFps()
{
    static std::chrono::steady_clock::time_point begin =
        std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point end =
        std::chrono::steady_clock::now();
    uint32_t diff_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - begin)
            .count();
    uint32_t fps = diff_ms == 0 ? 0 : 1000 / diff_ms;

    begin = end;
    std::cout << "          \r"
              << "[FPS: " << fps << "] " << std::flush;
}

int main(int argc, char* argv[])
{
    std::unordered_map<std::string, int> args = {
        {"videoCamNum", 0}, {"videoWidth", 640},  {"videoHeight", 480},
        {"videoFps", 60},   {"videoQuality", 80}, {"streamPort", 8001},
    };

    boost::program_options::options_description desc("Allowed options");
    desc.add_options()("help", "produce help message")(
        "logs", boost::program_options::value<int>(), "enable debug logs")(
        "port", boost::program_options::value<int>(), "set streaming port")(
        "lccv", "use opencv support for new libcamera stack");

    boost::program_options::variables_map vm;
    boost::program_options::store(
        boost::program_options::parse_command_line(argc, argv, desc), vm);
    boost::program_options::notify(vm);

    if (vm.contains("help"))
    {
        printLog(LOGLEVEL::INFO, "streamer", desc);
        return 0;
    }

    if (vm.contains("logs"))
    {
        printLog(LOGLEVEL::DEBUG, "streamer",
                 "Set logging level: " + vm.at("logs").as<int>());
    }

    int& port = args["streamPort"];
    if (vm.contains("port"))
    {
        port = vm.at("port").as<int>();
        printLog(LOGLEVEL::DEBUG, "streamer", "Set streaming port: " + port);
    }

    if (vm.contains("lccv"))
    {
        printLog(LOGLEVEL::DEBUG, "streamer", "Using new libcam stack");
    }

    http::Server s(port);
    std::string ip = getIPAddress("eth0");
    if (!ip.empty())
    {
        printLog(LOGLEVEL::INFO, "streamer",
                 "Reach streaming under url: http://" + ip + ":" +
                     std::to_string(port));
    }
    else
    {
        printLog(LOGLEVEL::ERROR, "streamer",
                 "Cannot detect ip aaddress for target interface, aborting");
        return 5;
    }

    s.get("/img", [&](auto, auto res) {
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

         std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY,
                                    args["videoQuality"]};
         lccv::PiCamera cam;
         cam.options->camera = args["videoCamNum"];
         cam.options->video_width = args["videoWidth"];
         cam.options->video_height = args["videoHeight"];
         cam.options->framerate = args["videoFps"];
         cam.options->verbose = false;
         cam.startVideo();

         cv::Mat frame;
         std::vector<uchar> buffer;

         while (true)
         {
             if (!cam.getVideoFrame(frame, 100))
             {
                 printLog(LOGLEVEL::WARN, "streamer",
                          "Cannot get frame, skipping");
                 continue;
             }

             cv::imencode(".jpg", frame, buffer, params);
             std::string image(buffer.begin(), buffer.end());

             if (!res.send_msg("--boundary\r\n"
                               "Content-Type: image/jpeg\r\n"
                               "Content-Length: " +
                               std::to_string(image.size()) + "\r\n\r\n" +
                               std::move(image)))
             {
                 return;
             }

             showFps();
             printLog(LOGLEVEL::DEBUG, "streamer", "New frame was streamed");
         }
     }).get("/", [ip, port, &args](auto, auto res) {
        res >> "<html>"
               "    <body>"
               "        <h1>Camera streaming</h1>"
               "        <img src='http://" +
                   ip + ":" + std::to_string(port) + "/img'/ width='" +
                   std::to_string(args["videoWidth"]) + "' height='" +
                   std::to_string(args["videoHeight"]) +
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
        printLog(LOGLEVEL::ERROR, "streamer",
                 "[Runtime error] " + std::string(ex.what()));
    }
    catch (std::exception& ex)
    {
        printLog(LOGLEVEL::ERROR, "streamer",
                 "[Generic error] " + std::string(ex.what()));
    }
    catch (...)
    {

        printLog(LOGLEVEL::ERROR, "streamer", "[Undefined error]");
    }

    return 0;
}
