#include "server.h"

#include <ifaddrs.h>

#include <lccv.hpp>

#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include <chrono>
#include <iostream>
#include <thread>
#include <unordered_map>

constexpr bool logsEnabled = false;

void printLog(const std::string& id, const std::string& msg)
{
    if constexpr (logsEnabled)
    {
        std::cout << "[" << id << "] " << msg << std::endl;
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
    int& port = args["streamPort"];

    if (argc > 1)
    {
        std::string arg = argv[1];
        if (std::find_if(arg.begin(), arg.end(), [](unsigned char c) {
                return !std::isdigit(c);
            }) == arg.end())
        {
            port = std::stoi(arg);
        }
    }

    http::Server s(port);
    std::string ip = getIPAddress("eth0");
    if (!ip.empty())
    {
        std::cout << "Reach streaming under url: http://" << ip << ":" << port
                  << std::endl;
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
/*
         //cv::VideoCapture cap(args["videoCapNum"]);
         cv::VideoCapture cap(cv::CAP_V4L2);
         if (!cap.isOpened())
         {
             std::cerr << "VideoCapture id: " << args["videoCapNum"]
                       << " not opened!\n";
             exit(EXIT_FAILURE);
         }

         std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY,
                                    args["videoQuality"]};
         cap.set(cv::CAP_PROP_FRAME_WIDTH, args["videoWidth"]);
         cap.set(cv::CAP_PROP_FRAME_HEIGHT, args["videoHeight"]);
         cap.set(cv::CAP_PROP_FPS, args["videoFps"]);
         cap.set(cv::CAP_PROP_BUFFERSIZE, 1);
         // cap.set(cv::CAP_PROP_AUTOFOCUS, true);
         // cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P',
         // 'G'));
         cap.set(cv::CAP_PROP_FOURCC,
                 cv::VideoWriter::fourcc('R', 'G', 'B', '3'));

         int codecRaw = static_cast<int>(cap.get(cv::CAP_PROP_FOURCC));
         char codec[] = {(char)(codecRaw & 0xFF),
                         (char)((codecRaw & 0xFF00) >> 8),
                         (char)((codecRaw & 0xFF0000) >> 16),
                         (char)((codecRaw & 0xFF000000) >> 24), 0};
         std::cout << "Streaming camera: " << args["videoCapNum"] << " ["
                   << cap.get(cv::CAP_PROP_FRAME_WIDTH) << "x"
                   << cap.get(cv::CAP_PROP_FRAME_HEIGHT)
                   << "] w/ quality: " << args["videoQuality"]
                   << " and FPS: " << cap.get(cv::CAP_PROP_FPS) << std::endl;
         std::cout << "Codec: " << codec
                   << ", buffer size: " << cap.get(cv::CAP_PROP_BUFFERSIZE)
                   << std::endl;
*/


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
             //cap.read(frame);
	     cam.getVideoFrame(frame, 1000);
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
             printLog("streamer", "New frame was streamed");
         }
     }).get("/", [ip, port, &args](auto, auto res) {
        res >> "<html>"
               "    <body>"
               "        <h1>Camera streaming</h1>"
               "        <img src='http://" +
                   ip + ":" + std::to_string(port) +
                   "/img'/ width='" + std::to_string(args["videoWidth"]) + 
		   "' height='" + std::to_string(args["videoHeight"]) + "'>"
                   "    </body>"
                   "</html>";
    });

    try
    {
        s.listen();
    }
    catch (std::runtime_error& ex)
    {
        std::cerr << "[Runtime error] " << ex.what() << std::endl;
    }
    catch (std::exception& ex)
    {
        std::cerr << "[Generic error] " << ex.what() << std::endl;
    }
    catch (...)
    {
        std::cerr << "[Undefined error]" << std::endl;
    }

    return 0;
}
