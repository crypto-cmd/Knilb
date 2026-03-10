#pragma once

#include <chrono>
namespace Knilb::Engine {

class ThinkStopwatch {
  private:
    std::chrono::steady_clock::time_point _startTime;
    std::chrono::steady_clock::time_point _endTime;
    bool forceStop= false;

  public:
    void start() {
        _startTime= std::chrono::steady_clock::now();
        forceStop= false;
    }

    void stop() {
        _endTime= std::chrono::steady_clock::now();
        forceStop= true;
    }
    int elapsed() const {
        auto now= isRunning() ? std::chrono::steady_clock::now() : _endTime;
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - _startTime).count();
    }

    bool isRunning() const {
        return !forceStop;
    }
};

} // namespace Knilb::Engine