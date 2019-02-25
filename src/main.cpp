#include "mdet.hpp"
#include "fs.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <ctime>

// a long-ass weekend
static const int ROTATING_LOG_MAX_DAYS = 5;

static void setup_rotating_logs(opts &os);


int main(int argc, char **argv) 
{
  opts os;
  

  std::stringstream USAGE;
  USAGE <<
    "Motion Detection (" << MDET_VERSION_STRING << ")\n" <<
    "usage: " << argv[0] << " OPTIONS\n" 
    "where\n" 
    "  OPTIONS are:\n" 
    //||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||v 80 cols
    "    --exit-after=INT            exits after this many seconds\n"
    "    --headless                  don't open any windows to show statistics\n" 
    "    --log-file=PATH             specifies the log file path\n" 
    "                                (defaults to " << os.log_file_path << ")\n" 
    "    --log-rotate                enables log rotation mode; we use the day\n"
    "                                of the year to choose the output directories\n"
    "                                rotating every " << ROTATING_LOG_MAX_DAYS << " days and overwriting old\n"
    "                                output; this also impacts --remote-copy\n"
    "                                NOTE: a successive run will not pre-delete old\n"
    "                                motion videos, so check the file timestamps\n"
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
    "                                startup\n"
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
    //||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||^ 80 cols
    "";

  // first check for  the --nightly option
  bool rotate_logs = false; // --log-rotate

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
          badOpt("option requires argument"
            " (all one token separated by '='; e.g. --foo=bar)");
      };
    auto forbidsOptValue =
      [&] () {
        if (!opt_value.empty())
          badOpt("option forbids argument (it's a flag)");
      };
    auto optValStr =
      [&]() {
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
    } else if (opt_key == "--exit-after") {
      os.exit_after = (int)optValInt();
    } else if (opt_key == "--headless") {
      forbidsOptValue();
      os.headless = true;
    } else if (opt_key == "--log-rotate") {
      forbidsOptValue();
      rotate_logs = true;
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

  if (rotate_logs) {
    // --log-rotate=...
    // std::cout << "--log-rotate=... given\n";
    setup_rotating_logs(os);
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


void setup_rotating_logs(opts &os)
{
  time_t tt;
  std::time(&tt);
  struct tm t;
#ifndef _MSC_VER
  // https://en.cppreference.com/w/c/chrono/localtime
  // the parameters are usually in the opposite order for the C11 function
  std::localtime_s(&tt, &t);
#else
  // MSVC's localtime_s takes them in the opposite order
  // likely I need to enable some flag to enable C11 (note, we're using C++17)
  ::localtime_s(&t, &tt);
#endif
  // day of the year
  int day_of_year = 0;
  try {
    char tbuf[128];
    std::strftime(tbuf, sizeof(tbuf), "%j",&t);
    day_of_year = (int)std::stoll(tbuf,nullptr,10);
  } catch (...) {
    fatal("INTERNAL ERROR: parsing day of year from time_t");
  }
  day_of_year %= ROTATING_LOG_MAX_DAYS;
  std::stringstream ss;
  ss << "logs";
  ss << std::setfill('0') << std::setw(5) << 
    (day_of_year % ROTATING_LOG_MAX_DAYS);
  std::string error;

  // create local copy (relative to CWD)
  fs::create_directory_if_absent(ss.str(), error);
  if (!error.empty())
      fatal(error,": creating local video copy directory ",ss.str());
  os.motion_video_dir = ss.str();

  if (!fs::is_absolute_path(os.log_file_path)) {
    os.log_file_path = 
      fs::join_path(ss.str(),os.log_file_path);
  } else {
    fatal("--log-file may not be an absolute path in rotating-log mode");
  }

  if (!os.remote_copy_dir.empty()) {
    // create remote copy (if needed)
    auto remote_dir = fs::join_path(os.remote_copy_dir,ss.str());
    fs::create_directory_if_absent(remote_dir, error);
    if (!error.empty())
      fatal(error,": creating remote video copy directory ",remote_dir);
  }
}

