/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SIGNAL_QUEUE_H
#define SIGNAL_QUEUE_H

#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <stop_token>
#include <vector>
#include <variant>

template<typename T>
class SignalQueue {
public:
    explicit SignalQueue(size_t capacity = 0) : capacity_(capacity) {}

    void send(T value, std::stop_token stop = {});
    std::optional<T> receive(std::stop_token stop = {});
    std::optional<T> try_receive();
    void close();

    // Variant to hold an item from any queue
    using AnyItem = std::variant<std::monostate, T>;

    // Wait on multiple queues and return the first available item
    template<typename... Ts>
    static AnyItem receive_any(std::stop_token stop, SignalQueue<Ts>*... queues);

private:
    std::queue<T> queue_;
    size_t capacity_;
    std::mutex mutex_;
    std::condition_variable_any cv_;
};

template<typename T>
template<typename... Ts>
typename SignalQueue<T>::AnyItem SignalQueue<T>::receive_any(std::stop_token stop, SignalQueue<Ts>*... queues) {
    // Create a shared condition variable for coordination
    std::condition_variable_any cv;
    std::mutex mtx;
    bool item_received = false;
    AnyItem result;

    // Lambda to check if any queue has an item
    auto check_queues = [&]() {
        return (queues->queue_.empty() && ...);
    };

    // Lock all queues
    std::unique_lock lock(mtx);
    cv.wait(lock, stop, [&] {
        return stop.stop_requested() || !check_queues();
    });

    if (stop.stop_requested()) {
        return std::monostate{};
    }

    // Check each queue for an item
    ((queues->mutex_.lock(), true), ...);
    ([&] {
        if (!queues->queue_.empty() && !item_received) {
            result = std::move(queues->queue_.front());
            queues->queue_.pop();
            item_received = true;
            cv.notify_one();
        }
        queues->mutex_.unlock();
    }(), ...));

    return result;
}

#endif // SIGNAL_QUEUE_H