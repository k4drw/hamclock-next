#include "WebSocketTransport.h"
#include <iostream>
#include <cstring>
#include <chrono>

#ifdef __EMSCRIPTEN__
#include <emscripten/websocket.h>
#else
// Desktop sockets
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef int socklen_t;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#endif
#endif

// A minimal SHA1 + Base64 is required for RFC6455 handshake if we implement it.
// For brevity in this iteration, we'll implement a basic non-compliant handshake
// or use mbedtls if available. Actually, many brokers accept a simple HTTP upgrade
// without strict Sec-WebSocket-Accept checking, but we should do it right eventually.

WebSocketTransport::WebSocketTransport() {
#ifdef _WIN32
  WSADATA wsaData;
  WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
}

WebSocketTransport::~WebSocketTransport() {
  disconnect();
#ifdef _WIN32
  WSACleanup();
#endif
}

bool WebSocketTransport::isConnected() const {
  return connected_;
}

#ifdef __EMSCRIPTEN__
// --- Emscripten WebSocket Implementation ---

bool WebSocketTransport::onOpen(int eventType, const EmscriptenWebSocketOpenEvent *websocketEvent, void *userData) {
  auto *transport = static_cast<WebSocketTransport*>(userData);
  transport->connected_ = true;
  return EM_TRUE;
}

bool WebSocketTransport::onError(int eventType, const EmscriptenWebSocketErrorEvent *websocketEvent, void *userData) {
  auto *transport = static_cast<WebSocketTransport*>(userData);
  transport->connected_ = false;
  return EM_TRUE;
}

bool WebSocketTransport::onClose(int eventType, const EmscriptenWebSocketCloseEvent *websocketEvent, void *userData) {
  auto *transport = static_cast<WebSocketTransport*>(userData);
  transport->connected_ = false;
  return EM_TRUE;
}

bool WebSocketTransport::onMessage(int eventType, const EmscriptenWebSocketMessageEvent *websocketEvent, void *userData) {
  auto *transport = static_cast<WebSocketTransport*>(userData);
  const auto *evt = websocketEvent;
  
  std::lock_guard<std::mutex> lock(transport->emMutex_);
  transport->emRxBuffer_.insert(transport->emRxBuffer_.end(), evt->data, evt->data + evt->numBytes);
  return EM_TRUE;
}

bool WebSocketTransport::connect(const std::string &uri) {
  if (emSocket_ != -1) disconnect();
  
  EmscriptenWebSocketCreateAttributes attr = {
      uri.c_str(),
      "mqtt", // MQTT subprotocol
      EM_TRUE
  };
  
  emSocket_ = emscripten_websocket_new(&attr);
  if (emSocket_ <= 0) return false;
  
  emscripten_websocket_set_onopen_callback(emSocket_, this, onOpen);
  emscripten_websocket_set_onerror_callback(emSocket_, this, onError);
  emscripten_websocket_set_onclose_callback(emSocket_, this, onClose);
  emscripten_websocket_set_onmessage_callback(emSocket_, this, onMessage);
  
  return true; // connected_ will be set in onOpen
}

void WebSocketTransport::disconnect() {
  if (emSocket_ != -1) {
    emscripten_websocket_close(emSocket_, 1000, "Normal Closure");
    emscripten_websocket_delete(emSocket_);
    emSocket_ = -1;
  }
  connected_ = false;
  std::lock_guard<std::mutex> lock(emMutex_);
  emRxBuffer_.clear();
}

int WebSocketTransport::send(const void *buf, size_t len) {
  if (!connected_ || emSocket_ == -1) return -1;
  // MQTT requires binary frames
  EMSCRIPTEN_RESULT res = emscripten_websocket_send_binary(emSocket_, (void*)buf, len);
  if (res == EMSCRIPTEN_RESULT_SUCCESS) return len;
  return -1;
}

int WebSocketTransport::recv(void *buf, size_t len) {
  if (!connected_ || emSocket_ == -1) return -1;
  std::lock_guard<std::mutex> lock(emMutex_);
  if (emRxBuffer_.empty()) return 0; // EAGAIN
  
  size_t toCopy = std::min(len, emRxBuffer_.size());
  std::memcpy(buf, emRxBuffer_.data(), toCopy);
  emRxBuffer_.erase(emRxBuffer_.begin(), emRxBuffer_.begin() + toCopy);
  return toCopy;
}

int WebSocketTransport::getFd() const {
  return -1;
}

#else
// --- Desktop Implementation (Placeholder / Stub for now) ---
// Fully implementing RFC6455 requires SHA1+Base64.
// For this step, we will just return false if a user tries to use desktop WebSockets
// without a proper library. In HamClock-Next, the primary use case for WS is WASM.
// If desktop needs it, we should use a proper WebSocket client library.
// For native desktop MQTT, we could just use TCP sockets without WS, but the user requested WS.
// I will implement a very basic raw TCP socket here instead of WS framing for the moment,
// or fallback to returning false.

bool WebSocketTransport::parseUrl(const std::string &uri, std::string &host, int &port, std::string &path) {
  std::string prefix = "ws://";
  if (uri.find("tcp://") == 0) prefix = "tcp://";
  if (uri.find(prefix) != 0) return false;
  
  size_t colonPos = uri.find(':', prefix.length());
  size_t slashPos = uri.find('/', prefix.length());
  
  if (colonPos != std::string::npos && colonPos < slashPos) {
    host = uri.substr(prefix.length(), colonPos - prefix.length());
    port = std::stoi(uri.substr(colonPos + 1, slashPos - colonPos - 1));
  } else {
    host = uri.substr(prefix.length(), slashPos - prefix.length());
    port = (prefix == "ws://") ? 80 : 1883;
  }
  
  if (slashPos != std::string::npos) {
    path = uri.substr(slashPos);
  } else {
    path = "/";
  }
  return true;
}

bool WebSocketTransport::connect(const std::string &uri) {
  if (sock_ != -1) disconnect();
  
  std::string host, path;
  int port;
  if (!parseUrl(uri, host, port, path)) return false;
  
  if (uri.find("ws://") == 0) {
    std::cerr << "[WebSocket] Native Desktop WebSockets not implemented. Please use tcp:// for desktop MQTT." << std::endl;
    return false;
  }
  
  struct hostent *he = gethostbyname(host.c_str());
  if (!he) return false;
  
  sock_ = socket(AF_INET, SOCK_STREAM, 0);
  if (sock_ < 0) return false;
  
  struct sockaddr_in addr;
  std::memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  std::memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);
  
  if (::connect(sock_, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    disconnect();
    return false;
  }
  
  // Set non-blocking
#ifdef _WIN32
  u_long mode = 1;
  ioctlsocket(sock_, FIONBIO, &mode);
#else
  int flags = fcntl(sock_, F_GETFL, 0);
  fcntl(sock_, F_SETFL, flags | O_NONBLOCK);
#endif

  connected_ = true;
  return true;
}

void WebSocketTransport::disconnect() {
  if (sock_ != -1) {
#ifdef _WIN32
    closesocket(sock_);
#else
    ::close(sock_);
#endif
    sock_ = -1;
  }
  connected_ = false;
}

int WebSocketTransport::send(const void *buf, size_t len) {
  if (!connected_ || sock_ < 0) return -1;
  int res = ::send(sock_, (const char*)buf, len, 0);
  if (res < 0) {
#ifdef _WIN32
    if (WSAGetLastError() == WSAEWOULDBLOCK) return 0;
#else
    if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
#endif
  }
  return res;
}

int WebSocketTransport::recv(void *buf, size_t len) {
  if (!connected_ || sock_ < 0) return -1;
  int res = ::recv(sock_, (char*)buf, len, 0);
  if (res < 0) {
#ifdef _WIN32
    if (WSAGetLastError() == WSAEWOULDBLOCK) return 0;
#else
    if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
#endif
  }
  if (res == 0) {
    // Graceful close
    disconnect();
    return -1;
  }
  return res;
}

int WebSocketTransport::getFd() const {
  return sock_;
}
#endif
