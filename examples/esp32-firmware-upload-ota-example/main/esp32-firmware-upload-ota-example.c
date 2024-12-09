/*
    ESP-IDF examples code used
    Public domain
*/
#include "esp_system.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_ota_ops.h" // esp_ota_get_running_partition
#include "esp_http_server.h"
#include "lwip/inet.h" // inet_ntoa_r

// component espressif/mdns
#include <mdns.h>

// component uqfus/esp32-wifi-provision-care https://github.com/uqfus/esp32-wifi-provision-care.git
#include "esp32-wifi-provision-care.h"

static const char *TAG = "esp32-firmware-upload-ota-example";

// HTTP Error (404) Handler - Redirects all requests to /
static esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    // get self IPv4 address
    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(esp_netif_get_default_netif(), &ip_info);
    char ip_addr[16]; // "111.222.111.222"
    inet_ntoa_r(ip_info.ip.addr, ip_addr, 16);
    char root_uri[32];
    sprintf(root_uri, "http://%s/", ip_addr);

    httpd_resp_set_status(req, "302 Temporary Redirect");
    httpd_resp_set_hdr(req, "Location", root_uri);
    httpd_resp_send(req, "Redirect.", HTTPD_RESP_USE_STRLEN);
    ESP_LOGI(TAG, "Redirecting to %s", root_uri );
    return ESP_OK;
}

// HTTP /
static esp_err_t root_get_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Serve root.");
    extern const char wifi_start[] asm("_binary_ota_html_start");
    extern const char wifi_end[] asm("_binary_ota_html_end");
    const uint32_t wifi_len = wifi_end - wifi_start;
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, wifi_start, wifi_len);
    return ESP_OK;
}

static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Start the httpd server
    ESP_LOGI(TAG, "Starting http server on port: %d", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        const httpd_uri_t root_uri =      { .uri = "/",          .method = HTTP_GET,  .handler = root_get_handler };
        httpd_register_uri_handler(server, &root_uri);
        // wifi_provision_care_updateota_post_handler POST handler from component uqfus/esp32-wifi-provision-care
        const httpd_uri_t updateota_uri = { .uri = "/updateota", .method = HTTP_POST, .handler = wifi_provision_care_updateota_post_handler };
        httpd_register_uri_handler(server, &updateota_uri);
        httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, http_404_error_handler);
    }
    return server;
}

static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    }
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "retry to connect to the AP");
    }
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
        // required to obtain IPv6 global addr via SLAAC
        esp_netif_create_ip6_linklocal(esp_netif_get_default_netif());
    }
}

static void wifi_init(void)
{
    ESP_LOGI(TAG, "Wi-Fi interface init.");
    esp_log_level_set("wifi", ESP_LOG_WARN);      // WARN logs from WiFi stack only

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM)); // Do not save wifi settings to NVS partition
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_EXAMPLE_WIFI_SSID,
            .password = CONFIG_EXAMPLE_WIFI_PASSWORD,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));

//    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE)); // Disabling any Wi-Fi power save mode, this allows best throughput, does not have much impact
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Wi-Fi interface up and ready");
}

void app_main(void)
{
    ESP_LOGI(TAG, "Example firmware upload ota entry point.");
    const esp_partition_t *running_partition = esp_ota_get_running_partition();
    ESP_LOGI(TAG, "Boot partition is '%s'.", running_partition->label);

    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // You can choose which method to use while conecting to wifi
    // Choose one and comment other
//    wifi_provision_care((char *)TAG);
    wifi_init();

    ESP_LOGI(TAG, "Publish mDNS hostname %s.local.", TAG);
    ESP_ERROR_CHECK(mdns_init());
    ESP_ERROR_CHECK(mdns_hostname_set(TAG));

    start_webserver();

    while(true) {
        vTaskDelay(60000 / portTICK_PERIOD_MS);
        ESP_LOGI(TAG, "Minimum free heap size: %" PRIu32 " bytes", esp_get_minimum_free_heap_size());

        esp_netif_ip_info_t ip_info;
        esp_netif_get_ip_info(esp_netif_get_default_netif(), &ip_info);
        ESP_LOGI(TAG, "- IP address: " IPSTR, IP2STR(&ip_info.ip));
 
        esp_ip6_addr_t ip6[5];
        int ip6_addrs = esp_netif_get_all_ip6(esp_netif_get_default_netif(), ip6);
        for (int j = 0; j < ip6_addrs; ++j) {
            esp_ip6_addr_type_t ipv6_type = esp_netif_ip6_get_addr_type(&(ip6[j]));
            ESP_LOGI(TAG, "- IPv6 address: " IPV6STR ", type: %d", IPV62STR(ip6[j]), ipv6_type);
        }
    }
    // Task functions should never return.
    vTaskDelete(NULL);
}