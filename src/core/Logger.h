#pragma once

#include <memory>
#include <spdlog/fmt/fmt.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <string>

class Log {
public:
  static void init(const std::string &fallbackDir = "");

  static std::shared_ptr<spdlog::logger> &get() { return s_Logger; }

  // Set log level at runtime
  static void setLevel(spdlog::level::level_enum level) {
    if (s_Logger) {
      s_Logger->set_level(level);
    }
  }

  // High-level macros for simple logging
#define LOG_TRACE(...) ::Log::get()->trace(__VA_ARGS__)
#define LOG_DEBUG(...) ::Log::get()->debug(__VA_ARGS__)
#define LOG_INFO(...) ::Log::get()->info(__VA_ARGS__)
#define LOG_WARN(...) ::Log::get()->warn(__VA_ARGS__)
#define LOG_ERROR(...) ::Log::get()->error(__VA_ARGS__)
#define LOG_CRITICAL(...) ::Log::get()->critical(__VA_ARGS__)

  // Categorized logging with runtime format strings to avoid C++20 consteval
  // escalation issues in lambdas.
  template <typename... Args>
  static void t(const std::string &cat, const std::string &f, Args &&...args) {
    s_Logger->trace(spdlog::fmt_lib::runtime("[{}] " + f), cat,
                    std::forward<Args>(args)...);
  }
  template <typename... Args>
  static void d(const std::string &cat, const std::string &f, Args &&...args) {
    s_Logger->debug(spdlog::fmt_lib::runtime("[{}] " + f), cat,
                    std::forward<Args>(args)...);
  }
  template <typename... Args>
  static void i(const std::string &cat, const std::string &f, Args &&...args) {
    s_Logger->info(spdlog::fmt_lib::runtime("[{}] " + f), cat,
                   std::forward<Args>(args)...);
  }
  template <typename... Args>
  static void w(const std::string &cat, const std::string &f, Args &&...args) {
    s_Logger->warn(spdlog::fmt_lib::runtime("[{}] " + f), cat,
                   std::forward<Args>(args)...);
  }
  template <typename... Args>
  static void e(const std::string &cat, const std::string &f, Args &&...args) {
    s_Logger->error(spdlog::fmt_lib::runtime("[{}] " + f), cat,
                    std::forward<Args>(args)...);
  }

#define LOG_T(cat, f, ...) ::Log::t(cat, f, ##__VA_ARGS__)
#define LOG_D(cat, f, ...) ::Log::d(cat, f, ##__VA_ARGS__)
#define LOG_I(cat, f, ...) ::Log::i(cat, f, ##__VA_ARGS__)
#define LOG_W(cat, f, ...) ::Log::w(cat, f, ##__VA_ARGS__)
#define LOG_E(cat, f, ...) ::Log::e(cat, f, ##__VA_ARGS__)

private:
  static std::shared_ptr<spdlog::logger> s_Logger;
};
