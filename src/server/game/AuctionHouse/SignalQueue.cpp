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

 #include "SignalQueue.h"
 #include "AuctionHouseCommon.h"
 #include "AuctionHouseMgr.h" // Ensure all auction-related types are available

template<typename T>
void SignalQueue<T>::send(T value, std::stop_token stop) {
    std::unique_lock lock(mutex_);
    cv_.wait(lock, stop, [this] {
        return queue_.size() < capacity_ || capacity_ == 0;
    });
    if (stop.stop_requested()) return;
    queue_.push(std::move(value));
    lock.unlock();
    cv_.notify_one();
}

template<typename T>
std::optional<T> SignalQueue<T>::receive(std::stop_token stop) {
    std::unique_lock lock(mutex_);
    cv_.wait(lock, stop, [this] { return !queue_.empty(); });
    if (stop.stop_requested() || queue_.empty()) return std::nullopt;
    T value = std::move(queue_.front());
    queue_.pop();
    lock.unlock();
    cv_.notify_one();
    return value;
}

template<typename T>
std::optional<T> SignalQueue<T>::try_receive() {
    std::unique_lock lock(mutex_);
    if (queue_.empty()) return std::nullopt;
    T value = std::move(queue_.front());
    queue_.pop();
    lock.unlock();
    cv_.notify_one();
    return value;
}

template<typename T>
void SignalQueue<T>::close() {
    std::unique_lock lock(mutex_);
    while (!queue_.empty()) queue_.pop();
    cv_.notify_all();
}

// Explicit template instantiations
template class SignalQueue<std::unique_ptr<AuctionSearcherRequest>>;
template class SignalQueue<std::unique_ptr<AuctionSearcherResponse>>;
template class SignalQueue<std::shared_ptr<AuctionSearcherUpdate>>;