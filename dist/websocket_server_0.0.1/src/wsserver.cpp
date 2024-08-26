/**
 * @file wsserver.cpp
 * @brief WebSocket server class implementation.
 *
 * This file defines the WSServer class implementation.
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

#include "wsserver.h"
#include <cstring>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_wifi.h"
#include <mbedtls/base64.h>
#include "lwip/sockets.h"

#include <esp_https_server.h>

#define TIMEOUT 1000
#define MAX_QUEUE_SIZE 10

const char *WSServer::TAG = "WSServer";
uint16_t WSServer::port = 80;

std::function<void(httpd_req_t *, httpd_ws_frame_t &)> WSServer::text_message_callback;
std::function<void(httpd_req_t *, httpd_ws_frame_t &)> WSServer::binary_message_callback;
std::function<void(httpd_req_t *, httpd_ws_frame_t &)> WSServer::close_message_callback;
std::function<void(int)> WSServer::client_connected_callback;
std::function<void(int)> WSServer::client_disconnected_callback;

char *WSServer::auth_user = nullptr;

char *WSServer::auth_password = nullptr;

std::set<int> WSServer::authenticated_clients;

KeepAlive *WSServer::keep_alive = nullptr;

const httpd_uri_t WSServer::ws = {
    .uri = "/ws",
    .method = HTTP_GET,
    .handler = WSServer::ws_handler,
    .user_ctx = nullptr,
    .is_websocket = true,
    .handle_ws_control_frames = true};

WSServer &WSServer::getInstance()
{
    static WSServer instance;
    return instance;
}

WSServer::WSServer() : server(NULL), ws_send_queue(NULL), max_clients(4) {}

esp_err_t WSServer::start(uint16_t _port, const char *ssid,
                          const char *password, bool isAP, bool use_ssl, size_t max_clients,
                          size_t keep_alive_period_ms, size_t not_alive_after_ms, char *auth_user, char *auth_password,
                          std::function<void()> extra_config)
{
    port = _port;
    this->max_clients = max_clients;
    if (auth_user)
    {
        WSServer::auth_user = strdup(auth_user);
    }
    if (auth_password)
    {
        WSServer::auth_password = strdup(auth_password);
    }

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    if (isAP)
    {
        esp_netif_create_default_wifi_ap();
    }
    else
    {
        esp_netif_create_default_wifi_sta();
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {};

    if (isAP)
    {
        strncpy((char *)wifi_config.ap.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
        wifi_config.sta.ssid[sizeof(wifi_config.ap.ssid) - 1] = '\0';

        strncpy((char *)wifi_config.ap.password, password, sizeof(wifi_config.sta.password) - 1);
        wifi_config.sta.password[sizeof(wifi_config.ap.password) - 1] = '\0';

        wifi_config.ap.max_connection = max_clients;
        wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));

        ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_STACONNECTED, &WSServer::connect_handler, this));
        ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_STADISCONNECTED, &WSServer::disconnect_handler, this));
    }
    else
    {
        strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
        wifi_config.sta.ssid[sizeof(wifi_config.sta.ssid) - 1] = '\0';

        strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
        wifi_config.sta.password[sizeof(wifi_config.sta.password) - 1] = '\0';

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

        ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &WSServer::connect_handler, this));
        ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &WSServer::disconnect_handler, this));
    }

    if (extra_config)
    {
        extra_config();
    }

    ESP_ERROR_CHECK(esp_wifi_start());

    ws_send_queue = xQueueCreate(MAX_QUEUE_SIZE, sizeof(ws_message_t *));
    if (ws_send_queue == NULL)
    {
        ESP_LOGE(TAG, "Failed to create the message queue");
        return ESP_FAIL;
    }

    KeepAlive::Config config = {
        .max_clients = max_clients,
        .task_stack_size = 4096,
        .task_prio = tskIDLE_PRIORITY,
        .keep_alive_period_ms = keep_alive_period_ms,
        .not_alive_after_ms = not_alive_after_ms,
        .check_client_alive_cb = check_client_alive_cb,
        .client_not_alive_cb = client_not_alive_cb,
        .user_ctx = this,
        .task_enabled = true};
    keep_alive = new KeepAlive(config);

    if (use_ssl)
    {
        httpd_ssl_config_t conf = HTTPD_SSL_CONFIG_DEFAULT();
        conf.httpd.max_open_sockets = max_clients;
        conf.httpd.global_user_ctx = keep_alive;
        conf.httpd.open_fn = ws_open_fd;
        conf.httpd.close_fn = ws_close_fd;

        extern const unsigned char servercert_start[] asm("_binary_servercert_pem_start");
        extern const unsigned char servercert_end[] asm("_binary_servercert_pem_end");
        conf.servercert = servercert_start;
        conf.servercert_len = servercert_end - servercert_start;

        extern const unsigned char prvtkey_pem_start[] asm("_binary_prvtkey_pem_start");
        extern const unsigned char prvtkey_pem_end[] asm("_binary_prvtkey_pem_end");
        conf.prvtkey_pem = prvtkey_pem_start;
        conf.prvtkey_len = prvtkey_pem_end - prvtkey_pem_start;

        esp_err_t ret = httpd_ssl_start(&server, &conf);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to start HTTPS server: %s", esp_err_to_name(ret));
            return ESP_FAIL;
        }
    }
    else
    {
        server = start_ws_server(this->port);
    }

    xTaskCreatePinnedToCore(&WSServer::ws_server_send_messages_task, "ws_server_send_messages_task", 10024, this, 5, nullptr, 1);
    return ESP_OK;
}

esp_err_t WSServer::enqueueMessage(int fd, const uint8_t *message, size_t length, bool broadcast, httpd_ws_type_t message_type)
{
    if (length > MAX_MESSAGE_SIZE)
    {
        ESP_LOGE(TAG, "Message size exceeds buffer limit");
        return ESP_FAIL;
    }

    ws_message_t *ws_message = static_cast<ws_message_t *>(pvPortMalloc(sizeof(ws_message_t) - MAX_MESSAGE_SIZE + length));
    if (ws_message == nullptr)
    {
        return ESP_ERR_NO_MEM;
    }

    ws_message->fd = fd;
    ws_message->broadcast = broadcast;
    ws_message->message_type = message_type;
    ws_message->length = length;
    memcpy(ws_message->message, message, length);

    if (xQueueSend(ws_send_queue, &ws_message, TIMEOUT) != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to enqueue message");
        vPortFree(ws_message);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t WSServer::sendText(int fd, const char *message)
{
    return enqueueMessage(fd, reinterpret_cast<const uint8_t *>(message), strlen(message), false, HTTPD_WS_TYPE_TEXT);
}

esp_err_t WSServer::sendTextBroadcast(const char *message)
{
    return enqueueMessage(-1, reinterpret_cast<const uint8_t *>(message), strlen(message), true, HTTPD_WS_TYPE_TEXT);
}

esp_err_t WSServer::sendBinary(int fd, const uint8_t *data, size_t length)
{
    return enqueueMessage(fd, data, length, false, HTTPD_WS_TYPE_BINARY);
}

esp_err_t WSServer::sendBinaryBroadcast(const uint8_t *data, size_t length)
{
    return enqueueMessage(-1, data, length, true, HTTPD_WS_TYPE_BINARY);
}

void WSServer::connect_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    WSServer *instance = static_cast<WSServer *>(arg);
    if (instance->server == NULL)
    {
        instance->server = instance->start_ws_server(port);
    }
}

void WSServer::disconnect_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    WSServer *instance = static_cast<WSServer *>(arg);
    if (instance->server)
    {
        if (instance->stop_ws_server(instance->server) == ESP_OK)
        {
            instance->server = NULL;
        }
        else
        {
            ESP_LOGE(TAG, "Failed to stop http server");
        }
    }
}

void WSServer::ws_server_send_messages_task(void *pvParameters)
{
    WSServer *instance = static_cast<WSServer *>(pvParameters);
    instance->ws_server_send_messages();
}

void WSServer::onTextMessage(std::function<void(httpd_req_t *, httpd_ws_frame_t &)> callback)
{
    text_message_callback = callback;
}

void WSServer::onBinaryMessage(std::function<void(httpd_req_t *, httpd_ws_frame_t &)> callback)
{
    binary_message_callback = callback;
}

void WSServer::onCloseMessage(std::function<void(httpd_req_t *, httpd_ws_frame_t &)> callback)
{
    close_message_callback = callback;
}

void WSServer::onClientConnected(std::function<void(int)> callback)
{
    client_connected_callback = callback;
}

void WSServer::onClientDisconnected(std::function<void(int)> callback)
{
    client_disconnected_callback = callback;
}

esp_err_t WSServer::ws_handler(httpd_req_t *req)
{
    uint8_t *buffer = nullptr;

    if (auth_user != nullptr && auth_password != nullptr)
    {
        int sockfd = httpd_req_to_sockfd(req);
        if (authenticated_clients.find(sockfd) == authenticated_clients.end())
        {
            if (!authenticate(req))
            {
                return ESP_FAIL;
            }
            authenticated_clients.insert(sockfd);
        }
    }

    if (req->method == HTTP_GET)
    {
        if (debug)
        {
            ESP_LOGI(TAG, "Handshake done, the new connection was opened");
        }
        return ESP_OK;
    }

    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));

    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK)
    {
        if (debug)
        {
            ESP_LOGE(TAG, "httpd_ws_recv_frame failed to get frame len with %d", ret);
        }
        return ret;
    }
    if (debug)
    {
        ESP_LOGI(TAG, "frame len is %d", ws_pkt.len);
    }

    if (ws_pkt.len)
    {
        if (ws_pkt.len > MAX_MESSAGE_SIZE)
        {
            if (debug)
            {
                ESP_LOGE(TAG, "Message too large for buffer");
            }
            return ESP_ERR_NO_MEM;
        }
        buffer = static_cast<uint8_t *>(pvPortMalloc(ws_pkt.len));
        if (buffer == nullptr)
        {
            ESP_LOGE(TAG, "Failed to allocate buffer for WebSocket payload");
            return ESP_ERR_NO_MEM;
        }
        ws_pkt.payload = buffer;
        memset(buffer, 0, ws_pkt.len);

        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK)
        {
            if (debug)
            {
                ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
            }
            vPortFree(buffer);
            return ret;
        }
    }
    if (debug)
    {
        ESP_LOGI(TAG, "Received frame length: %d", ws_pkt.len);
    }

    switch (ws_pkt.type)
    {

    case HTTPD_WS_TYPE_PONG:
    {
        if (debug)
        {
            ESP_LOGI(TAG, "Received PONG message from client fd: %d", httpd_req_to_sockfd(req));
        }
        keep_alive->updateClient(httpd_req_to_sockfd(req));
        break;
    }
    case HTTPD_WS_TYPE_TEXT:
        buffer[ws_pkt.len] = '\0';
        if (text_message_callback)
        {
            text_message_callback(req, ws_pkt);
        }
        else
        {
            ESP_LOGI(TAG, "Received text message: %s", ws_pkt.payload);
        }
        break;
    case HTTPD_WS_TYPE_BINARY:
        if (binary_message_callback)
        {
            binary_message_callback(req, ws_pkt);
        }
        else
        {
            ESP_LOGI(TAG, "Received binary message");
        }
        break;
    case HTTPD_WS_TYPE_CLOSE:
        if (close_message_callback)
        {
            close_message_callback(req, ws_pkt);
        }
        else
        {
            ESP_LOGI(TAG, "Received close message");
            ws_close_fd(req->handle, httpd_req_to_sockfd(req));
        }
        break;
    case HTTPD_WS_TYPE_PING:
        ESP_LOGI(TAG, "Received PING message, replying with PONG");
        ws_pkt.type = HTTPD_WS_TYPE_PONG;
        httpd_ws_send_frame(req, &ws_pkt);
        break;
    default:
        ESP_LOGW(TAG, "Received unknown message type");
        break;
    }

    if (buffer)
    {
        vPortFree(buffer);
    }

    return ESP_OK;
}

esp_err_t WSServer::ws_open_fd(httpd_handle_t hd, int sockfd)
{
    if (!client_connected_callback)
    {
        ESP_LOGI(TAG, "New client connected %d", sockfd);
    }
    if (client_connected_callback)
    {
        client_connected_callback(sockfd);
    }

    return keep_alive->addClient(sockfd) ? ESP_OK : ESP_FAIL;
}

void WSServer::ws_close_fd(httpd_handle_t hd, int sockfd)
{

    if (client_disconnected_callback)
    {
        client_disconnected_callback(sockfd);
    }
    else
    {
        ESP_LOGI(TAG, "Client disconnected %d", sockfd);
    }

    keep_alive->removeClient(sockfd);
    authenticated_clients.erase(sockfd);
    close(sockfd);
}

void WSServer::send_ping(void *arg)
{
    async_resp_arg *resp_arg = static_cast<async_resp_arg *>(arg);
    httpd_handle_t hd = resp_arg->hd;
    int fd = resp_arg->fd;
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = NULL;
    ws_pkt.len = 0;
    ws_pkt.type = HTTPD_WS_TYPE_PING;
    httpd_ws_send_frame_async(hd, fd, &ws_pkt);
    vPortFree(resp_arg);
}

bool WSServer::client_not_alive_cb(KeepAlive *h, int fd)
{
    if (debug)
    {
        ESP_LOGE(TAG, "Client not alive, closing fd %d", fd);
    }
    httpd_sess_trigger_close(h->getUserCtx(), fd);
    return true;
}

bool WSServer::check_client_alive_cb(KeepAlive *h, int fd)
{
    ESP_LOGD(TAG, "Checking if client (fd=%d) is alive", fd);
    async_resp_arg *resp_arg = static_cast<async_resp_arg *>(pvPortMalloc(sizeof(async_resp_arg)));
    if (resp_arg == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for async_resp_arg");
        return false;
    }

    resp_arg->hd = h->getUserCtx();
    resp_arg->fd = fd;

    if (httpd_queue_work(resp_arg->hd, send_ping, resp_arg) == ESP_OK)
    {
        return true;
    }
    vPortFree(resp_arg);
    return false;
}

httpd_handle_t WSServer::start_ws_server(uint16_t port)
{
    if (!keep_alive->start())
    {
        ESP_LOGE(TAG, "Failed to start keep alive");
        return NULL;
    }

    httpd_handle_t server = NULL;
    ESP_LOGI(TAG, "Starting server");

    httpd_config_t conf = HTTPD_DEFAULT_CONFIG();
    conf.max_open_sockets = max_clients;
    conf.global_user_ctx = keep_alive;
    conf.open_fn = ws_open_fd;
    conf.close_fn = ws_close_fd;
    conf.server_port = port;

    conf.stack_size = 8192;

    esp_err_t ret = httpd_start(&server, &conf);
    if (ESP_OK != ret)
    {
        ESP_LOGI(TAG, "Error starting server!");
        return NULL;
    }

    ESP_LOGI(TAG, "Registering URI handlers");
    httpd_register_uri_handler(server, &ws);
    keep_alive->setUserCtx(server);

    return server;
}

esp_err_t WSServer::stop_ws_server(httpd_handle_t server)
{
    keep_alive->stop();
    return httpd_stop(server);
}

void WSServer::ws_server_send_messages()
{
    if (debug)
    {
        ESP_LOGW(TAG, "Message task is ready");
    }
    ws_message_t *msg;
    for (;;)
    {
        if (xQueueReceive(ws_send_queue, &msg, portMAX_DELAY) == pdPASS)
        {
            httpd_ws_frame_t ws_pkt;
            memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
            ws_pkt.type = msg->message_type;
            ws_pkt.payload = msg->message;
            ws_pkt.len = msg->length;

            if (ws_pkt.len > MAX_MESSAGE_SIZE)
            {
                ESP_LOGE(TAG, "Message size exceeds buffer limit");
                vPortFree(msg);
                continue;
            }

            if (msg->broadcast)
            {
                for (const auto &client : keep_alive->getClients())
                {
                    if (server && httpd_ws_get_fd_info(server, client->fd) == HTTPD_WS_CLIENT_WEBSOCKET)
                    {
                        esp_err_t send_ret = httpd_ws_send_frame_async(server, client->fd, &ws_pkt);
                        if (send_ret != ESP_OK)
                        {
                            ESP_LOGE(TAG, "Failed to send message to client fd: %d, error: %d", client->fd, send_ret);
                        }
                    }
                    else
                    {
                        ESP_LOGE(TAG, "Client fd: %d is not in WebSocket state or server is null", client->fd);
                    }
                    vTaskDelay(pdMS_TO_TICKS(1));
                }
            }
            else
            {
                if (server && httpd_ws_get_fd_info(server, msg->fd) == HTTPD_WS_CLIENT_WEBSOCKET)
                {
                    esp_err_t send_ret = httpd_ws_send_frame_async(server, msg->fd, &ws_pkt);
                    if (send_ret != ESP_OK)
                    {
                        ESP_LOGE(TAG, "Failed to send message to client fd: %d, error: %d", msg->fd, send_ret);
                    }
                }
                else
                {
                    ESP_LOGE(TAG, "Client fd: %d is not in WebSocket state or server is null", msg->fd);
                }
                vTaskDelay(pdMS_TO_TICKS(1));
            }
            vPortFree(msg);
        }
    }
}

bool WSServer::authenticate(httpd_req_t *req)
{
    if (auth_user == nullptr || auth_password == nullptr)
    {
        return true;
    }

    char auth_buffer[128];
    if (httpd_req_get_hdr_value_str(req, "Authorization", auth_buffer, sizeof(auth_buffer)) == ESP_OK)
    {
        const char *auth_prefix = "Basic ";
        if (strncmp(auth_buffer, auth_prefix, strlen(auth_prefix)) == 0)
        {
            const char *auth_base64 = auth_buffer + strlen(auth_prefix);
            size_t decoded_len;
            unsigned char decoded[64];

            esp_err_t err = mbedtls_base64_decode(decoded, sizeof(decoded), &decoded_len,
                                                  reinterpret_cast<const unsigned char *>(auth_base64), strlen(auth_base64));
            if (err == 0)
            {
                std::string decoded_str(reinterpret_cast<char *>(decoded), decoded_len);
                char expected_auth[128];
                snprintf(expected_auth, sizeof(expected_auth), "%s:%s", auth_user, auth_password);
                std::string expected(expected_auth);

                if (decoded_str == expected)
                {
                    return true;
                }
            }
        }
    }

    httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"WSServer\"");
    httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    return false;
}
