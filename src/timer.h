#pragma once
#include <chrono>
#include <string>
#include <iostream>

class ScopedTimer {
public:
    explicit ScopedTimer(std::string label)
        : label_(std::move(label)),
          t0_(std::chrono::steady_clock::now()) {}

    ~ScopedTimer() {
        using namespace std::chrono;
        auto t1 = steady_clock::now();
        auto ms = duration_cast<milliseconds>(t1 - t0_).count();
        std::cerr << "[TIMER] " << label_ << ": " << ms << " ms\n";
    }

private:
    std::string label_;
    std::chrono::steady_clock::time_point t0_;
};
