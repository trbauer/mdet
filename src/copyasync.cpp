#include "mdet.hpp"
#include "fs.hpp"

#include <string>
#include <thread>


static void run_copy_thread(copy_thread *ct) {
  ct->run();
}

/*
static std::string make_target_path(
  std::string _file_name,
  std::string _target_dir)
{
  std::string remote_file = _target_dir;
  if (!remote_file.empty()) {
    char last_char = remote_file[remote_file.size() - 1];
    if (last_char != '\\' && last_char != '/')
      remote_file += fs::path::preferred_separator;
  }
  size_t file_name_start_off = 0;
  if (!remote_file.empty()) {
    for (int i = (int)remote_file.size() - 1; i >= 0; i--) {
      char c = _file_name[i];
      if (c == '\\' || c == '/') {
        file_name_start_off = (int)(i + 1);
        break;
      }
    }
    remote_file += _file_name.substr(file_name_start_off);
  }
  return remote_file;
}
*/

copy_thread::copy_thread(
  std::string _file_name, // video00001.mp4
  std::string _to_dir)    // t:\mybackup
    : source_file_name(_file_name)
    , target_file_name(fs::join_path(_to_dir,_file_name))
    , thread(run_copy_thread, this)
{
}

void copy_thread::run() {
  fs::copy_overwrite_with_error_message(
    source_file_name, target_file_name, error_message);
  done = true;
}


