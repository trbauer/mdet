#ifndef VCAP_HPP
#define VCAP_HPP

#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
// #include <opencv2/core/opencl/opencl_info.hpp>

#include <array>
#include <cstdint>
#include <chrono>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <string>
#include <sstream>

struct opts {
  std::string       log_file_path = "mdet.log";
  std::string       motion_video_dir;
  std::string       remote_copy_dir;
  std::string       preferred_fourcc;
  int               max_videos = 512; // 30s takes about 33mb, so this maxes out at about 20g
                                      // it's much smaller with H264
  int               max_video_length = 30;
  int               startup_delay = 5;
  // from testing we find these constants (640x480)
  //   covered webcam                  ~15000.0
  //   sitting totally still           ~60000.0
  //   breathing                      ~120000.0
  //   walking into the cube moving   over a million
  // this is for a 640x480 image
  double            motion_threshold = 4.0;
  bool              has_custom_motion_threshold = false;
  bool              headless = false;
  int               exit_after = 0;
};

static const int TARGET_FPS = 30;
static const double FRAME_BUDGET_MS = 1000.0/TARGET_FPS;

static const int ESC_KEY = 0x1B;

using time_point = std::chrono::steady_clock::time_point;
static time_point now() {return std::chrono::steady_clock::now();}

static const int EVENT_HISTORY = 8*32; // about 8 seconds
static const int PREVIOUS_FRAMES = 2*32; // about 2 seconds

// using image = cv::UMat;
using image = cv::Mat;


static void format(std::ostream &os, double d, int wid = 0, int prec = 1) {
  if (wid > 0)
    os << std::setw(wid) << std::fixed << std::setprecision(prec) << d;
  else
    os <<                   std::fixed << std::setprecision(prec) << d;
}
static std::string format(double d, int wid = 0, int prec = 1) {
  std::stringstream ss;
  format(ss, d, wid, prec);
  return ss.str();
}
static const char *format(bool z) {return z ? "true" : "false";}

template <typename T,typename...Ts>
static void concatUnpack(std::stringstream &ss, T t) {
  ss << t;
}
template <typename T,typename...Ts>
static void concatUnpack(std::stringstream &ss, T t, Ts...ts) {
  ss << t;
  concatUnpack(ss, ts...);
}
template <typename...Ts>
static std::string concat(Ts...ts)
{
  std::stringstream ss;
  concatUnpack(ss,ts...);
  return ss.str();
}


template <typename...Ts>
static void fatal(Ts...ts)
{
  auto str = concat(ts...);
  std::cerr << str;

  // in case of crash during startup we write the result of fatal() to
  // a different file; then if starting via task manager fails, we can bail
  {
    std::ofstream fatal_stream("fatal.log");
    if (fatal_stream) {
      fatal_stream << "FATAL: " << str;
      fatal_stream.flush();
    }
  }
  
  exit(EXIT_FAILURE);
}

// copyasync.cpp
struct copy_thread {
  volatile bool done = false;
  std::string source_file_name;
  std::string target_file_name;
  std::string error_message;

  std::thread thread;

  copy_thread(std::string _file_name, std::string _to_dir);
  void run();
};

template <typename T,int N>
struct circular_buffer {
  uint64_t total = 0;
  std::array<T,N> elements;

  T &add() {
    T* t = &elements[total % N];
    total++;
    return *t;
  }
  void add(const T &t) {
    add() = t;
  }

  const T &oldest() const {
    return total < N ?
      elements[0] :
      elements[total % N]; // next entry is oldest
  }
  const T &newest() const {
    return elements[(total - 1) % N];
  }

  void for_each(std::function<void(const T &)> process) const {
    if (total > N) {
      // the tail is populated
      for (int i = (int)total % N; i < N; i++) {
        process(elements[i]);
      }
    }
    for (int i = 0; i < total % N; i++) {
      process(elements[i]);
    }
  }
};

template <typename T,int N>
struct numeric_circular_buffer : circular_buffer<T,N>
{
  double average() const {
    double sum = 0.0f;
    for (int i = 0; i < total % (N+1); i++) {
      sum += (double)elements[i];
    }
    return sum / (total % (N+1));
  }
  /*
  double tail_average(int last) const {
    double sum = 0.0f;
    if (total < N) {
      for (int i = 0; i < last; i++) {
        sum += (double)elements[i];
      }
    } else {
      for (unsigned i = 0; i < std::min(last,total) % N; i++) {
        sum += (double)elements[(total - 1 - i) % N];
      }
    }
    return sum / (last % N);
  }
  */
};

template <int N>
struct time_samples : numeric_circular_buffer<int64_t,N>
{
  time_point start_time;
  
  time_samples() { }
  void start() {start_time = now();}
  void stop() {
    std::chrono::microseconds elapsed = 
      std::chrono::duration_cast<std::chrono::microseconds>(
        now() - start_time);
    add(elapsed.count());
  }

  double average_ms() const {
    return average() / 1000.0;
  }
};

static const int MOTION_SAMPLES = 32*8; // about a 8 seconds

// template <IMAGE_TYPE>
// template <IMAGE_TYPE=cv::umat> for OpenCL?
struct motion_detector {
  opts            os;
  std::ostream   &log_stream;

  cv::VideoCapture vc;
  int next_video_index = 0;

  double motion_threshold;
  
  numeric_circular_buffer<double,MOTION_SAMPLES>     motion_samples;

  time_samples<64> motion_cost_estimate;
  time_samples<64> hud_draw_cost_estimate;
  time_samples<64> frame_overhead_estimate;

  // pre-buffering so we can see stuff before the motion
  circular_buffer<image,PREVIOUS_FRAMES> color_frames;

  time_point startup_time; // for uptime()
  double min_motion_diff = DBL_MAX, max_motion_diff = 0.0f;

  // not sure if saving these is helpful; certainly if they pin GPU memory
  // it's less work to thrash new memory
  image color_to_gray, gray_to_blurred, background_frame_gray_blurred;
  image absdiff;
  image stats_window;

  time_point last_capture_time;  // helps us keep track of FPS

  std::list<copy_thread*> copy_threads; // pending async copies

  // HUD controls
  bool vidcap_disabled = false;
  bool hud_enabled = true;

  // exit flag
  bool exit_detector = false;

  motion_detector(
    std::ostream &_log_stream, 
    const opts &os);
  ~motion_detector();

  double uptime() const;
  void reset_background(int countdown_s, const char *why);

  void join_finished_asyncs();
  void start_copy_to_remote_async(std::string file_name);

  const image &capture_frame(cv::VideoWriter *vw = nullptr);
  
  void calibrate_motion_threshold();


  bool detecting_motion();

  void draw_hud(double video_offset = 0.0);

  void capture_video(const char *why);
  void capture_video_body(std::string file_name);

  void run();
  void process_key(int key);

  template <typename...Ts>
  void log(Ts...ts) {logs(concat(ts...));}
  void logs(const std::string &s);
  void fatals(const std::string &s);
  template <typename...Ts>
  void fatal(Ts...ts) {fatals(concat(ts...));}
};





#endif