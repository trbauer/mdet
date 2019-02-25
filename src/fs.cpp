#include "fs.hpp"

#if __has_include(<filesystem>)
#include <filesystem>
// different versions of VS2017 have this in different namespaces
// even with the top-level header
// namespace fs = std::filesystem;
namespace sfs = std::experimental::filesystem;
#elif __has_include(<experimental/filesystem>)
#include <experimental/filesystem>
namespace sfs = std::experimental::filesystem;
#else
#error "cannot find a std::filesystem header"
#endif


// combines dir and file into platform specific dir/file
// careful to deal with possible trailing / (or \\ on windows)
// just a path join
fs::path fs::join_path(const fs::path &dir, const fs::path &file)
{
  if (dir.empty()) {
    return file;
  } else {
    auto path = sfs::path(dir) / sfs::path(file);
    return path.string();
  }
}

void fs::copy_overwrite_with_error_message(
  const path &source_file,
  const path &target_file,
  std::string &error_message)
{
  try {
    sfs::copy(
      source_file,
      target_file,
      sfs::copy_options::overwrite_existing);
  } catch(sfs::filesystem_error &fse) {
    error_message = fse.what();
  } catch(...) {
    error_message = "copy failed (unknown error)";
  }
}

void fs::create_directory_if_absent(
  const fs::path &dir, 
  std::string &error_message)
{
  if (directory_exists(dir)) {
    // if started via Task Scheduler the nop fails
    return;
  }
  try {
    sfs::create_directories(dir);
  } catch(sfs::filesystem_error &fse) {
    error_message = fse.what();
  } catch(...) {
    error_message = "copy failed (unknown error)";
  }
}

bool fs::is_absolute_path(const fs::path &p) {
  return sfs::path(p).is_absolute();
}

bool fs::directory_exists(const fs::path &p) {
  return sfs::is_directory(sfs::path(p));
}