#include "RBNProvider.h"
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

RBNProvider::RBNProvider(std::shared_ptr<DXClusterDataStore> store,
                         PrefixManager &pm, HamClockState *state)
    : store_(store), pm_(pm), state_(state) {}

RBNProvider::~RBNProvider() { stop(); }

void RBNProvider::start(const AppConfig &config) {
  if (running_)
    stop();

  config_ = config;
  if (!config_.rbnEnabled)
    return;

  running_ = true;
  stopRequested_ = false;
  thread_ = std::thread(&RBNProvider::run, this);
}

void RBNProvider::stop() {
  stopRequested_ = true;
  if (thread_.joinable())
    thread_.join();
  running_ = false;
}

void RBNProvider::run() {
  std::string host =
      config_.rbnHost.empty() ? DEFAULT_HOST : config_.rbnHost;
  std::string login = config_.callsign; // RBN login = operator callsign

  while (!stopRequested_) {
    runTelnet(host, DEFAULT_PORT, login);

    if (stopRequested_)
      break;

    LOG_W("RBN", "Disconnected, retrying in 30s...");
    std::this_thread::sleep_for(std::chrono::seconds(30));
  }
  running_ = false;
}

void RBNProvider::runTelnet(const std::string &host, int port,
                             const std::string &login) {
  LOG_I("RBN", "Connecting to {}:{}", host, port);
  if (state_) {
    auto &s = state_->services["RBN"];
    s.ok = false;
    s.lastError = "Connecting...";
  }

  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    LOG_E("RBN", "Failed to create socket");
    if (state_)
      state_->services["RBN"].lastError = "Socket error";
    return;
  }

  struct hostent *he = gethostbyname(host.c_str());
  if (!he) {
    LOG_E("RBN", "Could not resolve {}", host);
    if (state_)
      state_->services["RBN"].lastError = "DNS failed";
    close(sock);
    return;
  }

  struct sockaddr_in server_addr{};
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  std::memcpy(&server_addr.sin_addr, he->h_addr_list[0], he->h_length);

  if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
#ifdef _WIN32
    LOG_E("RBN", "Connect to {} failed: error {}", host, WSAGetLastError());
#else
    LOG_E("RBN", "Connect to {} failed: {}", host, std::strerror(errno));
#endif
    if (state_)
      state_->services["RBN"].lastError = "Connect failed";
    close(sock);
    return;
  }

#ifdef _WIN32
  unsigned long mode = 1;
  ioctlsocket(sock, FIONBIO, &mode);
#else
  fcntl(sock, F_SETFL, O_NONBLOCK);
#endif

  LOG_I("RBN", "Connected to {}", host);
  if (state_) {
    state_->services["RBN"].lastError = "Connected";
  }

  // Send callsign login immediately
  if (!login.empty()) {
    std::string cmd = login + "\r\n";
    send(sock, cmd.c_str(), cmd.length(), 0);
  }

  std::string buffer;
  bool loggedIn = login.empty();
  auto lastHeartbeat = std::chrono::system_clock::now();

  while (!stopRequested_) {
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
      char tmp[1024];
      ssize_t n = recv(sock, tmp, sizeof(tmp) - 1, 0);
      if (n <= 0) {
        LOG_W("RBN", "Connection lost");
        if (state_) {
          state_->services["RBN"].ok = false;
          state_->services["RBN"].lastError = "Connection lost";
        }
        break;
      }

      tmp[n] = '\0';
      buffer.append(tmp, n);

      size_t pos;
      while ((pos = buffer.find('\n')) != std::string::npos) {
        std::string line = buffer.substr(0, pos);
        buffer.erase(0, pos + 1);
        while (!line.empty() &&
               (line.back() == '\r' || line.back() == '\n'))
          line.pop_back();

        if (!line.empty()) {
          processLine(line);

          if (!loggedIn) {
            if (line.find("Welcome") != std::string::npos ||
                line.find("DX de ") != std::string::npos) {
              loggedIn = true;
              if (state_) {
                auto &s = state_->services["RBN"];
                s.ok = true;
                s.lastSuccess = std::chrono::system_clock::now();
                s.lastError = "";
              }
              LOG_I("RBN", "Logged in as {}", login);
            }
          }
        }
      }

      // Re-send login on prompt if not yet accepted
      if (!loggedIn && !buffer.empty()) {
        if (buffer.find("login:") != std::string::npos ||
            buffer.find("callsign:") != std::string::npos) {
          std::string cmd = login + "\r\n";
          send(sock, cmd.c_str(), cmd.length(), 0);
          buffer.clear();
        }
      }

      if (buffer.length() > 4096)
        buffer.clear();
    }

    // Heartbeat every 60 seconds
    auto now = std::chrono::system_clock::now();
    if (now - lastHeartbeat > std::chrono::seconds(60)) {
      send(sock, "\r\n", 2, 0);
      lastHeartbeat = now;
    }
  }

  close(sock);
}

void RBNProvider::processLine(const std::string &line) {
  if (line.empty())
    return;

  // Standard DX de format:
  // DX de KA9Q-#:   14020.0  W1AW          CW    20 dB  12 WPM  CQ  0000Z
  const char *dxde = std::strstr(line.c_str(), "DX de ");
  if (!dxde)
    return;

  DXClusterSpot spot;
  char rxCall[32], txCall[32];
  float freq;

  if (sscanf(dxde, "DX de %31[^ :]: %f %31s", rxCall, &freq, txCall) != 3)
    return;

  spot.rxCall = rxCall;
  spot.txCall = txCall;
  spot.freqKhz = freq;
  spot.spottedAt = std::chrono::system_clock::now();

  // Parse time (HHMM before trailing Z, typically at position 70-74)
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
      std::time_t spot_c = Astronomy::portable_timegm(tm);
      if (spot_c > now_c)
        spot_c -= 86400;
      spot.spottedAt = std::chrono::system_clock::from_time_t(spot_c);
    }
  }

  // Parse mode from comment field (RBN always includes it)
  // Format after callsign: "  CW    20 dB  12 WPM  CQ"
  // The mode is at a fixed offset after the spotter/freq/call fields.
  // RBN line: "DX de SPOTTER: FREQ  CALL          MODE  SNR  SPEED  TYPE  TIME"
  // Rough parse: find the 4th token after "DX de ..." in the line
  {
    // Seek past "DX de SPOTTER: FREQ  CALL" to extract mode+SNR
    const char *p = dxde;
    int spaces = 0;
    // Skip past rxCall, freq, txCall (3 whitespace-delimited tokens)
    // then read the mode token
    int tokens = 0;
    bool inToken = false;
    while (*p && tokens < 3) {
      if (*p == ' ' || *p == ':') {
        if (inToken)
          tokens++;
        inToken = false;
      } else {
        inToken = true;
      }
      p++;
    }
    (void)spaces; // suppress warning

    // Now skip whitespace to find mode token
    while (*p == ' ')
      p++;
    // Read mode token
    char mode[16] = {};
    int mi = 0;
    while (*p && *p != ' ' && mi < 15)
      mode[mi++] = *p++;
    if (mi > 0)
      spot.mode = mode;

    // Try to parse SNR: skip whitespace, read number, expect " dB"
    while (*p == ' ')
      p++;
    float snr = 0;
    if (sscanf(p, "%f dB", &snr) == 1)
      spot.snr = snr;
  }

  // Resolve coordinates from prefix database
  LatLong ll;
  if (pm_.findLocation(spot.txCall, ll)) {
    spot.txLat = ll.lat;
    spot.txLon = ll.lon;
  }
  if (pm_.findLocation(spot.rxCall, ll)) {
    spot.rxLat = ll.lat;
    spot.rxLon = ll.lon;
  }

  LOG_D("RBN", "Spot: {} on {:.1f} kHz {} {:.0f}dB", spot.txCall,
        spot.freqKhz, spot.mode, spot.snr);

  store_->addSpot(spot);
}
