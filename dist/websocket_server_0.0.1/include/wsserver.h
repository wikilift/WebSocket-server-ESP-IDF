/**
 * @file wsserver.h
 * @brief WebSocket server class for handling WebSocket connections and messages.
 * 
 * This file defines the WSServer class, which provides a WebSocket server implementation
 * for the ESP32 using the ESP-IDF framework. It includes functionality for handling text
 * and binary messages, managing client connections, and performing keep-alive checks.
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
#include <esp_http_server.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <functional>
#include "keep_alive.h"
#include <string>
#include <set>

#define MAX_MESSAGE_SIZE 1024

#if !CONFIG_HTTPD_WS_SUPPORT
#error This example cannot be used unless HTTPD_WS_SUPPORT is enabled in esp-http-server component configuration
#endif

/**
 * @brief WebSocket server class for handling WebSocket connections and messages.
 */
class WSServer
{
public:
    /**
     * @brief Get the singleton instance of the WebSocket server.
     * @return Reference to the WSServer instance.
     */
    static WSServer &getInstance();

    /**
     * @brief Start the WebSocket server.
     *
     * @param _port Port number to start the server on.
     * @param ssid SSID for the Wi-Fi network.
     * @param password Password for the Wi-Fi network.
     * @param isAP Whether to start the server in Access Point mode (true) or Station mode (false).
     * @param use_ssl Whether to use SSL for secure communication. Note that self-signed certificates are used, so they need to be added to the trusted certificates or replaced with certificates issued by a trusted authority.
     * @param max_clients Maximum number of clients that can connect to the server.
     * @param keep_alive_period_ms Period (in milliseconds) for the keep-alive checks.
     * @param not_alive_after_ms Timeout (in milliseconds) after which a client is considered not alive.
     * @param auth_user Username for HTTP Basic Authentication. If null, authentication is disabled.
     * @param auth_password Password for HTTP Basic Authentication. If null, authentication is disabled.
     * @param extra_config Optional function for additional configuration.
     */

    void start(uint16_t _port = 80, const char *ssid = "default_ssid",
               const char *password = "default_password", bool isAP = true, bool use_ssl = false,
               size_t max_clients = 4, size_t keep_alive_period_ms = 10000,
               size_t not_alive_after_ms = 30000, char *auth_user = nullptr, char *auth_password = nullptr,
               std::function<void()> extra_config = nullptr);

    /**
     * @brief Send a text message to a specific client.
     *
     * @param fd File descriptor of the client.
     * @param message Text message to send.
     * @return esp_err_t Error code indicating success or failure.
     */
    esp_err_t sendText(int fd, const char *message);

    /**
     * @brief Broadcast a text message to all connected clients.
     *
     * @param message Text message to broadcast.
     * @return esp_err_t Error code indicating success or failure.
     */
    esp_err_t sendTextBroadcast(const char *message);

    /**
     * @brief Send a binary message to a specific client.
     *
     * @param fd File descriptor of the client.
     * @param data Binary data to send.
     * @param length Length of the binary data.
     * @return esp_err_t Error code indicating success or failure.
     */
    esp_err_t sendBinary(int fd, const uint8_t *data, size_t length);

    /**
     * @brief Broadcast a binary message to all connected clients.
     *
     * @param data Binary data to broadcast.
     * @param length Length of the binary data.
     * @return esp_err_t Error code indicating success or failure.
     */
    esp_err_t sendBinaryBroadcast(const uint8_t *data, size_t length);

    /**
     * @brief Set the callback function for handling text messages.
     *
     * @param callback Function to handle text messages.
     */
    void onTextMessage(std::function<void(httpd_req_t *, httpd_ws_frame_t &)> callback);

    /**
     * @brief Set the callback function for handling binary messages.
     *
     * @param callback Function to handle binary messages.
     */
    void onBinaryMessage(std::function<void(httpd_req_t *, httpd_ws_frame_t &)> callback);

    /**
     * @brief Set the callback function for handling close messages.
     *
     * @param callback Function to handle close messages.
     */
    void onCloseMessage(std::function<void(httpd_req_t *, httpd_ws_frame_t &)> callback);

    /**
     * @brief Set the callback function for handling client connections.
     *
     * @param callback Function to handle client connections.
     */
    void onClientConnected(std::function<void(int)> callback);

    /**
     * @brief Set the callback function for handling client disconnections.
     *
     * @param callback Function to handle client disconnections.
     */
    void onClientDisconnected(std::function<void(int)> callback);

private:
    /**
     * @brief Argument structure for asynchronous responses.
     */
    struct async_resp_arg
    {
        httpd_handle_t hd; /**< HTTPD handle */
        int fd;            /**< File descriptor */
    };

    /**
     * @brief Structure for WebSocket messages.
     */
    struct ws_message_t
    {
        int fd;                            /**< File descriptor */
        bool broadcast;                    /**< Whether to broadcast the message */
        httpd_ws_type_t message_type;      /**< Type of WebSocket message */
        size_t length;                     /**< Length of the message */
        uint8_t message[MAX_MESSAGE_SIZE]; /**< Message data */
    };

    /**
     * @brief Private Constructor.
     */
    WSServer();


    WSServer(const WSServer &) = delete;
    WSServer &operator=(const WSServer &) = delete;

    /**
     * @brief Handler for client connections.
     *
     * @param arg Argument passed to the handler.
     * @param event_base Event base.
     * @param event_id Event ID.
     * @param event_data Event data.
     */
    static void connect_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

    /**
     * @brief Handler for client disconnections.
     *
     * @param arg Argument passed to the handler.
     * @param event_base Event base.
     * @param event_id Event ID.
     * @param event_data Event data.
     */
    static void disconnect_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

    /**
     * @brief Task for sending WebSocket messages.
     *
     * @param pvParameters Parameters passed to the task.
     */
    static void ws_server_send_messages_task(void *pvParameters);

    /**
     * @brief WebSocket request handler.
     *
     * @param req HTTP request.
     * @return esp_err_t Error code indicating success or failure.
     */
    static esp_err_t ws_handler(httpd_req_t *req);

    /**
     * @brief Open a WebSocket file descriptor.
     *
     * @param hd HTTPD handle.
     * @param sockfd Socket file descriptor.
     * @return esp_err_t Error code indicating success or failure.
     */
    static esp_err_t ws_open_fd(httpd_handle_t hd, int sockfd);

    /**
     * @brief Close a WebSocket file descriptor.
     *
     * @param hd HTTPD handle.
     * @param sockfd Socket file descriptor.
     */
    static void ws_close_fd(httpd_handle_t hd, int sockfd);

    /**
     * @brief Send a ping message.
     *
     * @param arg Argument passed to the function.
     */
    static void send_ping(void *arg);

    /**
     * @brief Callback function for checking if a client is not alive.
     *
     * @param h KeepAlive handle.
     * @param fd File descriptor.
     * @return true If the client is not alive.
     * @return false If the client is alive.
     */
    static bool client_not_alive_cb(KeepAlive *h, int fd);

    /**
     * @brief Callback function for checking if a client is alive.
     *
     * @param h KeepAlive handle.
     * @param fd File descriptor.
     * @return true If the client is alive.
     * @return false If the client is not alive.
     */
    static bool check_client_alive_cb(KeepAlive *h, int fd);

    /**
     * @brief Start the WebSocket server.
     *
     * @param port Port number to start the server on.
     * @return httpd_handle_t HTTPD handle.
     */
    httpd_handle_t start_ws_server(uint16_t port);

    /**
     * @brief Stop the WebSocket server.
     *
     * @param server HTTPD handle of the server.
     * @return esp_err_t Error code indicating success or failure.
     */
    esp_err_t stop_ws_server(httpd_handle_t server);

    /**
     * @brief Send WebSocket messages.
     */
    void ws_server_send_messages();

    /**
     * @brief Enqueue a WebSocket message.
     *
     * @param fd File descriptor of the client.
     * @param message Message data.
     * @param length Length of the message.
     * @param broadcast Whether to broadcast the message.
     * @param message_type Type of WebSocket message.
     * @return esp_err_t Error code indicating success or failure.
     */
    esp_err_t enqueueMessage(int fd, const uint8_t *message, size_t length, bool broadcast, httpd_ws_type_t message_type);

    /**
     * @brief Authenticate the incoming HTTP request using Basic Authentication.
     *
     * @param req Pointer to the HTTP request.
     * @return True if authentication is successful or disabled, false otherwise.
     */
    static bool authenticate(httpd_req_t *req);

    httpd_handle_t server;                     /**< HTTPD handle */
    QueueHandle_t ws_send_queue;               /**< Queue handle for WebSocket messages */
    size_t max_clients;                        /**< Maximum number of clients */
    static KeepAlive *keep_alive;              /**< KeepAlive instance */
    static const char *TAG;                    /**< Tag for logging */
    static const httpd_uri_t ws;               /**< URI handler for WebSocket */
    static uint16_t port;                      /**< Port number */
    static const constexpr bool debug = false; /**< Debug flag */
    static uint8_t buffer[MAX_MESSAGE_SIZE];   /**< Buffer for messages */

    static std::function<void(httpd_req_t *, httpd_ws_frame_t &)> text_message_callback;   /**< Callback for text messages */
    static std::function<void(httpd_req_t *, httpd_ws_frame_t &)> binary_message_callback; /**< Callback for binary messages */
    static std::function<void(httpd_req_t *, httpd_ws_frame_t &)> close_message_callback;  /**< Callback for close messages */
    static std::function<void(int)> client_connected_callback;                             /**< Callback for client connections */
    static std::function<void(int)> client_disconnected_callback;                          /**< Callback for client disconnections */

    static char *auth_user;     /**< Username for HTTP Basic Authentication */
    static char *auth_password; /**< Password for HTTP Basic Authentication */
    /**
     * @brief Stores the authenticated clients' socket file descriptors.
     * This set keeps track of the clients that have successfully authenticated,
     * so they do not need to re-authenticate for each request.
     */
    static std::set<int> authenticated_clients;
};
