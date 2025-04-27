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

template<typename T>
class SignalQueue {
public:
    explicit SignalQueue(size_t capacity = 0) : capacity_(capacity) {}

    void send(T value, std::stop_token stop = {});
    std::optional<T> receive(std::stop_token stop = {});
    std::optional<T> try_receive();
    void close();

private:
    std::queue<T> queue_;
    size_t capacity_;
    std::mutex mutex_;
    std::condition_variable_any cv_;
};

#endif // SIGNAL_QUEUE_H