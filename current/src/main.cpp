#include "server/server.h"

#include "ai/ncs/detectors/face.hpp"
#include "ai/ncs/detectors/person.hpp"
#include "camera/interfaces/rpi5/csi.hpp"
#include "log/interfaces/console.hpp"
#include "streamer/helpers.hpp"
#include "tts/interfaces/googlecloud.hpp"

#include <sched.h>

#include <boost/program_options.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <atomic>
#include <future>
#include <iostream>
#include <ranges>
#include <sstream>

template <typename T>
class ActionHandler
{
  public:
    ActionHandler(const std::function<void(const T&)>& act,
                  const std::function<void()>& prep) :
        action(act),
        preparation(prep)
    {
        async = std::async(std::launch::async, [this]() {
            while (true)
            {
                std::unique_lock lock(mtx);
                condvar.wait(lock, [this]() { return isready; });
                isready = false;
                lock.unlock();
                action(value);
            }
        });
    }

    void update(const T& v)
    {
        std::lock_guard lock(mtx);
        preparation();
        value = v;
        isready = true;
        condvar.notify_one();
        std::cout << "Thread #action: on CPU " << sched_getcpu() << "\n";
    }

  private:
    std::function<void(const T&)> action;
    std::function<void()> preparation;
    std::future<void> async;
    std::mutex mtx;
    std::condition_variable condvar;
    bool isready{false};
    T value;
};

int main(int argc, char* argv[])
{
    std::unordered_map<std::string, uint32_t> args = {
        {"videoCamNum", 0}, {"videoWidth", 320},  {"videoHeight", 240},
        {"videoFps", 30},   {"videoQuality", 80}, {"streamPort", 8001},
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
        "mp", boost::program_options::value<std::string>(),
        "person detection model xml file path")(
        "mf", boost::program_options::value<std::string>(),
        "face detection model xml file path")(
        "device,d", boost::program_options::value<std::string>(),
        "device type")("lccv,l", "use opencv support for new libcamera stack");

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

    std::string mp;
    if (vm.contains("mp"))
    {
        mp = vm.at("mp").as<std::string>();
        logIf->log(logging::type::debug, module,
                   "path of xml person detection model file: " + mp);
    }

    std::string mf;
    if (vm.contains("mf"))
    {
        mf = vm.at("mf").as<std::string>();
        logIf->log(logging::type::debug, module,
                   "path of xml face detection model file: " + mf);
    }

    std::string device;
    if (vm.contains("device"))
    {
        device = vm.at("device").as<std::string>();
        logIf->log(logging::type::debug, module, "device type: " + device);
    }

    if (vm.contains("lccv"))
    {
        logIf->log(logging::type::debug, module, "Using new libcam stack");
    }

    http::Server server(port);
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

    auto ttsIf = tts::TextToVoiceFactory::create<tts::googlecloud::TextToVoice>(
        {tts::language::polish, tts::gender::female, 1});

    auto action = ActionHandler<int32_t>(
        [ttsIf](int32_t peoplenum) {
            std::string msg;
            switch (peoplenum)
            {
                case 0:
                    msg = "Nie widzę człowieków";
                    break;
                case 1:
                    msg = "Widzę 1 człowieka";
                    break;
                default:
                    msg = "Widzę " + std::to_string(peoplenum) + " ludzi";
                    break;
            }
            if (ttsIf)
            {
                std::cout << "Speak: " << msg << "\n";
                ttsIf->speak(msg);
            }
        },
        []() { tts::TextToVoiceIf::kill(); });

    auto ai = std::make_shared<ai::ncs::ProcessorIf>();
    auto person =
        ai::ncs::Factory::create<ai::ncs::person::Detector>(mp, device);
    auto face = ai::ncs::Factory::create<ai::ncs::face::Detector>(mf, device);
    ai->setnext(person)->setnext(face);

    person->Executable::subscribe(
        ai::Executor<std::vector<ai::ncs::Result>>::create([&action, logIf](
                                                               auto& results) {
            static uint32_t peoplenum;
            if (results.size() != peoplenum)
            {
                peoplenum = results.size();
                logIf->log(logging::type::info, "executor",
                           "People I can see: " + std::to_string(peoplenum));
                action.update(peoplenum);
            }
        }));

    face->Executable::subscribe(
        ai::Executor<std::vector<ai::ncs::Result>>::create(
            [logIf](auto& results) {
                static uint32_t facesnum;
                if (results.size() != facesnum)
                {
                    facesnum = results.size();
                    logIf->log(logging::type::info, "executor",
                               "Faces I can see: " + std::to_string(facesnum));
                }
            }));

    // personDetection.reshape(ie, 240, 320);
    auto camera = camera::Factory::create<camera::csi::Camera>(
        logIf, {args["videoCamNum"], args["videoWidth"], args["videoHeight"],
                args["videoFps"]});
    // personDetection.reshape(ie, 600, 800);
    //  camera->Processable::subscribe(
    //      {1, streamer::Processor<cv::Mat>::create([](auto& frame) {
    //           cv::resize(frame, frame, cv::Size(320, 240));
    //           cv::putText(frame, "#EXAMINED#", cv::Point(10, 30),
    //                       cv::FONT_HERSHEY_DUPLEX, 1.0, CV_RGB(0, 0,
    //                       255), 2);
    //       })});

    camera->Processable::subscribe(
        {2, camera::Processor<cv::Mat>::create([&ai](auto& frame) {
             // cv::resize(frame, frame, cv::Size(400, 300));
             static const cv::Mat blank{240, 320, CV_8UC3,
                                        CV_RGB(255, 255, 255)};
             std::vector<cv::Mat> out;
             ai->process(frame, out);
             switch (out.size())
             {
                 case 0:
                     break;
                 case 1:
                     cv::hconcat(frame, out[0], frame);
                     break;
                 case 2:
                     cv::hconcat(frame, out[0], out[0]);
                     cv::hconcat(out[1], blank, out[1]);
                     cv::vconcat(out[0], out[1], frame);
                     break;
                 case 3:
                     cv::hconcat(frame, out[0], out[0]);
                     cv::hconcat(out[1], out[2], out[2]);
                     cv::vconcat(out[0], out[2], frame);
                     break;
                 default:
                     throw std::runtime_error("Images array size (" +
                                              std::to_string(out.size()) +
                                              ") not supported ");
             }
             //  std::ranges::for_each(std::views::iota(0, (int32_t)out.size()),
             //                        [&](auto idx) {
             //                            if (idx % 2)
             //                                cv::vconcat(frame, out[idx],
             //                                frame);
             //                            else
             //                                cv::hconcat(frame, out[idx],
             //                                frame);
             //                        });
         })});
    camera->start();

    server
        .get("/img",
             [&module, quality = args["videoQuality"], camera,
              logIf](auto, auto res) {
                 res.headers.push_back("Connection: close");
                 res.headers.push_back("Max-Age: 0");
                 res.headers.push_back("Expires: 0");
                 res.headers.push_back("Cache-Control: no-cache, private");
                 res.headers.push_back("Pragma: no-cache");
                 res.headers.push_back(
                     "Content-Type: "
                     "multipart/x-mixed-replace;boundary=--boundary");

                 if (!res.send_header())
                 {
                     return;
                 }

                 static uint32_t clinetnum{1};
                 auto monitor{streamer::TimeMonitor{clinetnum++}};
                 std::stop_source state;
                 auto running = state.get_token();
                 auto streaming =
                     camera::Observer<cv::Mat>::create([&](auto& frame) {
                         std::vector<uchar> buffer;
                         cv::imencode(
                             ".jpg", frame, buffer,
                             {cv::IMWRITE_JPEG_QUALITY, (int32_t)quality});
                         std::string image(buffer.begin(), buffer.end());

                         if (!res.send_msg("--boundary\r\n"
                                           "Content-Type: image/jpeg\r\n"
                                           "Content-Length: " +
                                           std::to_string(image.size()) +
                                           "\r\n\r\n" + std::move(image)))
                         {
                             logIf->log(
                                 logging::type::warning, module,
                                 "Cannot stream image content, closing http "
                                 "thread");
                             state.request_stop();
                             return;
                         }

                         logIf->log(logging::type::debug, module,
                                    "New frame was streamed");
                         monitor.printfps();
                     });

                 camera->Observable::subscribe(streaming);
                 while (true)
                 {
                     if (running.stop_requested())
                     {
                         camera->Observable::unsubscribe(streaming);
                         break;
                     }
                     usleep(100 * 1000);
                 }
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
        server.listen();
        camera->stop();
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
