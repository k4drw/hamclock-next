// DXClusterProvider — Intentional architectural exception to the Store-Push pattern.
// This provider maintains its own background thread because it owns a persistent
// streaming TCP connection (Telnet to a DX cluster). The read loop blocks on recv()
// waiting for the next spot, which is intrinsic to the transport protocol. Polling
// or a non-blocking loop would add latency and complexity for no benefit.
// All other HTTP-based providers use Store-Push.
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

namespace {
// WSJT-X parsing helpers (big-endian)
bool wsjtx_bool(const uint8_t **bpp, const uint8_t *end) {
  if (*bpp + 1 > end)
    return false;
  bool x = **bpp > 0;
  *bpp += 1;
  return x;
}

uint32_t wsjtx_uint32(const uint8_t **bpp, const uint8_t *end) {
  if (*bpp + 4 > end)
    return 0;
  uint32_t x = ((*bpp)[0] << 24) | ((*bpp)[1] << 16) | ((*bpp)[2] << 8) | (*bpp)[3];
  *bpp += 4;
  return x;
}

uint64_t wsjtx_uint64(const uint8_t **bpp, const uint8_t *end) {
  if (*bpp + 8 > end)
    return 0;
  uint64_t x = ((uint64_t)wsjtx_uint32(bpp, end)) << 32;
  x |= wsjtx_uint32(bpp, end);
  return x;
}

std::string wsjtx_utf8(const uint8_t **bpp, const uint8_t *end) {
  uint32_t len = wsjtx_uint32(bpp, end);
  if (len == 0xffffffff)
    len = 0;
  if (*bpp + len > end)
    return "";
  std::string s((const char *)*bpp, len);
  *bpp += len;
  return s;
}
} // namespace

DXClusterProvider::DXClusterProvider(std::shared_ptr<DXClusterDataStore> store,
                                     PrefixManager &pm,
                                     std::shared_ptr<WatchlistStore> watchlist,
                                     std::shared_ptr<WatchlistHitStore> hits,
                                     HamClockState *state)
    : store_(store), pm_(pm), watchlist_(watchlist), hits_(hits),
      state_(state) {
  firstAttemptTime_ = std::chrono::system_clock::now();
}

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
      runUDP(config_.wsjtxPort);
    } else {
      // Check for excessive lost connection rate (10 per hour as per Elwood)
      if (checkConnectionRate()) {
        runTelnet(config_.dxClusterHost, config_.dxClusterPort,
                  config_.dxClusterLogin);
      } else {
        LOG_W("DXCluster", "Connection rate limit reached (10/hr)");
        if (state_) {
          state_->services["DXCluster"].ok = false;
          state_->services["DXCluster"].lastError = "Rate limit reached (10/hr)";
        }
        store_->setConnected(false, "Rate limit reached (10/hr)");
      }
    }

    if (stopClicked_)
      break;

    // Retry delay (increased to 60s to avoid hammering and IP bans)
    std::this_thread::sleep_for(std::chrono::seconds(60));
  }
}

bool DXClusterProvider::checkConnectionRate() {
  auto now = std::chrono::system_clock::now();
  if (connectionAttempts_ == 0) {
    firstAttemptTime_ = now;
    return true;
  }

  auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(now - firstAttemptTime_);
  if (elapsed >= std::chrono::minutes(60)) {
    // Reset after an hour
    connectionAttempts_ = 0;
    firstAttemptTime_ = now;
    return true;
  }

  // Original HamClock: MAX_LCN = 10 lost connections per hour
  return connectionAttempts_ < 10;
}

void DXClusterProvider::incrementConnectionAttempts() {
  connectionAttempts_++;
  LOG_W("DXCluster", "Lost connection count: {}/10 in current hour", connectionAttempts_);
}

void DXClusterProvider::runTelnet(const std::string &host, int port,
                                  const std::string &login) {
  LOG_I("DXCluster", "Connecting to {}:{}", host, port);
  if (state_) {
    auto &s = state_->services["DXCluster"];
    s.ok = false;
    s.lastError = "Connecting...";
  }

  // Note: Removed incrementConnectionAttempts() from here. 
  // Per Elwood, we only count LOST connections that were once established.

  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    LOG_E("DXCluster", "Failed to create socket");
    if (state_)
      state_->services["DXCluster"].lastError = "Socket error";
    return;
  }

  // Resolve hostname
  struct addrinfo hints{};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  struct addrinfo *res = nullptr;
  std::string portStr = std::to_string(port);
  if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res) != 0 || !res) {
    LOG_E("DXCluster", "Could not resolve {}", host);
    if (state_)
      state_->services["DXCluster"].lastError = "DNS failed";
    close(sock);
    return;
  }
  struct sockaddr_in server_addr{};
  std::memcpy(&server_addr, res->ai_addr, res->ai_addrlen);
  server_addr.sin_port = htons(port);
  freeaddrinfo(res);

  // Set non-blocking for connect timeout
#ifdef _WIN32
  unsigned long mode = 1;
  ioctlsocket(sock, FIONBIO, &mode);
#else
  fcntl(sock, F_SETFL, O_NONBLOCK);
#endif

  int ret = connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
  if (ret < 0) {
#ifdef _WIN32
    if (WSAGetLastError() != WSAEWOULDBLOCK) {
#else
    if (errno != EINPROGRESS) {
#endif
      LOG_E("DXCluster", "Connect to {} failed: {}", host, std::strerror(errno));
      if (state_)
        state_->services["DXCluster"].lastError = "Connect failed";
      close(sock);
      return;
    }

    // Wait for connect completion (10s timeout)
#ifdef _WIN32
    WSAPOLLFD pfd{};
    pfd.fd = sock;
    pfd.events = POLLOUT;
    ret = WSAPoll(&pfd, 1, 10000);
#else
    struct pollfd pfd{};
    pfd.fd = sock;
    pfd.events = POLLOUT;
    ret = poll(&pfd, 1, 10000); // 10s timeout
#endif
    if (ret <= 0) {
      LOG_E("DXCluster", "Connect to {} timed out after 10s", host);
      if (state_)
        state_->services["DXCluster"].lastError = "Connect timeout";
      close(sock);
      return;
    }

    // Check socket error status
    int error = 0;
    socklen_t len = sizeof(error);
    if (getsockopt(sock, SOL_SOCKET, SO_ERROR, (char *)&error, &len) < 0 || error != 0) {
      LOG_E("DXCluster", "Connect to {} failed: {}", host, std::strerror(error));
      if (state_)
        state_->services["DXCluster"].lastError = "Connect error";
      close(sock);
      return;
    }
  }

  LOG_I("DXCluster", "Connected to {}", host);
  if (state_)
    state_->services["DXCluster"].lastError = "Connected";
  store_->setConnected(true, "Connected to " + host);

  std::string buffer;
  bool loggedIn = login.empty();
  bool initialRequestSent = false;
  auto lastHeartbeat = std::chrono::system_clock::now();

  // Elwood's 500ms delay (DXCMSG_DT) to let the server breathe
  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  if (!login.empty() && !stopClicked_) {
    std::string cmd = login + "\r\n";
    send(sock, cmd.c_str(), cmd.length(), 0);
  }

  while (!stopClicked_) {
#ifdef _WIN32
    WSAPOLLFD pfd{};
    pfd.fd = sock;
    pfd.events = POLLIN;
    int poll_ret = WSAPoll(&pfd, 1, 500);
#else
    struct pollfd pfd{};
    pfd.fd = sock;
    pfd.events = POLLIN;
    int poll_ret = poll(&pfd, 1, 500); // 500ms timeout
#endif
    if (poll_ret < 0)
      break;

    if (poll_ret > 0) {
      char tmp[1024];
      ssize_t n = recv(sock, tmp, sizeof(tmp) - 1, 0);
      if (n <= 0) {
        LOG_W("DXCluster", "Connection lost");
        incrementConnectionAttempts();
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
              // Reset connection attempt count on successful login
              connectionAttempts_ = 0;
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
            if (line.find("login:") != std::string::npos ||
                line.find("callsign:") != std::string::npos ||
                line.find("Please enter your call:") != std::string::npos) {
              std::string cmd = login + "\r\n";
              send(sock, cmd.c_str(), cmd.length(), 0);
            }
          }
        }
      }

      // Prompt without newline logic
      if (!loggedIn && !buffer.empty()) {
        if (buffer.find("login:") != std::string::npos ||
            buffer.find("callsign:") != std::string::npos ||
            buffer.find("Please enter your call:") != std::string::npos) {
          std::string cmd = login + "\r\n";
          send(sock, cmd.c_str(), cmd.length(), 0);
          buffer.clear();
        }
      }

      if (buffer.length() > 4096)
        buffer.clear();
    }

    auto now = std::chrono::system_clock::now();
    if (now - lastHeartbeat > std::chrono::seconds(60)) {
      if (send(sock, "\r\n", 2, 0) < 0) {
        LOG_W("DXCluster", "Heartbeat failed, closing connection");
        incrementConnectionAttempts();
        break;
      }
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
      uint8_t tmp[2048];
      ssize_t n = recv(sock, (char *)tmp, sizeof(tmp), 0);
      if (n > 0) {
        // WSJT-X starts with 0xADBCCBDA
        if (n >= 12 && (uint32_t)ntohl(*(uint32_t *)tmp) == 0xADBCCBDA) {
          processWSJTX(tmp, n);
        } else {
          std::string line((char *)tmp, n);
          processLine(line);
        }
      }
    }
  }

  close(sock);
}

void DXClusterProvider::processWSJTX(const uint8_t *packet, size_t len) {
  const uint8_t *bp = packet;
  const uint8_t *end = packet + len;

  uint32_t magic = wsjtx_uint32(&bp, end);
  if (magic != 0xADBCCBDA)
    return;

  (void)wsjtx_uint32(&bp, end); // skip max schema
  uint32_t msgtype = wsjtx_uint32(&bp, end);

  if (msgtype == 1) { // Status message
    (void)wsjtx_utf8(&bp, end); // ID
    uint64_t hz = wsjtx_uint64(&bp, end);
    (void)wsjtx_utf8(&bp, end);   // mode
    std::string dx_call = wsjtx_utf8(&bp, end);
    (void)wsjtx_utf8(&bp, end);   // report
    (void)wsjtx_utf8(&bp, end);   // tx mode
    (void)wsjtx_bool(&bp, end);   // tx enabled
    (void)wsjtx_bool(&bp, end);   // transmitting
    (void)wsjtx_bool(&bp, end);   // decoding
    (void)wsjtx_uint32(&bp, end); // rx df
    (void)wsjtx_uint32(&bp, end); // tx df
    std::string de_call = wsjtx_utf8(&bp, end);
    std::string de_grid = wsjtx_utf8(&bp, end);
    std::string dx_grid = wsjtx_utf8(&bp, end);

    if (hz == 0 || dx_call.empty() || de_call.empty())
      return;

    DXClusterSpot spot;
    spot.txCall = dx_call;
    spot.rxCall = de_call;
    spot.txGrid = dx_grid;
    spot.rxGrid = de_grid;
    spot.freqKhz = hz * 1e-3f;
    spot.spottedAt = std::chrono::system_clock::now();

    LatLong ll;
    if (!dx_grid.empty()) {
      Astronomy::gridToLatLon(dx_grid, spot.txLat, spot.txLon);
    } else {
      pm_.findLocation(dx_call, ll);
      spot.txLat = ll.lat;
      spot.txLon = ll.lon;
    }

    if (!de_grid.empty()) {
      Astronomy::gridToLatLon(de_grid, spot.rxLat, spot.rxLon);
    } else {
      pm_.findLocation(de_call, ll);
      spot.rxLat = ll.lat;
      spot.rxLon = ll.lon;
    }

    store_->addSpot(spot);

    // Watchlist check
    if (watchlist_ && hits_ && watchlist_->contains(spot.txCall)) {
      WatchlistHit hit;
      hit.call = spot.txCall;
      hit.freqKhz = spot.freqKhz;
      hit.mode = "WSJT";
      hit.source = "WSJT-X";
      hit.time = spot.spottedAt;
      hits_->addHit(hit);
    }
  }
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
