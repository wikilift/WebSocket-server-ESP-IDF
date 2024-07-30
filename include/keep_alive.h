#pragma once

#include <vector>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_timer.h>

class KeepAlive
{
public:
    struct Client 
    {
        int fd;
        uint64_t last_seen;
    };

    struct Config
    {
        size_t max_clients;
        size_t task_stack_size;
        size_t task_prio;
        size_t keep_alive_period_ms;
        size_t not_alive_after_ms;
        bool (*check_client_alive_cb)(KeepAlive *h, int fd);
        bool (*client_not_alive_cb)(KeepAlive *h, int fd);
        void *user_ctx;
    };

    KeepAlive(const Config &config);
    ~KeepAlive();

    bool start();
    void stop();

    bool addClient(int fd);
    bool removeClient(int fd);
    bool updateClient(int fd);

    void setUserCtx(void *ctx);
    void *getUserCtx() const;
    const std::vector<Client>& getClients() const; 

private:
    static void task(void *arg);
    uint64_t getMaxDelay() const;

    Config config;
    QueueHandle_t queue;
    std::vector<Client> clients;
    bool running;
};


