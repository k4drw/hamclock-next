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

RBNProvider::RBNProvider(std::shared_ptr<LiveSpotDataStore> store,
                         PrefixManager &pm, HamClockState *state)
    : store_(store), pm_(pm), state_(state) {}

RBNProvider::~RBNProvider() { stop(); }

void RBNProvider::start(const AppConfig &config) {
  if (running_)
    stop();

  config_ = config;

  // Only run if RBN is the active source
  if (config_.liveSpotSource != LiveSpotSource::RBN)
    return;

  running_ = true;
  stopRequested_ = false;
  thread_ = std::thread([this]() { this->run(); });
}

void RBNProvider::stop() {
  stopRequested_ = true;
  if (thread_.joinable())
    thread_.join();
  running_ = false;
}

void RBNProvider::run() {
  std::string host = config_.rbnHost.empty() ? DEFAULT_HOST : config_.rbnHost;
  int port = (config_.rbnPort == 0) ? DEFAULT_PORT : config_.rbnPort;
  std::string login = config_.callsign;

  while (!stopRequested_) {
    runTelnet(host, port, login);

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
    LOG_E("RBN", "Connect to {} failed", host);
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
  bool filterSent = false;
  auto lastHeartbeat = std::chrono::system_clock::now();
  auto lastPrune = std::chrono::system_clock::now();

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
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
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

      // Send filters once logged in
      if (loggedIn && !filterSent) {
        std::string filterCmd;
        if (config_.liveSpotsOfDe) {
          // I am the sender, show who heard ME (dx is callsign)
          filterCmd = "set dx filter call " + config_.callsign + "\r\n";
        } else {
          // I am the receiver, show what I heard (spotter is callsign)
          filterCmd = "set dx filter spotter " + config_.callsign + "\r\n";
        }
        LOG_I("RBN", "Sending filter: {}", filterCmd);
        send(sock, filterCmd.c_str(), (int)filterCmd.length(), 0);
        filterSent = true;
      }

      if (buffer.length() > 4096)
        buffer.clear();
    }

    auto now = std::chrono::system_clock::now();
    // Heartbeat every 60 seconds
    if (now - lastHeartbeat > std::chrono::seconds(60)) {
      send(sock, "\r\n", 2, 0);
      lastHeartbeat = now;
    }

    // Prune every 60 seconds
    if (now - lastPrune > std::chrono::seconds(60)) {
      store_->prune(now - std::chrono::minutes(config_.liveSpotsMaxAge));
      lastPrune = now;
    }
  }

  close(sock);
}

void RBNProvider::processLine(const std::string &line) {
  if (line.empty() || line[0] == '#')
    return;

  // Standard DX de format:
  // DX de KA9Q-#:   14020.0  W1AW          CW    20 dB  12 WPM  CQ  0000Z
  const char *dxde = std::strstr(line.c_str(), "DX de ");
  if (!dxde)
    return;

  char rxCall[32], txCall[32];
  float freq;

  if (sscanf(dxde, "DX de %31[^ :]: %f %31s", rxCall, &freq, txCall) != 3)
    return;

  // Band filtering
  int bandIdx = freqToBandIndex(freq);
  if (bandIdx < 0)
    return;

  // Logic filtering: RBN usually respects filters on the server, but we double
  // check here if we used 'of call' vs 'by call'
  bool isMeTx = (config_.callsign == txCall);
  bool isMeRx = (config_.callsign == rxCall);

  if (config_.liveSpotsOfDe && !isMeTx)
    return;
  if (!config_.liveSpotsOfDe && !isMeRx)
    return;

  SpotRecord rec;
  rec.freqKhz = freq;
  rec.senderCallsign = txCall;
  rec.timestamp = std::chrono::system_clock::now();

  // If we are showing who heard ME, plot the receiver.
  // If we are showing what I heard, plot the sender.
  const char *targetCall = config_.liveSpotsOfDe ? rxCall : txCall;

  LatLong ll;
  if (pm_.findLocation(targetCall, ll)) {
    rec.receiverGrid = Astronomy::latLonToGrid(ll.lat, ll.lon);
  } else {
    // If we can't find coordinates, we can't plot it on the map.
    return;
  }

  LOG_D("RBN", "Spot ({}): {} on {:.1f} kHz",
        config_.liveSpotsOfDe ? "Of DE" : "By DE", txCall, freq);

  store_->addSpot(rec);
}
