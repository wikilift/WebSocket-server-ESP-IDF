#include "wsserver.h"

/**
 * 
 * This function initializes and starts the WebSocket server with default parameters.
 * The server will start in Access Point mode with the specified SSID and password. 
 * SSL is disabled, and the server allows a maximum of 4 clients with keep-alive checks every 10 seconds.
 * Authentication is required with the specified username and password.
 * It also sets up the necessary callbacks to handle incoming WebSocket messages and prints the received messages.
 * 
 * Headers for PostMan:
 * Key:Authorization Value:Basic YWRtaW46YWRtaW4= 
 */
extern "C" void app_main(void)
{
    WSServer &server = WSServer::getInstance();

    // Start the WebSocket server with authentication
    server.start(80, "default_ssid", "default_password", true, false, 4, 10000, 30000, "admin", "admin", nullptr);

    // Set the callback for text messages
    server.onTextMessage([](httpd_req_t *req, httpd_ws_frame_t &frame) {
        ESP_LOGI("SimpleCallback", "Received text message: %s", frame.payload);
    });

    // Set the callback for binary messages
    server.onBinaryMessage([](httpd_req_t *req, httpd_ws_frame_t &frame) {
        ESP_LOGI("SimpleCallback", "Received binary message");
        esp_log_buffer_hex("Received WS binary", frame.payload, frame.len);
    });

    // Set the callback for when a client connects
    server.onClientConnected([](int sockfd) {
        ESP_LOGI("SimpleCallback", "Client connected: %d", sockfd);
    });

    // Set the callback for when a client disconnects
    server.onClientDisconnected([](int sockfd) {
        ESP_LOGI("SimpleCallback", "Client disconnected: %d", sockfd);
    });
}