#include <cstdlib>
#include <iostream>
namespace helper {
inline std::string getEnvVarValue(const std::string &name) {
  auto env = getenv(name.data());
  if (env == nullptr) {
    return "";
  }
  return env;
}
} // namespace helper
