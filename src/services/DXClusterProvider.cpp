// DXClusterProvider — Intentional architectural exception to the Store-Push pattern.
// This provider maintains its own background thread because it owns a persistent
// streaming TCP connection (Telnet to a DX cluster). The read loop uses select()
// with a 500ms timeout to stay responsive to stop requests and send heartbeats.
// All other HTTP-based providers use Store-Push.
#include "DXClusterProvider.h"
#include "../core/Astronomy.h"
#include "../core/HamClockState.h"
#include "../core/Logger.h"
#include "../core/PrefixManager.h"
#include "../core/SoundManager.h"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include "../core/Constants.h"
#include "../core/LiveSpotData.h"
#include "../core/ADIFData.h"
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
                                     HamClockState *state,
                                     std::shared_ptr<ADIFStore> adif)
    : store_(store), pm_(pm), watchlist_(watchlist), hits_(hits),
      state_(state), adif_(adif) {
  firstAttemptTime_ = std::chrono::system_clock::now();
}

DXClusterProvider::~DXClusterProvider() { stop(); }

void DXClusterProvider::start(const AppConfig &config) {
  if (running_)
    stop();

  config_ = config;
  store_->setMaxAgeMinutes(config_.dxClusterMaxAgeMinutes);
  if (!config_.dxClusterEnabled)
    return;

  running_ = true;
  stopClicked_ = false;
  thread_ = std::thread(&DXClusterProvider::run, this);
}

void DXClusterProvider::stop() {
  stopClicked_ = true;
  sleepCv_.notify_one();
  if (thread_.joinable()) {
    thread_.join();
  }
  running_ = false;
}

void DXClusterProvider::run() {
  if (config_.hubMode == HubMode::Client && !config_.hubIp.empty()) {
    runHubClient();
    return;
  }
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
          std::lock_guard<std::mutex> lk(state_->servicesMutex);
          state_->services["DXCluster"].ok = false;
          state_->services["DXCluster"].lastError = "Rate limit reached (10/hr)";
        }
        store_->setConnected(false, "Rate limit reached (10/hr)");
      }
    }

    if (stopClicked_)
      break;

    // Prune stale in-memory spots even when no new spots arrive (connection
    // dropped, rate-limited, etc.) so the display doesn't show ancient data.
    store_->pruneInMemory();

    // Retry delay (increased to 60s to avoid hammering and IP bans)
    // Interruptible: stop() will wake this immediately via sleepCv_.notify_one()
    {
      std::unique_lock<std::mutex> lk(sleepMutex_);
      sleepCv_.wait_for(lk, std::chrono::seconds(60),
                        [this] { return stopClicked_.load(); });
    }
  }
}

void DXClusterProvider::runHubClient() {
  while (!stopClicked_) {
    httplib::Client cli(config_.hubIp, config_.hubPort);
    cli.set_connection_timeout(5);
    auto resp = cli.Get("/api/hub/dxcluster");
    if (resp && resp->status == 200) {
      try {
        auto arr = nlohmann::json::parse(resp->body);
        std::vector<DXClusterSpot> spots;
        for (const auto &j : arr) {
          DXClusterSpot s;
          s.txCall  = j.value("txCall", "");
          s.txGrid  = j.value("txGrid", "");
          s.rxCall  = j.value("rxCall", "");
          s.rxGrid  = j.value("rxGrid", "");
          s.mode    = j.value("mode", "");
          s.freqKhz = j.value("freqKhz", 0.0);
          s.snr     = j.value("snr", 0.0);
          s.txLat   = j.value("txLat", 0.0);
          s.txLon   = j.value("txLon", 0.0);
          s.rxLat   = j.value("rxLat", 0.0);
          s.rxLon   = j.value("rxLon", 0.0);
          s.txDxcc  = j.value("txDxcc", 0);
          s.rxDxcc  = j.value("rxDxcc", 0);
          int64_t ts = j.value("spottedAt", (int64_t)0);
          s.spottedAt = std::chrono::system_clock::time_point(
                            std::chrono::seconds(ts));
          spots.push_back(s);
        }
        if (store_) store_->setSpots(spots);
        if (store_) store_->setConnected(true, "Hub client");
      } catch (...) {
        if (store_) store_->setConnected(false, "Hub parse error");
      }
    } else {
      if (store_) store_->setConnected(false, "Hub unreachable");
    }
    {
      std::unique_lock<std::mutex> lk(sleepMutex_);
      sleepCv_.wait_for(lk, std::chrono::seconds(30),
                        [this] { return stopClicked_.load(); });
    }
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
    std::lock_guard<std::mutex> lk(state_->servicesMutex);
    auto &s = state_->services["DXCluster"];
    s.ok = false;
    s.lastError = "Connecting...";
  }

  incrementConnectionAttempts();

  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    LOG_E("DXCluster", "Failed to create socket");
    if (state_) {
      std::lock_guard<std::mutex> lk(state_->servicesMutex);
      state_->services["DXCluster"].lastError = "Socket error";
    }
    return;
  }

  int keepalive = 1;
  setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (const char *)&keepalive,
             sizeof(keepalive));

  // Resolve hostname
  struct addrinfo hints{};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  struct addrinfo *res = nullptr;
  std::string portStr = std::to_string(port);
  if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res) != 0 || !res) {
    LOG_E("DXCluster", "Could not resolve {}", host);
    if (state_) {
      std::lock_guard<std::mutex> lk(state_->servicesMutex);
      state_->services["DXCluster"].lastError = "DNS failed";
    }
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
      LOG_E("DXCluster", "Connect to {} failed: {}", host,
#ifdef _WIN32
            std::to_string(WSAGetLastError())
#else
            std::strerror(errno)
#endif
      );
      if (state_) {
        std::lock_guard<std::mutex> lk(state_->servicesMutex);
        state_->services["DXCluster"].lastError = "Connect failed";
      }
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
      if (state_) {
        std::lock_guard<std::mutex> lk(state_->servicesMutex);
        state_->services["DXCluster"].lastError = "Connect timeout";
      }
      close(sock);
      return;
    }

    // Check socket error status
    int error = 0;
    socklen_t len = sizeof(error);
    if (getsockopt(sock, SOL_SOCKET, SO_ERROR, (char *)&error, &len) < 0 || error != 0) {
      LOG_E("DXCluster", "Connect to {} failed: {}", host, std::strerror(error));
      if (state_) {
        std::lock_guard<std::mutex> lk(state_->servicesMutex);
        state_->services["DXCluster"].lastError = "Connect error";
      }
      close(sock);
      return;
    }
  }

  LOG_I("DXCluster", "Connected to {}", host);
  if (state_) {
    std::lock_guard<std::mutex> lk(state_->servicesMutex);
    state_->services["DXCluster"].lastError = "Connected";
  }
  store_->setConnected(true, "Connected to " + host);

  // Keep non-blocking mode (already set for connect). Use select() for the
  // recv loop timeout — SO_RCVTIMEO on Windows TCP sockets resets the
  // connection when the timeout fires (MSDN: "indeterminate state"), which
  // caused the 2-second disconnect Bob N2ESP reported.

  std::string buffer;
  bool loggedIn = login.empty();
  bool loginSent = false;   // track whether callsign has been sent
  bool initialRequestSent = false;
  auto lastHeartbeat = std::chrono::system_clock::now();
  auto lastPrune = std::chrono::system_clock::now();
  auto lastDataReceived = std::chrono::system_clock::now();

  // Elwood's DXCMSG_DT: 500ms pause before first send to let server settle.
  {
    std::unique_lock<std::mutex> lk(sleepMutex_);
    sleepCv_.wait_for(lk, std::chrono::milliseconds(500),
                      [this] { return stopClicked_.load(); });
  }

  // Send login once. loginSent prevents the reactive path below from
  // re-sending if the server also issues a "login:" prompt (double-send
  // confuses servers that DO prompt and causes immediate disconnect).
  if (!login.empty() && !stopClicked_) {
    LOG_D("DXCluster", "<<< callsign (preemptive)");
    std::string cmd = login + "\r\n";
    if (send(sock, cmd.c_str(), cmd.length(), 0) < 0) {
      LOG_W("DXCluster", "Failed to send login");
      close(sock);
      return;
    }
    loginSent = true;
  }

  while (!stopClicked_) {
    // select() with 500ms timeout — safe on Windows (SO_RCVTIMEO is not).
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET((unsigned)sock, &rfds);
    struct timeval tv{0, 500000};
    int sel = select(sock + 1, &rfds, nullptr, nullptr, &tv);
    if (sel < 0) {
#ifdef _WIN32
      LOG_W("DXCluster", "select() error {}", WSAGetLastError());
#else
      LOG_W("DXCluster", "select() error: {}", std::strerror(errno));
#endif
      break;
    }

    auto now = std::chrono::system_clock::now();

    // Periodic In-Memory Pruning (clears UI if feed goes quiet)
    if (now - lastPrune > std::chrono::seconds(60)) {
      store_->pruneInMemory();
      lastPrune = now;
    }

    // Idle Data Watchdog (detects VPN zombies / dead streams)
    if (now - lastDataReceived > std::chrono::minutes(10)) {
      LOG_W("DXCluster", "No data received for 10 minutes. Forcing reconnect.");
      break;
    }

    if (sel == 0) {
      // Timeout — no data yet. Send heartbeat if needed.
      if (now - lastHeartbeat > std::chrono::seconds(60)) {
        if (send(sock, "\r\n", 2, 0) < 0) {
          LOG_W("DXCluster", "Heartbeat failed, closing connection");
          break;
        }
        lastHeartbeat = now;
      }
      continue;
    }

    char tmp[1024];
    ssize_t n = recv(sock, tmp, sizeof(tmp) - 1, 0);
    if (n < 0) {
#ifdef _WIN32
      int e = WSAGetLastError();
      if (e == WSAEWOULDBLOCK) continue; // spurious wakeup, safe to retry
      LOG_W("DXCluster", "Connection lost (recv error {})", e);
#else
      if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
      LOG_W("DXCluster", "Connection lost (recv error: {})", std::strerror(errno));
#endif
      if (state_) {
        std::lock_guard<std::mutex> lk(state_->servicesMutex);
        state_->services["DXCluster"].ok = false;
        state_->services["DXCluster"].lastError = "Connection lost";
      }
      break;
    }
    if (n == 0) {
      LOG_W("DXCluster", "Connection closed by server");
      if (state_) {
        std::lock_guard<std::mutex> lk(state_->servicesMutex);
        state_->services["DXCluster"].ok = false;
        state_->services["DXCluster"].lastError = "Connection lost";
      }
      break;
    }

    lastDataReceived = std::chrono::system_clock::now();

    // Strip Telnet IAC sequences (0xFF + 1–2 bytes) — prevents them from
    // corrupting line parsing and avoids stalling servers that send option
    // negotiation and expect a timed-out response before proceeding.
    char clean[1024];
    int ci = 0;
    for (int i = 0; i < n; ) {
      unsigned char c = (unsigned char)tmp[i];
      if (c == 0xFF && i + 1 < n) {           // IAC
        unsigned char cmd2 = (unsigned char)tmp[i + 1];
        if (cmd2 == 0xFF) {                   // escaped 0xFF literal
          clean[ci++] = (char)0xFF;
          i += 2;
        } else if (cmd2 >= 0xFB && i + 2 < n) { // WILL/WONT/DO/DONT + option
          i += 3;
        } else {                              // 2-byte IAC command
          i += 2;
        }
      } else {
        clean[ci++] = tmp[i++];
      }
    }
    n = ci;

    clean[n] = '\0';
    LOG_D("DXCluster", "srv>>> {}", std::string(clean, n));
    buffer.append(clean, n);

    size_t pos;
    while ((pos = buffer.find('\n')) != std::string::npos) {
      std::string line = buffer.substr(0, pos);
      buffer.erase(0, pos + 1);
      while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
        line.pop_back();

      if (!line.empty()) {
        LOG_D("DXCluster", "line [loggedIn={} loginSent={} initSent={}]: {}", loggedIn, loginSent, initialRequestSent, line);
        processLine(line);

        if (line.find("Welcome") != std::string::npos ||
            line.find("connected") != std::string::npos ||
            line.find("Nodes") != std::string::npos ||
            line.find(">") != std::string::npos ||
            line.find("DX de ") != std::string::npos) {
          if (!loggedIn) {
            loggedIn = true;
            connectionAttempts_ = 0;
            if (state_) {
              std::lock_guard<std::mutex> lk(state_->servicesMutex);
              auto &s = state_->services["DXCluster"];
              s.ok = true;
              s.lastSuccess = std::chrono::system_clock::now();
            }
            store_->setConnected(true, "Logged in as " + login);
          }
          if (!initialRequestSent) {
            LOG_D("DXCluster", "<<< sh/dx 30 (triggered by: {})", line.substr(0, 40));
            const char *req = "sh/dx 30\r\n";
            if (send(sock, req, std::strlen(req), 0) < 0) {
              LOG_W("DXCluster", "Failed to send initial request");
              break;
            }
            initialRequestSent = true;
          }
        }

        if (!loggedIn && !loginSent) {
          if (line.find("login:") != std::string::npos ||
              line.find("callsign:") != std::string::npos ||
              line.find("Please enter your call:") != std::string::npos) {
            LOG_D("DXCluster", "<<< callsign (reactive, line)");
            std::string cmd = login + "\r\n";
            if (send(sock, cmd.c_str(), cmd.length(), 0) < 0) {
              LOG_W("DXCluster", "Failed to send callsign");
              break;
            }
            loginSent = true;
          }
        }
      }
    }

    // Prompt without newline (prompt arrived without trailing newline)
    if (!loggedIn && !loginSent && !buffer.empty()) {
      if (buffer.find("login:") != std::string::npos ||
          buffer.find("callsign:") != std::string::npos ||
          buffer.find("Please enter your call:") != std::string::npos) {
        LOG_D("DXCluster", "<<< callsign (reactive, buf={})", buffer.substr(0, 60));
        std::string cmd = login + "\r\n";
        if (send(sock, cmd.c_str(), cmd.length(), 0) < 0) {
          LOG_W("DXCluster", "Failed to send callsign for prompt");
          break;
        }
        loginSent = true;
        buffer.clear();
      }
    }
    if (!buffer.empty())
      LOG_D("DXCluster", "buf remainder [loggedIn={}]: {}", loggedIn, buffer.substr(0, 80));

    if (buffer.length() > 4096)
      buffer.clear();
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

    // Watched call check
    {
      auto data = store_->snapshot();
      if (!data->watchedCall.empty() && spot.txCall == data->watchedCall) {
        store_->setWatchedSpotted(std::chrono::steady_clock::now());
        // Update DX location from spot
        if (state_) {
          state_->dxLocation = {spot.txLat, spot.txLon};
          state_->dxGrid = spot.txGrid;
        }
      }
    }

    // Watchlist check
    if (watchlist_ && hits_ && watchlist_->contains(spot.txCall)) {
      WatchlistHit hit;
      hit.call = spot.txCall;
      hit.freqKhz = spot.freqKhz;
      hit.mode = "WSJT";
      hit.source = "WSJT-X";
      hit.time = spot.spottedAt;
      hits_->addHit(hit);
      SoundManager::getInstance().speak("Watchlist: " + hit.call);
    }
  }
}

void DXClusterProvider::processLine(const std::string &line) {
  if (line.empty())
    return;

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

        // Extract time: find trailing HHMMZ anywhere in the line
        size_t z_pos = line.rfind('Z');
        if (z_pos != std::string::npos && z_pos >= 4) {
          int hr, mn;
          if (sscanf(line.c_str() + z_pos - 4, "%2d%2d", &hr, &mn) == 2 &&
              hr >= 0 && hr <= 23 && mn >= 0 && mn <= 59) {
            auto now = std::chrono::system_clock::now();
            std::time_t now_c = std::chrono::system_clock::to_time_t(now);
            struct tm tm_buf{};
            struct tm *tm = Astronomy::portable_gmtime(&now_c, &tm_buf);
            tm->tm_hour = hr;
            tm->tm_min = mn;
            tm->tm_sec = 0;
            // Handle day wrap: subtract a day only if the spot time is
            // significantly in the future (e.g., > 30 min). This avoids 
            // incorrectly rollbacking due to minor local clock drifts.
            std::time_t spot_c = Astronomy::portable_timegm(tm);
            if (spot_c > now_c + 1800)
              spot_c -= 86400;

            // HHMMZ has only minute precision. If the spot is from within
            // the current minute or any upcoming minute tolerated by drift,
            // use actual receipt time to preserve immediate display.
            if (static_cast<long>(now_c - spot_c) < 60)
              spot.spottedAt = now;
            else
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

        // Watched call check
        {
          auto data = store_->snapshot();
          if (!data->watchedCall.empty() && spot.txCall == data->watchedCall) {
            store_->setWatchedSpotted(std::chrono::steady_clock::now());
            // Update DX location from spot
            if (state_) {
              state_->dxLocation = {spot.txLat, spot.txLon};
              state_->dxGrid = spot.txGrid;
            }
          }
        }

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
          SoundManager::getInstance().speak("Watchlist: " + hit.call);
        }

        // DX ADIF Alert Check (ATNO / Needed Entity)
        if (adif_ && spot.txLat != 0.0) {
          int dxcc = pm_.findDXCC(spot.txCall);
          if (dxcc > 0) {
            auto stats = adif_->get();
            if (stats.valid) {
              int bi = freqToBandIndex(spot.freqKhz);
              std::string band = (bi >= 0) ? kBands[bi].name : "";
              bool worked = false;
              if (stats.workedEntitiesPerBand.count(dxcc)) {
                if (band.empty() || stats.workedEntitiesPerBand[dxcc].count(band)) {
                  worked = true;
                }
              }

              if (!worked) {
                // Trigger Alert
                struct DXAlertData {
                  std::string call;
                  std::string entity;
                  double freq;
                  std::string mode;
                };
                auto *data = new DXAlertData{spot.txCall, pm_.getCountryName(dxcc), spot.freqKhz, spot.mode};
                SDL_Event event;
                std::memset(&event, 0, sizeof(event));
                event.type = HamClock::AE_BASE_EVENT + HamClock::AE_DX_ALERT;
                event.user.data1 = data;
                SDL_PushEvent(&event);
              }
            }
          }
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
