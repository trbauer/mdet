#include "mdet.hpp"

#include <ctime>


motion_detector::motion_detector(
  std::ostream &_log_stream,
  const opts &_os) 
  : os(_os) 
  , vc(0)
  , log_stream(_log_stream)
  , stats_window(480,640,CV_8UC3) {
  if (!vc.isOpened()) {
    std::cerr << "FATAL: cannot open camera\n";
    std::exit(EXIT_FAILURE);
  }
  hud_enabled = !os.headless;
  vidcap_disabled = os.max_video_length <= 0;
  if (vidcap_disabled)
    log("video capture disabled (max video length <= 0)");
  motion_threshold = os.motion_threshold;
  startup_time = now();
} 

motion_detector::~motion_detector() {
  std::cout << "shutting down\n";
  for (copy_thread *ct : copy_threads) {
    std::cout << "waiting for copy thread\n";
    ct->thread.join();
  }
  cv::destroyAllWindows();
}

double motion_detector::uptime() const {
  auto elapsed =
    std::chrono::duration_cast<std::chrono::microseconds>(
      now() - startup_time);
  return elapsed.count()/1000.0/1000.0;
}

void motion_detector::reset_background(int countdown_s, const char *why) {
  log("resetting background (",why,")");
  for (int i = 0; i < countdown_s && !exit_detector; i++) {     
    std::cout << (countdown_s - i) << "...\n";
    auto key = cv::waitKey(1000);
    process_key(key);
  }
  image background_frame_color, background_frame;
  vc.read(background_frame_color);
  cv::cvtColor(background_frame_color,background_frame,cv::COLOR_BGR2GRAY);
  cv::GaussianBlur(
    background_frame, background_frame_gray_blurred, cv::Size(21,21), 0.0);

  cv::imshow("background frame",background_frame_gray_blurred);
}

void motion_detector::join_finished_asyncs() {
  while (!copy_threads.empty()) {
    auto *ct = copy_threads.front();
    if (!ct->done) {
      break;
    }
    copy_threads.pop_front();
    ct->thread.join();
    if (!ct->error_message.empty()) {
      log(ct->target_file_name,": ERROR: ", ct->error_message);
    }
    log(ct->target_file_name,": deleted copy thread");
    delete ct;
  }
}

void motion_detector::start_copy_to_remote_async(std::string file_name) {
  if (!os.remote_copy_dir.empty()) {
    log(file_name, ": starting copy thread");
    copy_threads.push_back(new copy_thread(file_name,os.remote_copy_dir));
  }
}

const image &motion_detector::capture_frame(cv::VideoWriter *vw) {
  image &i = color_frames.add();

  frame_overhead_estimate.stop();

  vc.read(i);

  frame_overhead_estimate.start();

  if (vw) {
    vw->write(i);
  }

  return i;
}

bool motion_detector::detecting_motion() {
  // https://www.pyimagesearch.com/2015/05/25/basic-motion-detection-and-tracking-with-python-and-opencv/
  // I can't tell if the gray version of currFrame is blurred

  motion_cost_estimate.start();

  cv::cvtColor(color_frames.newest(),color_to_gray,cv::COLOR_BGR2GRAY);
  cv::GaussianBlur(
    color_to_gray, 
    gray_to_blurred, 
    cv::Size(21,21), 0.0);

  cv::absdiff(gray_to_blurred, background_frame_gray_blurred, absdiff);
  double adiff_susm = cv::sum(absdiff)[0];
  double adiff_ratio = adiff_susm/absdiff.size().area();
  // TODO: remove once the HUD works
  // if (color_frames.total % 32 ==  0)
  //  std::cout << std::fixed << std::setprecision(3) << "DIFF: " << adiff_ratio << "\n";

  min_motion_diff = std::min(adiff_ratio,min_motion_diff);
  max_motion_diff = std::max(adiff_ratio,max_motion_diff);

  motion_samples.add(adiff_ratio);
  // if (motion_samples.average() > motion_threshold) {
  //    careful here, if we outrun our color buffer history, we're screwed
  // }

  bool motion_detected = adiff_ratio > motion_threshold;
  if (motion_detected) {
    log("motion detected (", format(adiff_ratio,0,3), " > ", 
      format(motion_threshold,0,3), ")");
  }

  motion_cost_estimate.stop();

  return motion_detected;
}

 void motion_detector::capture_video(const char *why) {
  if (vidcap_disabled || os.max_video_length <= 0) {
    log("aborting capture (vidcap disabled)");
    return;
  }
  int video_index = next_video_index++;
  std::stringstream ss;
  ss << "video" << std::setw(5) << std::setfill('0') << video_index << ".mp4";
  auto file_name = ss.str();
  log("capturing video (",why,") as ", file_name);
  capture_video_body(file_name);
  start_copy_to_remote_async(file_name);
}

void motion_detector::capture_video_body(std::string file_name) {
  cv::VideoWriter vw;

  auto open_video_output = 
    [&](const char *ccs)
    {
      int four_cc = cv::VideoWriter::fourcc(
            ccs[0],
            ccs[1],
            ccs[2],
            ccs[3]);
      vw.open(
        file_name, 
        four_cc, 
        (double)TARGET_FPS, 
        color_frames.newest().size(), 
        true);
      return vw.isOpened();
    };

  // http://www.fourcc.org/codecs.php
  static const char* FOUR_CCS[] {
    "H264",
    "X264",
    "XVID",
    "MP4V",
  };
    
  if (!os.preferred_fourcc.empty() && open_video_output(os.preferred_fourcc.c_str())) {
    log("opened video (in preferred format)");
  } else {
    log("falling back to other formats");
    for (int i = 0; i < sizeof(FOUR_CCS)/sizeof(FOUR_CCS[0]); i++) {
      log("trying ", FOUR_CCS[i]);
      if (open_video_output(FOUR_CCS[i])) {
        log("opened video (in ",FOUR_CCS[i],")");
        break;
      }
    }
    if (!vw.isOpened()) {
      log("ERROR: failed to open video writer after several tries; giving up");
      return;
    }
  }

  // write the past frames
  color_frames.for_each([&](const image &color_frame) {
    vw.write(color_frame);
  });

  double last_elapsed = 0.0f;
  auto video_started = uptime();
 
  while (true) {
    capture_frame(&vw);

    double elapsed = uptime() - video_started;
    if (elapsed > os.max_video_length) {
      log(file_name,": stopping video recording (max time reached)");
      break;
    }

    auto stall = FRAME_BUDGET_MS - 1000.0*(elapsed - last_elapsed);
    int stall_int = std::max((int)stall,1); // 0 means forever; so use 1
    // std::cout << "stall: " << stall_int << "\n";
    auto key = cv::waitKey(stall_int);
    process_key(key);
    if (exit_detector || key == 'c') {
      log(file_name,": stopping video recording (by command)");
      break;
    }

    if (hud_enabled) {
      draw_hud(elapsed);
    }

    last_elapsed = elapsed;
  }
  vw.release();
}

void motion_detector::calibrate_motion_threshold()
{
  log("calibrating motion threshold");
  for (int i = 0; i < MOTION_SAMPLES; i++) {
    (void)capture_frame();
    (void)detecting_motion();
  }
  double avg = motion_samples.average();
  log("     average motion is ",format(avg,0,3));
  motion_threshold = 1.2*avg;
  log("     setting threshold to ",format(motion_threshold,0,3));
}

void motion_detector::draw_hud (double video_offset) {
  hud_draw_cost_estimate.start();

  // zero it
  stats_window.setTo(cv::Scalar::all(0));

  static const double GRAPH_HEIGHT = 200.0f;
  static const double GRAPH_WIDTH = 600.0f;
  static const double DX = GRAPH_WIDTH/MOTION_SAMPLES;
  static const double BASE_X = 20.0f, BASE_Y = 10.0f;
  // colors are in BGR format
  static const cv::Scalar WHITE(255,255,255);
  static const cv::Scalar YELLOW(0,255,255);
  static const cv::Scalar RED(0,0,255);
  static const cv::Scalar GREEN(0,255,0);
  auto toInt = [](double x){return (int)std::round(x);};
  auto point = [&](double x, double y) {
    return cv::Point(toInt(x), toInt(y));
  };
  static const cv::Rect GRAPH_BORDER(
    toInt(BASE_Y - 1.0),       toInt(BASE_X - 1.0),
    toInt(GRAPH_WIDTH + 2.0),  toInt(GRAPH_HEIGHT + 2.0));
  cv::rectangle(stats_window, GRAPH_BORDER, WHITE);
  auto valueToHeight = 
    [&] (double value) {
      const double MAX = 1.5*motion_threshold;
      return BASE_Y + GRAPH_HEIGHT - 
        (std::min(value,MAX)/MAX)*GRAPH_HEIGHT;
    };
  auto threshold_y = valueToHeight(motion_threshold);
  cv::line(stats_window, 
    point(BASE_X,               threshold_y),
    point(BASE_X + GRAPH_WIDTH, threshold_y),
    YELLOW);
  auto colorForValue = [&](double value) {
    double t = std::min(1.0,value/motion_threshold);
    return cv::Scalar(
      toInt((1.0-t*t)*255.0),
      0,
      toInt(t*t*255.0)
    );
  };
  cv::Point last = point(valueToHeight(motion_samples.oldest()),BASE_X);
  motion_samples.for_each(
    [&] (const double &y) {
      cv::Point this_point = point(last.x + DX, valueToHeight(y));

      // auto &color = 
      //   d >= motion_threshold ? RED :
      //   d >= 0.8*motion_threshold ? YELLOW :
      //   GREEN;
      cv::line(stats_window,last,this_point,colorForValue(y));
      last = this_point;
    });

  auto last_motion = motion_samples.newest();
  auto s = format(last_motion,0,3);
  cv::putText(stats_window, s, 
    cv::Point(20,20), cv::FONT_HERSHEY_PLAIN, 1.0, colorForValue(last_motion));
  if (video_offset != 0.0) {
    std::stringstream voff;
    voff << "recording (" << format(video_offset,0,1) << ")";
    cv::putText(stats_window, voff.str(), 
      cv::Point(20,60), cv::FONT_HERSHEY_PLAIN, 1.0, RED);
  }

  cv::imshow("stats",stats_window);
  cv::imshow("current frame",color_frames.newest());
  cv::imshow("motion",absdiff);

  hud_draw_cost_estimate.stop();
}

void motion_detector::run() {
  // prime it by burning some frames
  // the lighting adjusts as the program starts up and this causes spikes
  log("warming up");
  auto warmup_start = uptime();
  while (uptime() - warmup_start < os.startup_delay) {
    auto &curr_frame = capture_frame();
    cv::imshow("current frame",curr_frame);
  }

  reset_background(0,"initial background");
  if (!os.has_custom_motion_threshold)
    calibrate_motion_threshold();

  log("running");

  while (!exit_detector) {
    (void)capture_frame();
    bool motion = detecting_motion();
    if (hud_enabled) {
      draw_hud();
    }

    if (motion) {
      capture_video("motion detected");
      if (next_video_index == os.max_videos) {
        log("exiting because we created the maximum number of videos");
        exit_detector = true;
      }
      // force a reset after the capture since this could be a spurious hit
      // from lights being turned on or something; this prevents some change
      // from spamming the motion detection
      reset_background(0,"motion detected");
    } else {
      auto key = cv::waitKey((int)FRAME_BUDGET_MS);
      process_key(key);
      if (key == 'c') {
        capture_video("forced");
      }
    }
    if (color_frames.total % (4*32)) { // about 4s
      join_finished_asyncs();
      log_stream.flush();
    }
  } // while
}

void motion_detector::process_key(int key)
{
  auto toggle = 
    [&] (const char *name, bool &z) {
      z = !z;
      std::string nm = name;
      nm += ": ";
      std::cout << std::setw(32) << std::left << nm << format(z) << "\n";
    };
  if ( key == 'q' || key == ESC_KEY) {
    log("exit requested");
    exit_detector = true; // esc or q
  } else if (key == 'r' || key == 'R') {
    reset_background(key == 'r' ? os.startup_delay : 0,"forced");
  } else if (key == 'k') {
    log("recalibrating of motion threshold (forced)");
    calibrate_motion_threshold();
  } else if (key == 'h') {
    toggle("hud_enabled",hud_enabled);
    if (!hud_enabled) {
      cv::destroyAllWindows();
    }
  } else if (key == 'v') {
    toggle("vidcap_disabled",vidcap_disabled);
  } else if (key == 'c') {
    // capture_video();
    // handled by the parent (might be in capture_video or run)
    return;
  } else if (key == 'd') {
    std::cout << "uptime:                 " << format(uptime()) << " s\n";
    std::cout << "frame index:            " << color_frames.total << "\n";   
    std::cout << "\n";
    std::cout << "motion_threshold:       " << format(motion_threshold) << "\n";
    std::cout << "\n";
    std::cout << "est. mdet   cost:       " << format(motion_cost_estimate.average_ms(),0,1) << " ms\n";
    std::cout << "est. draw   cost:       " << format(hud_draw_cost_estimate.average_ms(),0,1) << " ms\n";
    std::cout << "est. frame ovrhd:       " << format(frame_overhead_estimate.average_ms(),0,1) << " ms\n";
    std::cout << "\n";
    std::cout << "hud_enabled             " << format(hud_enabled) << "\n";
    std::cout << "vidcap_disabled         " << format(vidcap_disabled) << "\n";
    std::cout << "motion diffs\n";
    std::cout << "   buffer avg:          " << format(motion_samples.average(),0,3) << "\n";
    std::cout << "   min:                 " << format(min_motion_diff,0,3) << "\n";
    std::cout << "   max:                 " << format(max_motion_diff,0,3) << "\n";
    //
    std::cout << "copy_threads:           " << copy_threads.size() << "\n";
    for (const auto *ct : copy_threads) {
      std::cout << "  * " << ct->target_file_name << 
        " (" << format(ct->done);
      if (ct->done && !ct->error_message.empty()) {
        std::cout << " " << ct->error_message;
      }
      std::cout << ")\n";
    }
    log_stream.flush();
  } else if (key != -1) {
    if (key != 'h' && key != '?')
      std::cout << "unrecognized key: 0x" << std::hex << key << std::dec << "\n";
    std::cout <<
      "keys are:\n"
      "  c     - forces video capture (or stops running capture)\n"
      "  d     - dumps debug info to stdout\n"
      "  k     - forces recalibration of motion threshold\n"
      "  h     - toggles the stats HUD\n"
      "  q/ESC - quits\n"
      "  r/R   - resets the background image with a delay (R for no delay)\n"
      "  v     - disables/enables video recording\n";
  }
}


void motion_detector::logs(const std::string &msg)
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
  char tbuf[128];
  std::strftime(tbuf, sizeof(tbuf), "%Y-%m-%d-%H:%M:%S",&t);

  // appending milliseconds: this is hard; strftime won't work and
  // gettimeofday is Unix only.  So we will use the fractional part of 
  // uptime.  The intent is to show relative differences.  So the uptime
  // clock in seconds is good enough.
  //
  // (C++20 adds STL stuff to solve all this)
  double up = uptime();
  up = up - (int64_t)up;
  std::stringstream ss_message;
  ss_message << 
    tbuf <<
    std::fixed << std::setprecision(3) << up;
  ss_message << ": " << msg << "\n";
  auto message = ss_message.str();
  log_stream << message;
  std::cout << message; 
}

void motion_detector::fatals(const std::string &msg)
{
  log("FATAL ERROR: ",msg);
  log_stream.flush();
  ::fatal(msg);
}
