#include "DXClusterProvider.h"
#include "../core/Astronomy.h"
#include "../core/HamClockState.h"
#include "../core/Logger.h"
#include "../core/PrefixManager.h"
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#define close closesocket
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
#include <cstring>

DXClusterProvider::DXClusterProvider(std::shared_ptr<DXClusterDataStore> store,
                                     PrefixManager &pm,
                                     std::shared_ptr<WatchlistStore> watchlist,
                                     std::shared_ptr<WatchlistHitStore> hits,
                                     HamClockState *state)
    : store_(store), pm_(pm), watchlist_(watchlist), hits_(hits),
      state_(state) {}

DXClusterProvider::~DXClusterProvider() { stop(); }

void DXClusterProvider::start(const AppConfig &config) {
  if (running_)
    stop();

  config_ = config;
  if (!config_.dxClusterEnabled)
    return;

  running_ = true;
  stopClicked_ = false;
  thread_ = std::thread(&DXClusterProvider::run, this);
}

void DXClusterProvider::stop() {
  stopClicked_ = true;
  if (thread_.joinable()) {
    thread_.join();
  }
  running_ = false;
}

void DXClusterProvider::run() {
  while (!stopClicked_) {
    if (config_.dxClusterUseWSJTX) {
      runUDP(config_.dxClusterPort);
    } else {
      runTelnet(config_.dxClusterHost, config_.dxClusterPort,
                config_.dxClusterLogin);
    }

    if (stopClicked_)
      break;

    // Retry delay
    store_->setConnected(false, "Disconnected, retrying in 10s...");
    std::this_thread::sleep_for(std::chrono::seconds(10));
  }
}

void DXClusterProvider::runTelnet(const std::string &host, int port,
                                  const std::string &login) {
  LOG_I("DXCluster", "Connecting to {}:{}", host, port);
  if (state_) {
    auto &s = state_->services["DXCluster"];
    s.ok = false;
    s.lastError = "Connecting...";
  }

  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    LOG_E("DXCluster", "Failed to create socket");
    if (state_)
      state_->services["DXCluster"].lastError = "Socket error";
    return;
  }

  // Set non-blocking for connect timeout if needed, but for now just simple
  struct hostent *he = gethostbyname(host.c_str());
  if (!he) {
    LOG_E("DXCluster", "Could not resolve {}", host);
    if (state_)
      state_->services["DXCluster"].lastError = "DNS failed";
    close(sock);
    return;
  }

  struct sockaddr_in server_addr{};
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  std::memcpy(&server_addr.sin_addr, he->h_addr_list[0], he->h_length);

  if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
#ifdef _WIN32
    int err = WSAGetLastError();
    LOG_E("DXCluster", "Connect to {} failed: error {}", host, err);
#else
    LOG_E("DXCluster", "Connect to {} failed: {}", host, std::strerror(errno));
#endif
    if (state_)
      state_->services["DXCluster"].lastError = "Connect failed";
    close(sock);
    return;
  }

#ifdef _WIN32
  unsigned long mode = 1;
  ioctlsocket(sock, FIONBIO, &mode);
#else
  fcntl(sock, F_SETFL, O_NONBLOCK);
#endif

  LOG_I("DXCluster", "Connected to {}", host);
  if (state_)
    state_->services["DXCluster"].lastError = "Connected";
  store_->setConnected(true, "Connected to " + host);

  std::string buffer;
  bool loggedIn = login.empty();
  bool initialRequestSent = false;
  auto lastHeartbeat = std::chrono::system_clock::now();

  // Following original HamClock logic: send login immediately as prompts may
  // not have newlines
  if (!login.empty()) {
    std::string cmd = login + "\r\n";
    send(sock, cmd.c_str(), cmd.length(), 0);
    // Note: Don't set loggedIn = true yet, we want to see if we get a "Welcome"
    // or prompt
  }

  while (!stopClicked_) {
#ifdef _WIN32
    WSAPOLLFD pfd{};
    pfd.fd = sock;
    pfd.events = POLLIN;
    int ret = WSAPoll(&pfd, 1, 500);
#else
    struct pollfd pfd{};
    pfd.fd = sock;
    pfd.events = POLLIN;
    int ret = poll(&pfd, 1, 500); // 500ms timeout
#endif
    if (ret < 0)
      break;

    if (ret > 0) {
      char tmp[1024];
      ssize_t n = recv(sock, tmp, sizeof(tmp) - 1, 0);
      if (n <= 0) {
        LOG_W("DXCluster", "Connection lost");
        if (state_) {
          state_->services["DXCluster"].ok = false;
          state_->services["DXCluster"].lastError = "Connection lost";
        }
        break; // Error or closed
      }

      tmp[n] = '\0';
      buffer.append(tmp, n);

      size_t pos;
      while ((pos = buffer.find('\n')) != std::string::npos) {
        std::string line = buffer.substr(0, pos);
        buffer.erase(0, pos + 1);
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
          line.pop_back();

        if (!line.empty()) {
          processLine(line);

          // Check for common indicators that we are in
          if (line.find("Welcome") != std::string::npos ||
              line.find("connected") != std::string::npos ||
              line.find("Nodes") != std::string::npos ||
              line.find(">") != std::string::npos ||
              line.find("DX de ") !=
                  std::string::npos) { // Spot line also means we are in
            if (!loggedIn) {
              loggedIn = true;
              if (state_) {
                auto &s = state_->services["DXCluster"];
                s.ok = true;
                s.lastSuccess = std::chrono::system_clock::now();
              }
              store_->setConnected(true, "Logged in as " + login);
            }
            if (!initialRequestSent) {
              const char *req = "sh/dx 30\r\n";
              send(sock, req, std::strlen(req), 0);
              initialRequestSent = true;
            }
          }

          if (!loggedIn) {
            // Still check for login prompt just in case we didn't send it or it
            // asked again
            if (line.find("login:") != std::string::npos ||
                line.find("callsign:") != std::string::npos ||
                line.find("Please enter your call:") != std::string::npos) {
              std::string cmd = login + "\r\n";
              send(sock, cmd.c_str(), cmd.length(), 0);
            }
          }
        }
      }

      // Check for prompt without newline at the end of buffer
      if (!loggedIn && !buffer.empty()) {
        if (buffer.find("login:") != std::string::npos ||
            buffer.find("callsign:") != std::string::npos ||
            buffer.find("Please enter your call:") != std::string::npos) {
          std::string cmd = login + "\r\n";
          send(sock, cmd.c_str(), cmd.length(), 0);
          buffer.clear(); // Clear so we don't repeat
        }
      }

      // If still buffer too long without newline, clear it
      if (buffer.length() > 4096)
        buffer.clear();
    }

    // Heartbeat
    auto now = std::chrono::system_clock::now();
    if (now - lastHeartbeat > std::chrono::seconds(60)) {
      send(sock, "\r\n", 2, 0);
      lastHeartbeat = now;
    }
  }

  std::fprintf(stderr, "DXCluster: telnet session ended\n");
  close(sock);
}

void DXClusterProvider::runUDP(int port) {
  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0)
    return;

  struct sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = INADDR_ANY;

  if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    close(sock);
    return;
  }

#ifdef _WIN32
  unsigned long mode = 1;
  ioctlsocket(sock, FIONBIO, &mode);
#else
  fcntl(sock, F_SETFL, O_NONBLOCK);
#endif
  store_->setConnected(true, "Listening UDP on port " + std::to_string(port));

  while (!stopClicked_) {
#ifdef _WIN32
    WSAPOLLFD pfd{};
    pfd.fd = sock;
    pfd.events = POLLIN;
    int ret = WSAPoll(&pfd, 1, 500);
#else
    struct pollfd pfd{};
    pfd.fd = sock;
    pfd.events = POLLIN;
    int ret = poll(&pfd, 1, 500);
#endif
    if (ret < 0)
      break;

    if (ret > 0) {
      char tmp[2048];
      ssize_t n = recv(sock, tmp, sizeof(tmp), 0);
      if (n > 0) {
        // For UDP, we might have binary protocols (WSJT-X) or XML/ADIF
        // For now, let's assume simple string lines or binary
        // WSJT-X starts with 0xADBCCBDA
        if (n >= 4 && (uint32_t)ntohl(*(uint32_t *)tmp) == 0xADBCCBDA) {
          // TODO: WSJT-X parsing
        } else {
          std::string line(tmp, n);
          processLine(line);
        }
      }
    }
  }

  close(sock);
}

void DXClusterProvider::processLine(const std::string &line) {
  if (line.empty())
    return;

  // std::fprintf(stderr, "DXCluster: data: %s\n", line.c_str());

  // Example: DX de KD0AA:     18100.0  JR1FYS       FT8 LOUD in FL! 2156Z
  if (line.find("DX de ") != std::string::npos) {
    DXClusterSpot spot;
    char rxCall[32], txCall[32];
    float freq;
    // Attempt to skip leading prefix if any
    const char *start = line.c_str();
    const char *dxde = std::strstr(start, "DX de ");
    if (dxde) {
      if (sscanf(dxde, "DX de %31[^ :]: %f %31s", rxCall, &freq, txCall) == 3) {
        spot.rxCall = rxCall;
        spot.txCall = txCall;
        spot.freqKhz = freq;
        spot.spottedAt = std::chrono::system_clock::now(); // Default to now if
                                                           // time parsing fails

        // Extract time if possible (fixed position in standard cluster output)
        if (line.length() >= 74 && line[74] == 'Z') {
          int hr, mn;
          if (sscanf(line.c_str() + 70, "%2d%2d", &hr, &mn) == 2) {
            auto now = std::chrono::system_clock::now();
            std::time_t now_c = std::chrono::system_clock::to_time_t(now);
            struct tm tm_buf{};
            struct tm *tm = Astronomy::portable_gmtime(&now_c, &tm_buf);
            tm->tm_hour = hr;
            tm->tm_min = mn;
            tm->tm_sec = 0;
            // Handle day wrap if needed (if hr:mn is in the future compared to
            // now, it's likely yesterday)
            std::time_t spot_c = Astronomy::portable_timegm(tm);
            if (spot_c > now_c)
              spot_c -= 86400;
            spot.spottedAt = std::chrono::system_clock::from_time_t(spot_c);
          }
        }

        // Map location
        LatLong ll;
        if (pm_.findLocation(spot.txCall, ll)) {
          spot.txLat = ll.lat;
          spot.txLon = ll.lon;
        }
        if (pm_.findLocation(spot.rxCall, ll)) {
          spot.rxLat = ll.lat;
          spot.rxLon = ll.lon;
        }

        store_->addSpot(spot);

        // Watchlist Check
        if (watchlist_ && hits_ && watchlist_->contains(spot.txCall)) {
          WatchlistHit hit;
          hit.call = spot.txCall;
          hit.freqKhz = spot.freqKhz;
          hit.mode = "DX"; // Cluster usually doesn't specify mode clearly
                           // without parsing comment
          hit.source = "Cluster";
          hit.time = spot.spottedAt;
          hits_->addHit(hit);
        }
      }
    }
  }
}

nlohmann::json DXClusterProvider::getDebugData() const {
  nlohmann::json j;
  j["running"] = running_.load();
  j["config_host"] = config_.dxClusterHost;
  return j;
}
