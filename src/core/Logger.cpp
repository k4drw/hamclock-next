#include "Logger.h"
#include <filesystem>
#include <spdlog/sinks/rotating_file_sink.h>

#ifdef _WIN32
#include <io.h>
#define access _access
#define W_OK 2
#else
#include <unistd.h>
#endif

std::shared_ptr<spdlog::logger> Log::s_Logger;

void Log::init(const std::string &fallbackDir) {
  std::fprintf(stderr, "Initializing spdlog...\n");
  spdlog::set_pattern("%^[%Y-%m-%d %H:%M:%S.%e] [%l] %v%$");

  std::vector<spdlog::sink_ptr> sinks;

  // 1. Stderr Color Sink (Standard for journalctl/console)
  sinks.push_back(std::make_shared<spdlog::sinks::stderr_color_sink_mt>());

  // 2. Rotating File Sink
  std::filesystem::path primaryPath = "/var/log/hamclock";
  std::filesystem::path logFile;

  std::error_code ec;
  if (std::filesystem::exists(primaryPath, ec) &&
      access(primaryPath.string().c_str(), W_OK) == 0) {
    logFile = primaryPath / "hamclock.log";
  } else if (!fallbackDir.empty()) {
    logFile = std::filesystem::path(fallbackDir) / "hamclock.log";
  }

  if (!logFile.empty()) {
    try {
      // 5MB per file, 3 rotated files max (15MB total)
      auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
          logFile.string(), 5 * 1024 * 1024, 3);
      sinks.push_back(fileSink);
      std::fprintf(stderr, "Logging to file: %s\n", logFile.string().c_str());
    } catch (const spdlog::spdlog_ex &ex) {
      std::fprintf(stderr, "Log initialization failed: %s\n", ex.what());
    }
  }

  s_Logger =
      std::make_shared<spdlog::logger>("HAMCLOCK", sinks.begin(), sinks.end());
  // Default to WARN level - use --log-level to change
  s_Logger->set_level(spdlog::level::warn);
  spdlog::flush_on(spdlog::level::warn);

  // This will only show if log level is set to INFO or DEBUG via --log-level
  LOG_INFO("Logger initialized with {} sinks", sinks.size());
  std::fprintf(stderr, "spdlog initialized successfully.\n");
}
