#include "mdet.hpp"

#include <string>
#include <thread>
#if __has_include(<filesystem>)
#include <filesystem>
// different versions of VS2017 have this in different namespaces
// even with the top-level header
// namespace fs = std::filesystem;
namespace fs = std::experimental::filesystem;
#elif __has_include(<experimental/filesystem>)
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#else
#error "cannot find a std::filesystem header"
#endif
#include <system_error>


static void run_copy_thread(copy_thread *ct) {
  ct->run();
}

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

copy_thread::copy_thread(
  std::string _file_name,
  std::string _to_dir) 
    : source_file_name(_file_name)
    , target_file_name(make_target_path(_file_name,_to_dir))
    , thread(run_copy_thread, this)
{
}

void copy_thread::run() {

  try {
    fs::copy(
      source_file_name,
      target_file_name,
      fs::copy_options::overwrite_existing);
  } catch(fs::filesystem_error &fse) {
    error_message = fse.what();
  } catch(...) {
    error_message = "copy failed (unknown error)";
  }
  done = true;
}


