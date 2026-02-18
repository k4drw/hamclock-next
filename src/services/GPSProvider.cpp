#include "GPSProvider.h"
#include "../core/Logger.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <cstring>
#include <nlohmann/json.hpp>

using namespace HamClock;

GPSProvider::GPSProvider(HamClockState *state, AppConfig &config)
    : state_(state), config_(config) {}

GPSProvider::~GPSProvider() { stop(); }

void GPSProvider::start() {
#ifndef __EMSCRIPTEN__
  if (thread_.joinable())
    return;
  stopClicked_ = false;
  thread_ = std::thread(&GPSProvider::run, this);
#endif
}

void GPSProvider::stop() {
#ifndef __EMSCRIPTEN__
  stopClicked_ = true;
  if (thread_.joinable()) {
    thread_.join();
  }
#endif
}

void GPSProvider::run() {
#ifndef __EMSCRIPTEN__
  while (!stopClicked_) {
    if (!config_.gpsEnabled) {
      std::this_thread::sleep_for(std::chrono::seconds(5));
      continue;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
      std::this_thread::sleep_for(std::chrono::seconds(10));
      continue;
    }

    struct hostent *he = gethostbyname("localhost");
    if (!he) {
#ifdef _WIN32
      closesocket(sock);
#else
      close(sock);
#endif
      std::this_thread::sleep_for(std::chrono::seconds(10));
      continue;
    }

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(2947); // gpsd port
    std::memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
#ifdef _WIN32
      closesocket(sock);
#else
      close(sock);
#endif
      std::this_thread::sleep_for(
          std::chrono::seconds(30)); // gpsd likely not running
      continue;
    }

    LOG_I("GPS", "Connected to gpsd on localhost:2947");

    // Request JSON updates
    const char *watch_cmd = "?WATCH={\"enable\":true,\"json\":true};\r\n";
    send(sock, watch_cmd, std::strlen(watch_cmd), 0);

    // ... (rest of the run implementation)
    std::string buffer;
    char chunk[1024];

    while (!stopClicked_ && config_.gpsEnabled) {
#ifdef _WIN32
      WSAPOLLFD pfd{};
      pfd.fd = sock;
      pfd.events = POLLIN;
      int ret = WSAPoll(&pfd, 1, 1000);
#else
      struct pollfd pfd{};
      pfd.fd = sock;
      pfd.events = POLLIN;
      int ret = poll(&pfd, 1, 1000);
#endif

      if (ret > 0) {
        int n = recv(sock, chunk, sizeof(chunk) - 1, 0);
        if (n <= 0)
          break; // Disconnected
        chunk[n] = '\0';
        buffer += chunk;

        size_t pos;
        while ((pos = buffer.find('\n')) != std::string::npos) {
          std::string line = buffer.substr(0, pos);
          buffer.erase(0, pos + 1);
          processLine(line);
        }
      } else if (ret < 0) {
        break;
      }
    }

#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif
    LOG_I("GPS", "Disconnected from gpsd");
  }
#endif
}

void GPSProvider::processLine(const std::string &line) {
#ifndef __EMSCRIPTEN__
  try {
    auto j = nlohmann::json::parse(line);
    std::string class_name = j.value("class", "");

    if (class_name == "TPV") {
      // Time-Position-Velocity
      if (j.contains("lat") && j.contains("lon")) {
        double lat = j["lat"];
        double lon = j["lon"];
        LOG_D("GPS", "TPV: lat={}, lon={}", lat, lon);
        // In a real implementation, we'd update state_->deLat/deLon here
        // if a "Follow GPS" setting is active.
      }
    }
  } catch (...) {
    // Ignore parse errors
  }
#else
  (void)line;
#endif
}
