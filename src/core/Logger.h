#pragma once

#include <deque>
#include <fmt/format.h>
#include <memory>
#include <mutex>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <string>
#include <vector>

// In-memory circular log buffer sink (last kCapacity lines).
class RingBufferSink : public spdlog::sinks::base_sink<std::mutex> {
public:
  static constexpr int kCapacity = 200;

  std::vector<std::string> getLines() const {
    std::lock_guard<std::mutex> lk(bufMutex_);
    return {buf_.begin(), buf_.end()};
  }

protected:
  void sink_it_(const spdlog::details::log_msg &msg) override {
    spdlog::memory_buf_t formatted;
    base_sink<std::mutex>::formatter_->format(msg, formatted);
    std::lock_guard<std::mutex> lk(bufMutex_);
    if ((int)buf_.size() >= kCapacity)
      buf_.pop_front();
    buf_.push_back(fmt::to_string(formatted));
  }
  void flush_() override {}

private:
  mutable std::mutex bufMutex_;
  std::deque<std::string> buf_;
};

class Log {
public:
  static void init(const std::string &fallbackDir = "");

  static std::shared_ptr<spdlog::logger> &get() { return s_Logger; }

  // Return the last N log lines from the in-memory ring buffer.
  static std::vector<std::string> getRecentLogs() {
    if (s_RingSink)
      return s_RingSink->getLines();
    return {};
  }

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
  static std::shared_ptr<RingBufferSink> s_RingSink;
};
