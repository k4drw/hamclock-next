#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <mutex>

class WebSocketTransport {
public:
  WebSocketTransport();
  ~WebSocketTransport();

  // Connects to a ws:// URL
  bool connect(const std::string &uri);
  void disconnect();
  bool isConnected() const;

  // Send/recv raw payload (MQTT packet). The transport handles WS framing.
  // Returns number of bytes sent/received, 0 on no data (EAGAIN), or -1 on error.
  int send(const void *buf, size_t len);
  int recv(void *buf, size_t len);

  // For polling
  int getFd() const;

private:
  bool connected_ = false;

#ifndef __EMSCRIPTEN__
  int sock_ = -1;
  std::vector<uint8_t> rxBuffer_;
  bool parseUrl(const std::string &uri, std::string &host, int &port, std::string &path);
  bool performHandshake(const std::string &host, int port, const std::string &path);
  int readFrame(void *buf, size_t len);
#else
  int emSocket_ = -1;
  std::vector<uint8_t> emRxBuffer_;
  std::mutex emMutex_;
  
  static int onOpen(int eventType, const void *websocketEvent, void *userData);
  static int onError(int eventType, const void *websocketEvent, void *userData);
  static int onClose(int eventType, const void *websocketEvent, void *userData);
  static int onMessage(int eventType, const void *websocketEvent, void *userData);
#endif
};
