#include "mdet.hpp"

#include <iostream>
#include <fstream>
#include <sstream>

int main(int argc, char **argv) 
{
  opts os;

  std::stringstream USAGE;
  USAGE <<
    "Motion Detection (" << MDET_VERSION_STRING << ")\n" <<
    "usage: " << argv[0] << " OPTIONS\n" 
    "where\n" 
    "  OPTIONS are:\n" 
    //                                                                              |
    "    --headless                  don't open any windows to show statistics\n" 
    "    --log-file=PATH             specifies the log file path\n" 
    "                                (defaults to " << os.log_file_path << ")\n" 
    "    --max-videos=INT            exit after creating this many videos\n" 
    "                                (defaults to " << os.max_videos << ")\n" 
    "    --max-video-length=INT      maximum length in seconds for video captures\n"
    "                                (defaults to " << os.max_video_length << ")\n" 
    "                                setting this to 0 disables video capture\n" 
    "    --motion-threshold=FLT      sets the motion threshold to a given value\n" 
    "                                this must be value between 0.0 and 255.0;\n" 
    "                                good values are around 0.5 to 2.0; the program\n"
    "                                compares this to the average pixel value\n"
    "                                (0 to 255) in the blurred difference image\n"
    "                                to infer motion; by default the program\n"
    "                                automatically calibrates the threshold upon\n"
    "                                starutp\n"
    "    --preferred-fourcc=CHAR[4]  the four character code for the video format\n"
    "                                (passed to cv::VideoWriter); without this set\n"
    "                                (or if this code fails)  the program tries\n"
    "                                several possible formats (e.g. H264, X264,\n"
    "                                XVID, MP4V etc...); for an h264 encoder see\n" 
    "                                https://github.com/cisco/openh264/releases\n" 
    "    --remote-copy=PATH          asynchronously copy videos to this directory\n" 
    "    --startup-delay=INT         delay this many seconds before starting up\n" 
    "                                (defaults to " << os.startup_delay << ")\n" <<
    "  INTERACTIVE OPTIONS (when focused on an OpenCV window)\n"
    "    type '?' to emit help to the console on which keys do what\n"
    "";


  for (int i = 1; i < argc; i++) {
    std::string argstr(argv[i]);
    std::string opt_key;
    std::string opt_value;
    if (!argstr.empty() && argstr[0] == '-') {
      auto voff = argstr.find('=');
      opt_key = argstr.substr(0,voff);
      if (voff != std::string::npos)
        opt_value = argstr.substr(voff + 1);
    }
    auto badOpt =
      [&](const char *why) {
        std::cerr << argstr << ": " << why << "\n";
        exit(EXIT_FAILURE);
      };
    auto needsOptValue =
      [&] () {
        if (opt_value.empty())
          badOpt("option requires argument (all one token separated by '='; e.g. --foo=bar)");
      };
    auto forbidsOptValue =
      [&] () {
        if (!opt_value.empty())
          badOpt("option forbids argument (it's a flag)");
      };
    auto optValStr = [&](){
      needsOptValue();
      return opt_value;
    };

    auto optValInt = [&](){
      needsOptValue();
      int64_t value = 0;
      try {
        if (opt_value.size() > 2 && 
          (opt_value.substr(0,2) == "0x" || opt_value.substr(0,2) == "0X"))
          value =(int64_t)std::stoll(opt_value.substr(2),nullptr,16);
        else
          value = (int64_t)std::stoll(opt_value,nullptr,10);
      } catch (...) {
        badOpt("malformed integer");
      }
      return value;
    };
    auto optValDouble = [&](){
      needsOptValue();
      double value = 0;
      try {
        value = std::stod(opt_value);
      } catch (...) {
        badOpt("malformed double");
      }
      return value;
    };

    if (argstr == "-h" || argstr == "--help") {
      std::cout << USAGE.str();
      exit(EXIT_SUCCESS);
    } else if (opt_key == "--headless") {
      forbidsOptValue();
      os.headless = true;
    } else if (opt_key == "--log-file") {
      os.log_file_path = optValStr();
    } else if (opt_key == "--max-video-length") {
      os.max_video_length = (int)optValInt();
    } else if (opt_key == "--max-videos") {
      os.max_videos = (int)optValInt();
    } else if (opt_key == "--motion-threshold") {
      os.has_custom_motion_threshold = true;
      os.motion_threshold = optValDouble();
    } else if (opt_key == "--preferred-fourcc") {
      os.preferred_fourcc = optValStr();
      if (os.preferred_fourcc.size() != 4)
        badOpt("must be four characters");
    } else if (opt_key == "--remote-copy") {
      os.remote_copy_dir = optValStr();
    } else if (opt_key == "--startup-delay") {
      os.startup_delay = (int)optValInt();
    } else {
      badOpt("unrecognized option");
    }
  }

//  cv::dumpOpenCLInformation();
  std::ofstream log_file(os.log_file_path);
  if (!log_file.is_open()) {
    std::cerr << "failed to open log file\n";
    exit(EXIT_FAILURE);
  }
  
  motion_detector s(log_file,os);
  s.run();
  
  return EXIT_SUCCESS;
}