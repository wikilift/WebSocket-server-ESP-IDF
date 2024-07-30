# WSServer Library for ESP-IDF 📡

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v4.4+-blue.svg)](https://github.com/espressif/esp-idf)

This library provides a  WebSocket server implementation for ESP-IDF framework.

## Features ✨

- **WebSocket Server:** Handles multiple WebSocket connections with ease.
- **SSL/TLS Support:** Secure your communications with SSL/TLS (self-signed certificates by default).
- **Authentication:** Basic HTTP Authentication to control access.
- **Keep-Alive:** Ensures clients remain connected and responsive.
- **Callbacks:** Customizable callbacks for handling text, binary, and close messages, as well as client connect and disconnect events.

## Getting Started 🚀

### Prerequisites 📋
- ESP-IDF v4.4 or later.
- ESP32 development board.
- CMake build system.

### Installation 📥

1. **Clone the repository:**
 ```sh
    git clone https://github.com/yourusername/wsserver.git
``` 

2. **Add WSServer as a component to your ESP-IDF project:**
 Copy the `wsserver` directory into the `components` directory of your ESP-IDF project. 
 
3. **Configure your project:**
 Make sure to enable WebSocket support in your project configuration:
    ```sh
    idf.py menuconfig
    Enable WebSocket support: Component config -> ESP HTTP server -> Enable ESP_HTTPS_SERVER component
 
### Usage 📝

Here are some example usages of the WSServer library:

#### Basic WebSocket Server 🌟

```cpp
#include "wsserver.h"

/**
 * This function initializes and starts the WebSocket server with default parameters.
 * The server will start in Access Point mode with the default SSID "default_ssid" and 
 * password "default_password". SSL is disabled, and the server allows a maximum of 4 
 * clients with keep-alive checks every 10 seconds. No authentication is required by default.
 */
extern "C" void app_main(void)
{
    WSServer &server = WSServer::getInstance();
    server.start();
}
``` 

#### Echo Server Example 🔄

```cpp


#include "wsserver.h"

/**
 * This function initializes and starts the WebSocket server with default parameters.
 * The server will start in Access Point mode with the default SSID "default_ssid" and 
 * password "default_password". SSL is disabled, and the server allows a maximum of 4 
 * clients with keep-alive checks every 10 seconds. No authentication is required by default.
 * It also sets up the necessary callbacks to handle incoming WebSocket messages and 
 * implements an echo functionality.
 */
extern "C" void app_main(void) {
    WSServer &server = WSServer::getInstance();

    // Start the WebSocket server
    server.start(80, "default_ssid", "default_password", true, false, 4, 10000, 30000, nullptr);

    // Set the callback for text messages
    server.onTextMessage([&server](httpd_req_t *req, httpd_ws_frame_t &frame) {
        ESP_LOGI("EchoServer", "Received text message: %s", frame.payload);
        server.sendText(httpd_req_to_sockfd(req), (const char *)frame.payload);
    });

    // Set the callback for binary messages
    server.onBinaryMessage([&server](httpd_req_t *req, httpd_ws_frame_t &frame) {
        ESP_LOGI("EchoServer", "Received binary message");
        esp_log_buffer_hex("Received WS binary", frame.payload, frame.len);
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
```

#### Secure Server with Authentication 🔒

```cpp

#include "wsserver.h"

/**
 * This function initializes and starts the WebSocket server with default parameters.
 * The server will start in Access Point mode with the specified SSID and password. 
 * SSL is enabled, using certificates issued by a trusted authority.
 * The server allows a maximum of 4 clients with keep-alive checks every 10 seconds.
 * Authentication is required with the specified username and password.
 * It also sets up the necessary callbacks to handle incoming WebSocket messages.
 *
 * Note: To use this example, you need to provide your own trusted certificates.
 * Place your server certificate and private key in the /certs folder as servercert.pem and prvtkey.pem, respectively.
 * Alternatively, if using self-signed certificates, ensure they are added to the trusted certificates on your client system.
 *
 * Headers for PostMan:
 * Key: Authorization
 * Value: Basic YWRtaW46YWRtaW4=
 */
extern "C" void app_main(void) {
    WSServer &server = WSServer::getInstance();

    // Start the WebSocket server with authentication
    server.start(443, "default_ssid", "default_password", true, true, 4, 10000, 30000, "admin", "admin", nullptr);

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
```

## Documentation 📚

For  documentation, please refer to the Doxygen-generated documentation in the `.h` files.

## Contributing 🤝

Contributions are welcome! Please open an issue or submit a pull request for any improvements or bug fixes.

## License 📝

This project is licensed under the MIT License. See the LICENSE file for details.

----------

Happy coding! 🎉

