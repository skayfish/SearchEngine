#pragma once

#include <chrono>
#include <iostream>

#define PROFILE_CONCAT_INTERNAL(X, Y) X ## Y
#define PROFILE_CONCAT(X, Y) PROFILE_CONCAT_INTERNAL(X, Y)
#define UNIQUE_VAR_NAME_PROFILE PROFILE_CONCAT(profileGuard, __LINE__)
#define LOG_DURATION(x) LogDuration UNIQUE_VAR_NAME_PROFILE(x)

#define LOG_DURATION_STREAM(x, y) LogDuration guard(x, y)

class LogDuration {
public:
    using Clock = std::chrono::steady_clock;

    LogDuration() {

    }

    LogDuration(std::string name_operation)
        : name_operation_(name_operation) {

    }

    LogDuration(std::string name_operation, std::ostream& os) 
        : name_operation_(name_operation)
        , os_(os) {

    }

    ~LogDuration() {
        using namespace std::chrono;
        using namespace std::literals;

        const auto end_time = Clock::now();
        const auto dur = end_time - start_time_;
        os_ << name_operation_ << ": "s << duration_cast<milliseconds>(dur).count() << " ms"s << std::endl;
    }

private:
    const Clock::time_point start_time_ = Clock::now();
    std::string name_operation_ = std::string("name_operation");
    std::ostream& os_ = std::cerr;
};