#pragma once
#include <filesystem>
#include <iostream>
#include <vector>

namespace fs = std::filesystem;

namespace file_helpers {
inline bool isExecutable(const fs::path &p) {
  fs::file_status s = fs::status(p);
  auto perms = s.permissions();

  // Check if any execute bit is set (owner, group, or others)
  return (perms & fs::perms::owner_exec) != fs::perms::none ||
         (perms & fs::perms::group_exec) != fs::perms::none ||
         (perms & fs::perms::others_exec) != fs::perms::none;
}
inline std::string getExecutableCommandPath(const std::string &command) {
  const char *env = std::getenv("PATH");
  std::string path = env;
  size_t start = 0;

  while (true) {
    size_t end = path.find(':', start);
    std::string dir = path.substr(start, end - start);

    if (end == std::string::npos) {
      break;
    }

    fs::path fullPath = fs::path(dir) / command;

    if (fs::exists(fullPath) && fs::is_regular_file(fullPath) &&
        isExecutable(fullPath)) {
      return fullPath.string();
    }
    start = end + 1;
  }
  return "";
}
inline std::string getCurrentWorkingDirectory() {
  try {
    fs::path currentDir = fs::current_path();
    return currentDir.string();
  } catch (const fs::filesystem_error &e) {
    std::cerr << "Error: " << e.what() << std::endl;
  }
  return "";
}
inline bool isEmpty(const fs::path &file) {
  return fs::exists(file) && fs::file_size(file) == 0;
}
inline std::vector<std::string> getCurrentDirectoryFiles() {
  std::vector<std::string> result{};
  fs::path currentDir = fs::current_path();
  for (const auto &entry : fs::directory_iterator(currentDir)) {
    if (entry.path().filename().extension() == ".txt") {
      result.push_back(entry.path().filename().string());
    }
  }
  return result;
}
} // namespace file_helpers
