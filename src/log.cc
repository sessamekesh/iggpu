#include <iggpu/iggpu_config.h>
#include <iggpu/log.h>

#ifdef IGGPU_ENABLE_DEFAULT_LOGGING
#include <cstdio>
#endif

namespace {

void null_log_fn(iggpu::LogLevel, const std::string&) {}

void default_print_cb(iggpu::LogLevel log_level, const std::string& msg) {
#ifdef IGGPU_ENABLE_DEFAULT_LOGGING
  switch (log_level) {
    case iggpu::LogLevel::Error:
      std::fprintf(stderr, "%s", msg.c_str());
      return;
    case iggpu::LogLevel::Warning:
    case iggpu::LogLevel::Info:
      std::fprintf(stdout, "%s", msg.c_str());
  }
#endif
}

std::function<void(iggpu::LogLevel, const std::string&)> gLogFn =
    ::default_print_cb;

}  // namespace

void iggpu::set_log_fn(
    std::function<void(LogLevel, const std::string&)> log_fn) {
  if (!log_fn) {
    ::gLogFn = ::null_log_fn;
    return;
  }

  ::gLogFn = log_fn;
}

void iggpu::log(LogLevel log_level, const std::string& msg) {
  ::gLogFn(log_level, msg);
}
