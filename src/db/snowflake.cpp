#include "omniface/db/snowflake.h"

#include <chrono>
#include <thread>

namespace omniface {
namespace db {

namespace {

int64_t CustomEpochMs() {
    using namespace std::chrono;
    auto epoch = system_clock::time_point{sys_days{January / 1 / 2024}};
    return duration_cast<milliseconds>(epoch.time_since_epoch()).count();
}

}  // namespace

Snowflake& Snowflake::Instance() {
    static Snowflake instance;
    return instance;
}

Snowflake::Snowflake() = default;

int64_t Snowflake::CurrentMillis() const {
    using namespace std::chrono;
    auto now = duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count();
    return now - CustomEpochMs();
}

int64_t Snowflake::NextId() {
    std::lock_guard<std::mutex> lock(mutex_);

    int64_t now = CurrentMillis();
    if (now < last_timestamp_) {
        now = last_timestamp_;  // clock rollback: clamp to last known time
    }

    if (now == last_timestamp_) {
        sequence_ = (sequence_ + 1) & kMaxSequence;
        if (sequence_ == 0) {
            while (CurrentMillis() <= last_timestamp_) {
                std::this_thread::yield();
            }
            now = CurrentMillis();
        }
    } else {
        sequence_ = 0;
    }

    last_timestamp_ = now;

    return (now << kTimestampShift)
         | (kMachineId << kSequenceBits)
         | sequence_;
}

}  // namespace db
}  // namespace omniface
