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
  if (thread_.joinable())
    return;
  stopClicked_ = false;
  thread_ = std::thread(&GPSProvider::run, this);
}

void GPSProvider::stop() {
  stopClicked_ = true;
  if (thread_.joinable()) {
    thread_.join();
  }
}

void GPSProvider::run() {
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
}

void GPSProvider::processLine(const std::string &line) {
  try {
    auto j = nlohmann::json::parse(line);
    if (j.value("class", "") != "TPV") return;

    // Require 2D or 3D fix
    if (j.value("mode", 0) < 2) return;
    if (!j.contains("lat") || !j.contains("lon")) return;

    double lat = j["lat"].get<double>();
    double lon = j["lon"].get<double>();
    if (lat == 0.0 && lon == 0.0) return; // gpsd sometimes sends zeroed pre-fix data

    LOG_D("GPS", "TPV fix: lat={:.5f} lon={:.5f}", lat, lon);

    // Throttle: no more than once per 60 seconds
    auto now = std::chrono::steady_clock::now();
    if (now - lastUpdate_ < std::chrono::seconds(60)) return;

    // Distance gate: skip update if < 500 m from current DE (prevents GPS jitter)
    LatLon newLoc{lat, lon};
    bool firstFix = (lastUpdate_.time_since_epoch().count() == 0);
    if (!firstFix && Astronomy::calculateDistance(state_->deLocation, newLoc) < 0.5)
      return;

    // Apply update
    state_->deLocation = newLoc;
    config_.lat  = lat;
    config_.lon  = lon;
    config_.grid = Astronomy::latLonToGrid(lat, lon);
    state_->deGrid = config_.grid;
    lastUpdate_  = now;

    LOG_I("GPS", "DE updated from GPS fix: {:.5f},{:.5f} grid={}", lat, lon, config_.grid);
  } catch (...) {
    // Ignore JSON parse errors
  }
}
