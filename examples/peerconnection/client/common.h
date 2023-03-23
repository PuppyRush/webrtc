#pragma once

#include <atomic>
#include <chrono>
#include <cstdio>
#include <functional>
#include <random>
#include <string>
#include <thread>
namespace common {
static std::string CrateToken(const std::string& pattern, const int len) {
  std::string roomToken;
  std::random_device rng;
  std::uniform_int_distribution<> index_dist(0, pattern.size() - 1);
  for (int i = 0; i < len; ++i) {
    roomToken += std::to_string(index_dist(rng));
  }

  return roomToken;
}

static std::string CrateRoomToken() {
  return CrateToken("1234567890", 5);
}

static std::string CrateUserToken() {
  return CrateToken("1234567890", 6);
}

}  // namespace common

namespace timer {

class Timer {
 public:
   Timer() = default;
  ~Timer() {
    if (mRunning) {
      stop();
    }
  }
  typedef std::chrono::milliseconds Interval;
  typedef std::function<void(void)> Timeout;

  void start(const std::function<void(std::map<std::string, std::string>)>& fn,
    std::map<std::string, std::string>& param,
             const Interval& interval,
             const Timeout& timeout) {
    mRunning = true;

    mThread = std::thread([this, interval, timeout, fn, param] {
      while (mRunning) {
        std::this_thread::sleep_for(interval);
        fn(param);
        timeout();
      }
    });
  }
  void stop() {
    mRunning = false;
    mThread.join();
  }

 private:
  std::map<std::string, std::string> param;
  std::thread mThread{};
  std::atomic_bool mRunning{};
};

}  // namespace timer
