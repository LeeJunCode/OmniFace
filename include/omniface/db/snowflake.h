#pragma once

#include <cstdint>
#include <mutex>

namespace omniface {
namespace db {

class Snowflake {
public:
    static Snowflake& Instance();

    int64_t NextId();

    Snowflake(const Snowflake&) = delete;
    Snowflake& operator=(const Snowflake&) = delete;

private:
    Snowflake();

    int64_t CurrentMillis() const;

    std::mutex mutex_;
    int64_t last_timestamp_ = -1;
    int64_t sequence_ = 0;

    static constexpr int64_t kMachineId = 1;
    static constexpr int kMachineBits = 10;
    static constexpr int kSequenceBits = 12;
    static constexpr int64_t kMaxSequence = (1LL << kSequenceBits) - 1;
    static constexpr int kTimestampShift = kMachineBits + kSequenceBits;
};

}  // namespace db
}  // namespace omniface
