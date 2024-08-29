#include "wsserver.h"

/**
 * 
 * 
 * This function initializes and starts the WebSocket server with default parameters.
 * The server will start in Access Point mode with the default SSID "default_ssid" and 
 * password "default_password". SSL is disabled, and the server allows a maximum of 4 
 * clients with keep-alive checks every 10 seconds. No authentication is required by default.
 * It also sets up the necessary callbacks to handle incoming WebSocket messages and 
 * implements an echo functionality.
 */
extern "C" void app_main(void)
{
    WSServer &server = WSServer::getInstance();

    // Start the WebSocket server
    server.start(80,                    /*<< port*/
                 'mySSID',          /*<< ssid*/
                 "mySecretPwd", /*<< password*/
                 true,                  /*<< isAP*/
                 false,                 /*<< SSL*/
                 1,                     /*<< max_clients*/
                 10000,                 /*<< keep_alive_period*/
                 30000,                 /*<< not_alive_after*/
                 nullptr,               /*<< auth_user*/
                 nullptr, /*<< auth_pass*/
                 true,  /*<< keep_alive_task*/
                 nullptr // Start DHCP server
                 );

    // Set the callback for text messages
    server.onTextMessage([&server](httpd_req_t *req, httpd_ws_frame_t &frame) {
        ESP_LOGI("EchoServer", "Received text message: %s", frame.payload);

        // Echo the received message back to the client
        server.sendText(httpd_req_to_sockfd(req), (const char *)frame.payload);
    });

    // Set the callback for binary messages
    server.onBinaryMessage([&server](httpd_req_t *req, httpd_ws_frame_t &frame) {
        ESP_LOGI("EchoServer", "Received binary message");
        esp_log_buffer_hex("Received WS binary", frame.payload, frame.len);

        // Echo the received binary data back to the client
        server.sendBinary(httpd_req_to_sockfd(req), frame.payload, frame.len);
    });

    // Set the callback for when a client connects
    server.onClientConnected([](int sockfd) {
        ESP_LOGI("EchoServer", "Client connected: %d", sockfd);
    });

    // Set the callback for when a client disconnects
    server.onClientDisconnected([](int sockfd) {
        ESP_LOGI("EchoServer", "Client disconnected: %d", sockfd);
    });
}