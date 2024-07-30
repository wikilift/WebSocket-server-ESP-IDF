#include "keep_alive.h"
#include <algorithm>

KeepAlive::KeepAlive(const Config &config) : config(config), queue(nullptr), running(false) {}

KeepAlive::~KeepAlive() {
    stop();
}

bool KeepAlive::start() {
    if (running) return false;
    queue = xQueueCreate(config.max_clients, sizeof(Client));
    if (!queue) return false;
    running = true;
    xTaskCreatePinnedToCore(task, "keep_alive_task", config.task_stack_size, this, config.task_prio, nullptr,1);
    return true;
}

void KeepAlive::stop() {
    if (!running) return;
    running = false;
    Client dummy = {-1, 0};
    xQueueSend(queue, &dummy, portMAX_DELAY);
    vQueueDelete(queue);
    queue = nullptr;
}

bool KeepAlive::addClient(int fd) {
    for (auto &client : clients) {
        if (client.fd == fd) return false;
    }
    clients.push_back({fd,(uint64_t) esp_timer_get_time() / 1000});
    return true;
}

bool KeepAlive::removeClient(int fd) {
    auto it = std::remove_if(clients.begin(), clients.end(), [fd](const Client &client) {
        return client.fd == fd;
    });
    if (it != clients.end()) {
        clients.erase(it, clients.end());
        return true;
    }
    return false;
}

bool KeepAlive::updateClient(int fd)
{
    for (auto &client : clients)
    {
        if (client.fd == fd)
        {
            client.last_seen = esp_timer_get_time() / 1000;
           // ESP_LOGI("KeepAlive", "Updated last_seen for client fd: %d", fd);
            return true;
        }
    }
    ESP_LOGW("KeepAlive", "Client fd: %d not found for update", fd);
    return false;
}


void KeepAlive::setUserCtx(void *ctx) {
    config.user_ctx = ctx;
}

void *KeepAlive::getUserCtx() const {
    return config.user_ctx;
}

const std::vector<KeepAlive::Client>& KeepAlive::getClients() const {
    return clients;
}

uint64_t KeepAlive::getMaxDelay() const {
    uint64_t now = esp_timer_get_time() / 1000;
    uint64_t max_delay = config.keep_alive_period_ms;
    for (const auto &client : clients) {
        uint64_t delay = config.keep_alive_period_ms - (now - client.last_seen);
        if (delay < max_delay) max_delay = delay;
    }
    return max_delay;
}

void KeepAlive::task(void *arg) {
    KeepAlive *self = static_cast<KeepAlive *>(arg);
    Client client;
    while (self->running) {
        if (xQueueReceive(self->queue, &client, self->getMaxDelay() / portTICK_PERIOD_MS) == pdTRUE) {
            if (client.fd == -1) {
                break;
            }
            self->updateClient(client.fd);
        }

        uint64_t now = esp_timer_get_time() / 1000;
        for (auto &c : self->clients) {
            if (now - c.last_seen > self->config.not_alive_after_ms) {
                self->config.client_not_alive_cb(self, c.fd);
            } else if (now - c.last_seen > self->config.keep_alive_period_ms) {
                self->config.check_client_alive_cb(self, c.fd);
            }
        }
        taskYIELD();
    }

    vTaskDelete(nullptr);
}


