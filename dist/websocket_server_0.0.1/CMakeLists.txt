idf_component_register(
    SRCS "src/wsserver.cpp" "src/keep_alive.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_server freertos nvs_flash esp_event esp_netif esp_wifi esp_timer esp_eth mbedtls esp_https_server
    EMBED_TXTFILES "certs/servercert.pem" "certs/prvtkey.pem"
)
