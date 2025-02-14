#ifndef TG2SIP_QUEUE_H
#define TG2SIP_QUEUE_H

#include <mutex>
#include <queue>
#include <condition_variable>
#include <optional>

template<typename T>
class OptionalQueue {
public:
    OptionalQueue() = default;

    OptionalQueue(const OptionalQueue &) = delete;

    OptionalQueue &operator=(const OptionalQueue &) = delete;

    virtual ~OptionalQueue() = default;

    void emplace(std::optional<T> &&value) {
        {
            std::unique_lock<std::mutex> lock(this->mutex);
            q.emplace(std::move(value));
        }
    };

    std::optional<T> pop() {
        std::unique_lock<std::mutex> lock(this->mutex);

        if (q.empty()) {
            return std::nullopt;
        }

        std::optional<T> value = std::move(this->q.front());
        this->q.pop();

        return value;
    };

private:
    std::queue<std::optional<T>> q;
    std::mutex mutex;
};

#endif //TG2SIP_QUEUE_H
