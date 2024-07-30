#include "wsserver.h"

/**
 * @brief Start the WebSocket server with SSL enabled.
 *
 * This function initializes and starts the WebSocket server with SSL enabled.
 * The server will start in Access Point mode with the specified SSID and password. 
 * SSL is enabled, using certificates issued by a trusted authority.
 * The server allows a maximum of 4 clients with keep-alive checks every 10 seconds.
 * Authentication is not required.
 * It also sets up the necessary callbacks to handle incoming WebSocket messages.
 *
 * Note: To use this example, you need to provide your own trusted certificates.
 * Place your server certificate and private key in the componenets/wsserver/certs folder as servercert.pem and prvtkey.pem, respectively.
 * Alternatively, if using self-signed certificates, ensure they are added to the trusted certificates on your client system.
 */
extern "C" void app_main(void)
{
    WSServer &server = WSServer::getInstance();

    // Start the WebSocket server with SSL enabled
    server.start(443, "default_ssid", "default_password", true, true, 4, 10000, 30000, nullptr);

    // Set the callback for text messages
    server.onTextMessage([](httpd_req_t *req, httpd_ws_frame_t &frame) {
        ESP_LOGI("SSLServer", "Received text message: %s", frame.payload);
    });

    // Set the callback for binary messages
    server.onBinaryMessage([](httpd_req_t *req, httpd_ws_frame_t &frame) {
        ESP_LOGI("SSLServer", "Received binary message");
        esp_log_buffer_hex("Received WS binary", frame.payload, frame.len);
    });

    // Set the callback for when a client connects
    server.onClientConnected([](int sockfd) {
        ESP_LOGI("SSLServer", "Client connected: %d", sockfd);
    });

    // Set the callback for when a client disconnects
    server.onClientDisconnected([](int sockfd) {
        ESP_LOGI("SSLServer", "Client disconnected: %d", sockfd);
    });
}
