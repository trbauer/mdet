#ifndef FS_HPP
#define FS_HPP

#include <string>

namespace fs {
  // Wrapper to std::filesystem since it's still a little skitzo on some
  // compilers (*cough* *cough* Visual Studio); it's been moved around
  // a bit or even absent.   My MinGW version also seemed to totally omit it.
  //
  // Once std::filesystem is a little more stable, we'll just re-alias
  // everything and simplify this part. E.g.
  //    namespace fs = std::filesystem
  //
  using path = std::string; // std::filesystem::path

  // Combines dir and file into platform specific dir/file
  // careful to deal with possible trailing / (or \\ on windows)
  // just a path join
  //
  // also allows empty dir, giving just file
  path join_path(const path &dir, const path &file);

  // std::filesystem::copy; exceptions converted to strings
  void copy_overwrite_with_error_message(
    const path &source_file,
    const path &target_file,
    std::string &error_message);

  // std::filesystem::create_directories (includes parent directories)
  // nop if directory already exists
  void create_directory_if_absent(const path &dir, std::string &error);

  // std::filesystem::path::is_absolute
  bool is_absolute_path(const path &p);

  // std::filesystem::path::is_absolute
  bool directory_exists(const path &p);

  // removes a file if already exists (e.g. so we get a fresh create stamp)
  void remove_if_exists(const path &p);
} // fs::

#endif