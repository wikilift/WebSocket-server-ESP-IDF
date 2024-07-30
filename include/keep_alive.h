/**
 * @file keep_alive.h
 * @brief KeepAlive class for managing client keep-alive functionality.
 * 
 * This file defines the KeepAlive class, which provides functionality for
 * managing keep-alive checks for WebSocket clients. It includes methods for
 * adding, removing, and updating clients, as well as starting and stopping
 * the keep-alive task.
 * 
 * This implementation is based on the `echoserver` example provided in the ESP-IDF framework.
 * 
 * @author Daniel Giménez
 * @date 2024-07-28
 * @license MIT License
 * 
 * @par License:
 * 
 * MIT License
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#pragma once

#include <vector>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_timer.h>

/**
 * @brief KeepAlive class for managing client keep-alive functionality.
 */
class KeepAlive
{
public:
    /**
     * @brief Structure representing a client.
     */
    struct Client 
    {
        int fd;               /**< File descriptor of the client */
        uint64_t last_seen;   /**< Timestamp of the last time the client was seen */
    };

    /**
     * @brief Configuration structure for KeepAlive.
     */
    struct Config
    {
        size_t max_clients;                       /**< Maximum number of clients */
        size_t task_stack_size;                   /**< Stack size for the keep-alive task */
        size_t task_prio;                         /**< Priority for the keep-alive task */
        size_t keep_alive_period_ms;              /**< Period for the keep-alive checks */
        size_t not_alive_after_ms;                /**< Timeout after which a client is considered not alive */
        bool (*check_client_alive_cb)(KeepAlive *h, int fd);   /**< Callback function for checking if a client is alive */
        bool (*client_not_alive_cb)(KeepAlive *h, int fd);     /**< Callback function for handling clients that are not alive */
        void *user_ctx;                           /**< User context */
    };

    /**
     * @brief Construct a new KeepAlive object.
     * @param config Configuration for the KeepAlive instance.
     */
    KeepAlive(const Config &config);

    /**
     * @brief Destroy the KeepAlive object.
     */
    ~KeepAlive();

    /**
     * @brief Start the keep-alive task.
     * @return true if the task was started successfully, false otherwise.
     */
    bool start();

    /**
     * @brief Stop the keep-alive task.
     */
    void stop();

    /**
     * @brief Add a client to the keep-alive list.
     * @param fd File descriptor of the client.
     * @return true if the client was added successfully, false otherwise.
     */
    bool addClient(int fd);

    /**
     * @brief Remove a client from the keep-alive list.
     * @param fd File descriptor of the client.
     * @return true if the client was removed successfully, false otherwise.
     */
    bool removeClient(int fd);

    /**
     * @brief Update the last seen timestamp for a client.
     * @param fd File descriptor of the client.
     * @return true if the client was updated successfully, false otherwise.
     */
    bool updateClient(int fd);

    /**
     * @brief Set the user context.
     * @param ctx Pointer to the user context.
     */
    void setUserCtx(void *ctx);

    /**
     * @brief Get the user context.
     * @return Pointer to the user context.
     */
    void *getUserCtx() const;

    /**
     * @brief Get the list of clients.
     * @return Reference to the vector of clients.
     */
    const std::vector<Client>& getClients() const;

private:
    /**
     * @brief Task function for keep-alive checks.
     * @param arg Pointer to the KeepAlive instance.
     */
    static void task(void *arg);

    /**
     * @brief Get the maximum delay for the keep-alive checks.
     * @return Maximum delay in milliseconds.
     */
    uint64_t getMaxDelay() const;

    Config config;                   /**< Configuration for the KeepAlive instance */
    QueueHandle_t queue;             /**< Queue handle for keep-alive messages */
    std::vector<Client> clients;     /**< Vector of clients */
    bool running;                    /**< Flag indicating if the keep-alive task is running */
};

