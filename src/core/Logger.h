#pragma once

#include <fmt/format.h>
#include <memory>
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

  // Categorized logging. We format the entire message into a string first
  // to bypass spdlog's consteval validation of the format string.
  // This is necessary in C++20 where fmt's compile-time checking is very
  // strict.
  template <typename... Args>
  static void t(const std::string &cat, const std::string &f, Args &&...args) {
    if (s_Logger && s_Logger->should_log(spdlog::level::trace)) {
      std::string msg =
          fmt::format(fmt::runtime("[{}] {}"), cat,
                      fmt::vformat(f, fmt::make_format_args(args...)));
      s_Logger->log(spdlog::level::trace, msg);
    }
  }
  template <typename... Args>
  static void d(const std::string &cat, const std::string &f, Args &&...args) {
    if (s_Logger && s_Logger->should_log(spdlog::level::debug)) {
      std::string msg =
          fmt::format(fmt::runtime("[{}] {}"), cat,
                      fmt::vformat(f, fmt::make_format_args(args...)));
      s_Logger->log(spdlog::level::debug, msg);
    }
  }
  template <typename... Args>
  static void i(const std::string &cat, const std::string &f, Args &&...args) {
    if (s_Logger && s_Logger->should_log(spdlog::level::info)) {
      std::string msg =
          fmt::format(fmt::runtime("[{}] {}"), cat,
                      fmt::vformat(f, fmt::make_format_args(args...)));
      s_Logger->log(spdlog::level::info, msg);
    }
  }
  template <typename... Args>
  static void w(const std::string &cat, const std::string &f, Args &&...args) {
    if (s_Logger && s_Logger->should_log(spdlog::level::warn)) {
      std::string msg =
          fmt::format(fmt::runtime("[{}] {}"), cat,
                      fmt::vformat(f, fmt::make_format_args(args...)));
      s_Logger->log(spdlog::level::warn, msg);
    }
  }
  template <typename... Args>
  static void e(const std::string &cat, const std::string &f, Args &&...args) {
    if (s_Logger && s_Logger->should_log(spdlog::level::err)) {
      std::string msg =
          fmt::format(fmt::runtime("[{}] {}"), cat,
                      fmt::vformat(f, fmt::make_format_args(args...)));
      s_Logger->log(spdlog::level::err, msg);
    }
  }
  template <typename... Args>
  static void c(const std::string &cat, const std::string &f, Args &&...args) {
    if (s_Logger && s_Logger->should_log(spdlog::level::critical)) {
      std::string msg =
          fmt::format(fmt::runtime("[{}] {}"), cat,
                      fmt::vformat(f, fmt::make_format_args(args...)));
      s_Logger->log(spdlog::level::critical, msg);
    }
  }

  // Redirect standard macros through categorized helpers to ensure consistent
  // bypass of spdlog/fmt consteval validation for literal strings in C++20.
#define LOG_TRACE(...) ::Log::t("Main", __VA_ARGS__)
#define LOG_DEBUG(...) ::Log::d("Main", __VA_ARGS__)
#define LOG_INFO(...) ::Log::i("Main", __VA_ARGS__)
#define LOG_WARN(...) ::Log::w("Main", __VA_ARGS__)
#define LOG_ERROR(...) ::Log::e("Main", __VA_ARGS__)
#define LOG_CRITICAL(...) ::Log::c("Main", __VA_ARGS__)

#define LOG_T(cat, f, ...) ::Log::t(cat, f, ##__VA_ARGS__)
#define LOG_D(cat, f, ...) ::Log::d(cat, f, ##__VA_ARGS__)
#define LOG_I(cat, f, ...) ::Log::i(cat, f, ##__VA_ARGS__)
#define LOG_W(cat, f, ...) ::Log::w(cat, f, ##__VA_ARGS__)
#define LOG_E(cat, f, ...) ::Log::e(cat, f, ##__VA_ARGS__)

private:
  static std::shared_ptr<spdlog::logger> s_Logger;
};
