#ifndef GEB_LIB_H
#define GEB_LIB_H

#include <atomic>
#include <mutex>
#include <optional>

namespace GebLib {
    // indices are MSB to LSB of 0x0123 (left to right)
    u4 get_nibble(uint16_t word, size_t nibble_i) {
        if (nibble_i >= 4)
            throw std::runtime_error("nibble_i must be in [0, 3]");
        auto num_shifts = 4 * (3 - nibble_i); // 4 bits per nibble
        return (word & 0xf << num_shifts) >> num_shifts;
    }

    namespace Threading {
        /// @brief a channel allowing a consumer to request the next item from the producer, and the producer to decide when to
        /// accept a request.
        template <typename T>
        class ChannelCoordinator {
        private:
            std::atomic<bool> is_request_pending = false;
            std::optional<T> message = std::nullopt;

            std::mutex channel_lock;
            std::condition_variable wait_for_response;

        public:
            ChannelCoordinator() {}

            T request() {
                std::unique_lock lock(this->channel_lock);
                this->is_request_pending = true;
                this->wait_for_response.wait(lock, [this]{ return this->message.has_value(); });

                // hold this lock until the end of this function in case another request is made in between & we skip a response!
                T response = this->message.value();
                this->message = std::nullopt;
                return response;
            }

            /// @brief will only send & copy data if requested, otherwise will not
            void send_if_requested(T&& data) {
                std::lock_guard lock(this->channel_lock);
                if (this->is_request_pending) {
                    this->message = std::optional(std::forward<T>(data));
                    this->is_request_pending = false;
                    this->wait_for_response.notify_one();
                }
            }
        };
    }
}

#endif