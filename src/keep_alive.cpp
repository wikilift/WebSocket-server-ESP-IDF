/**
 * @file keep_alive.cpp
 * @brief KeepAlive class implementation.
 *
 * This file defines the KeepAlive class implementation.
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
#include "keep_alive.h"
#include <algorithm>

KeepAlive::KeepAlive(const Config &config) : config(config), queue(nullptr), running(false) {}

KeepAlive::~KeepAlive()
{
    stop();

    for (auto client : clients)
    {
        delete client;
    }
    clients.clear();
}

bool KeepAlive::start()
{
    if (running)
    {
        return false;
    }
    queue = xQueueCreate(config.max_clients, sizeof(Client *));
    if (!queue)
    {
        return false;
    }
    running = true;

    xTaskCreatePinnedToCore(task, "keep_alive_task", config.task_stack_size, this, config.task_prio, nullptr, 1);

    return true;
}

void KeepAlive::stop()
{
    if (!running)
        return;
    running = false;

    Client *dummy = new (std::nothrow) Client{-1, 0};
    if (dummy != nullptr)
    {
        xQueueSend(queue, &dummy, portMAX_DELAY);
        delete dummy;
    }

    vQueueDelete(queue);
    queue = nullptr;

    for (auto client : clients)
    {
        delete client;
    }
    clients.clear();
}

bool KeepAlive::addClient(int fd)
{
    for (const auto &client : clients)
    {
        if (client->fd == fd)
            return false;
    }
    Client *new_client = new (std::nothrow) Client{fd, (uint64_t)esp_timer_get_time() / 1000};
    if (new_client == nullptr)
    {
        return false;
    }
    clients.push_back(new_client);
    return true;
}

bool KeepAlive::removeClient(int fd)
{
    auto it = std::remove_if(clients.begin(), clients.end(), [fd](Client *client)
                             { return client->fd == fd; });
    if (it != clients.end())
    {
        for (auto i = it; i != clients.end(); ++i)
        {
            delete *i;
        }
        clients.erase(it, clients.end());
        return true;
    }
    return false;
}
bool KeepAlive::updateClient(int fd)
{
    for (auto &client : clients)
    {
        if (client->fd == fd)
        {
            client->last_seen = esp_timer_get_time() / 1000;
            if (debug)
            {
                ESP_LOGI("KeepAlive", "Updated last_seen for client fd: %d", fd);
            }
            return true;
        }
    }
    if (debug)
    {
        ESP_LOGW("KeepAlive", "Client fd: %d not found for update", fd);
    }
    return false;
}

void KeepAlive::setUserCtx(void *ctx)
{
    config.user_ctx = ctx;
}

void *KeepAlive::getUserCtx() const
{
    return config.user_ctx;
}

const std::vector<KeepAlive::Client *> &KeepAlive::getClients() const
{
    return clients;
}

uint64_t KeepAlive::getMaxDelay() const
{
    uint64_t now = esp_timer_get_time() / 1000;
    uint64_t max_delay = config.keep_alive_period_ms;
    for (const auto &client : clients)
    {
        uint64_t delay = config.keep_alive_period_ms - (now - client->last_seen);
        if (delay < max_delay)
            max_delay = delay;
    }
    return max_delay;
}

void KeepAlive::task(void *arg)
{
    KeepAlive *self = static_cast<KeepAlive *>(arg);
    Client *client;
    while (self->running)
    {
        if (xQueueReceive(self->queue, &client, self->getMaxDelay() / portTICK_PERIOD_MS) == pdTRUE)
        {
            if (client->fd == -1)
            {
                break;
            }
            self->updateClient(client->fd);
        }

        uint64_t now = esp_timer_get_time() / 1000;
        for (auto &c : self->clients)
        {
            if (now - c->last_seen > self->config.not_alive_after_ms)
            {
                self->config.client_not_alive_cb(self, c->fd);
            }
            else if (now - c->last_seen > self->config.keep_alive_period_ms)
            {
                self->config.check_client_alive_cb(self, c->fd);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    vTaskDelete(nullptr);
}
