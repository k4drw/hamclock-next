#include "LTR329Provider.h"
#include "../core/Logger.h"
#include <chrono>

#if defined(__linux__) && !defined(__EMSCRIPTEN__)
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

static constexpr uint8_t kAddr      = 0x29;
static constexpr uint8_t kRegContr  = 0x80; // ALS_CONTR
static constexpr uint8_t kRegMeas   = 0x85; // ALS_MEAS_RATE
static constexpr uint8_t kRegCH1L   = 0x88; // CH1 data low byte
static constexpr uint8_t kRegCH0L   = 0x8A; // CH0 data low byte

LTR329Provider::LTR329Provider() {}

LTR329Provider::~LTR329Provider() { stop(); }

void LTR329Provider::start() {
  if (running_)
    return;
  running_ = true;
  thread_ = std::thread(&LTR329Provider::worker, this);
}

void LTR329Provider::stop() {
  {
    std::lock_guard<std::mutex> lk(stopMutex_);
    running_ = false;
  }
  stopCv_.notify_all();
  if (thread_.joinable())
    thread_.join();
#if defined(__linux__) && !defined(__EMSCRIPTEN__)
  if (fd_ >= 0) {
    close(fd_);
    fd_ = -1;
  }
#endif
}

void LTR329Provider::worker() {
  if (!initI2C()) {
    LOG_W("LTR329", "Sensor not found on I2C bus");
    available_ = false;
    running_ = false;
    return;
  }

  LOG_I("LTR329", "Sensor initialized at address 0x{:02x}", kAddr);
  available_ = true;

  while (running_) {
    float lux = readLux();
    if (lux >= 0.0f)
      lux_.store(lux);
    std::unique_lock<std::mutex> lk(stopMutex_);
    stopCv_.wait_for(lk, std::chrono::seconds(1), [this] { return !running_.load(); });
  }
}

bool LTR329Provider::initI2C() {
#if defined(__linux__) && !defined(__EMSCRIPTEN__)
  fd_ = open("/dev/i2c-1", O_RDWR);
  if (fd_ < 0)
    return false;

  if (ioctl(fd_, I2C_SLAVE, kAddr) < 0) {
    close(fd_);
    fd_ = -1;
    return false;
  }

  // Activate: gain x1, active mode
  uint8_t contr[2] = {kRegContr, 0x01};
  if (write(fd_, contr, 2) != 2) {
    close(fd_);
    fd_ = -1;
    return false;
  }

  // Measurement rate: 100ms integration, 500ms repeat
  uint8_t meas[2] = {kRegMeas, 0x03};
  write(fd_, meas, 2);

  // Allow first measurement to complete — interruptible so stop() returns promptly
  {
    std::unique_lock<std::mutex> lk(stopMutex_);
    stopCv_.wait_for(lk, std::chrono::milliseconds(600), [this] { return !running_.load(); });
  }
  return true;
#else
  return false;
#endif
}

float LTR329Provider::readLux() {
#if defined(__linux__) && !defined(__EMSCRIPTEN__)
  if (fd_ < 0)
    return -1.0f;

  // Read CH1 (IR channel): registers 0x88 and 0x89
  uint8_t reg = kRegCH1L;
  if (write(fd_, &reg, 1) != 1) return -1.0f;
  uint8_t buf[2];
  if (read(fd_, buf, 2) != 2) return -1.0f;
  uint16_t ch1 = static_cast<uint16_t>(buf[0]) | (static_cast<uint16_t>(buf[1]) << 8);

  // Read CH0 (visible+IR channel): registers 0x8A and 0x8B
  reg = kRegCH0L;
  if (write(fd_, &reg, 1) != 1) return -1.0f;
  if (read(fd_, buf, 2) != 2) return -1.0f;
  uint16_t ch0 = static_cast<uint16_t>(buf[0]) | (static_cast<uint16_t>(buf[1]) << 8);

  // Simplified lux: use CH0 as the ambient light indicator.
  // Full ratio-based lux formula is sensor-specific; for auto-dim purposes
  // the raw CH0 value (0..65535) is a good relative brightness measure.
  // Scale to approximate lux by dividing by ~3 (empirical for indoor lighting).
  float lux = static_cast<float>(ch0) / 3.0f;
  LOG_D("LTR329", "CH0={} CH1={} lux={:.0f}", ch0, ch1, lux);
  return lux;
#else
  (void)kRegCH1L;
  (void)kRegCH0L;
  return -1.0f;
#endif
}
