#ifndef IGGPU_LOG_H
#define IGGPU_LOG_H

#include <functional>
#include <string>

namespace iggpu {

enum class LogLevel {
  Error,
  Warning,
  Info,
};

void set_log_fn(std::function<void(LogLevel, const std::string&)> log_fn);
void log(LogLevel log_level, const std::string& msg);

}  // namespace iggpu

#endif
