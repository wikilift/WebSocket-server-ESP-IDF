#include "wsserver.h"

/**
 *
 * 
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