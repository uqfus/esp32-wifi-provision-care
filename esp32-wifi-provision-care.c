/*
    ESP-IDF examples code used
    Public domain
*/
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include "dns_server.h"
#include "lwip/inet.h"
#include "nvs.h"
#include "cJSON.h"

static const char *TAG = "esp32-wifi-provision-care";
static esp_netif_t *s_wifi_sta_netif = NULL;
static SemaphoreHandle_t s_semph_get_ip_addrs = NULL;
static SemaphoreHandle_t s_semph_get_ip6_addrs = NULL;

// HTTP /favicon.ico
static esp_err_t favicon_get_handler(httpd_req_t *req)
{
    extern const unsigned char favicon_ico_start[] asm("_binary_favicon_ico_start");
    extern const unsigned char favicon_ico_end[]   asm("_binary_favicon_ico_end");
    const size_t favicon_ico_size = (favicon_ico_end - favicon_ico_start);
    httpd_resp_set_type(req, "image/x-icon");
    httpd_resp_send(req, (const char *)favicon_ico_start, favicon_ico_size);
    return ESP_OK;
}

// HTTP Error (404) Handler - Redirects all requests to the WiFi provision page
static esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"), &ip_info);
    char ip_addr[16];
    inet_ntoa_r(ip_info.ip.addr, ip_addr, 16);
    char root_uri[32];
    sprintf(root_uri, "http://%s/wifi", ip_addr);

    httpd_resp_set_status(req, "302 Temporary Redirect");
    httpd_resp_set_hdr(req, "Location", root_uri);
    // iOS requires content in the response to detect a captive portal, simply redirecting is not sufficient.
    httpd_resp_send(req, "Redirect to the captive portal.", HTTPD_RESP_USE_STRLEN);
    ESP_LOGI(TAG, "Redirecting to Wi-Fi provision page %s", root_uri );
    return ESP_OK;
}

// HTTP / - WiFi provision page
static esp_err_t root_get_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Serve root.");
    extern const char wifi_start[] asm("_binary_wifi_html_gz_start");
    extern const char wifi_end[] asm("_binary_wifi_html_gz_end");
    const uint32_t wifi_len = wifi_end - wifi_start;
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    httpd_resp_send(req, wifi_start, wifi_len);
    return ESP_OK;
}

// HTTP /nvserase - Perform NVS erase
static esp_err_t nvserase_get_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Perform NVS full erase.");
    ESP_ERROR_CHECK(nvs_flash_erase());
    ESP_ERROR_CHECK(nvs_flash_init());
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, "Perform NVS full erase.", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// HTTP /savewifi - WiFi provision page
static esp_err_t savewifi_get_handler(httpd_req_t *req)
{
    char  *buf = NULL;
    size_t buf_len;
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            char param[65];
            wifi_config_t wifi_cfg = { 0 };
            if ( httpd_query_key_value( buf, "ssid", param, sizeof(param) ) == ESP_OK ) {
                strncpy( (char *)wifi_cfg.sta.ssid, param, sizeof(wifi_cfg.sta.ssid) );
            }
            if ( httpd_query_key_value( buf, "password", param, sizeof(param) ) == ESP_OK ) {
                strncpy( (char *)wifi_cfg.sta.password, param, sizeof(wifi_cfg.sta.password) );
            }

            if ( strlen((char *)wifi_cfg.sta.ssid) > 0 && strlen((char *)wifi_cfg.sta.password) > 0 ) {
                httpd_resp_set_type(req, "text/html");
                if (esp_wifi_set_storage(WIFI_STORAGE_FLASH) == ESP_OK &&
                        esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg) == ESP_OK) {
                    ESP_LOGI(TAG, "Wi-Fi settings saved, SSID: '%s', password: '%s'.", (char *)wifi_cfg.sta.ssid, (char *)wifi_cfg.sta.password );
                    httpd_resp_send(req, "Wi-Fi settings saved.", HTTPD_RESP_USE_STRLEN);
                } else {
                    ESP_LOGE(TAG, "Failed to write Wi-Fi config to flash.");
                    httpd_resp_send(req, "Failed to write Wi-Fi config to flash.", HTTPD_RESP_USE_STRLEN);
                }
                free(buf);
                esp_restart();
                return ESP_OK;
            }
        }
        free(buf);
    }

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, "Error. Wi-Fi settings incorrect.", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// HTTP /scanap - WiFi  page
static esp_err_t scanap_get_handler(httpd_req_t *req)
{
    uint16_t number = 15;
    wifi_ap_record_t ap_info[15];
    uint16_t ap_count = 0;
    memset(ap_info, 0, sizeof(ap_info));
    ESP_ERROR_CHECK(esp_wifi_scan_start(NULL, true));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
    ESP_LOGI(TAG, "Total APs scanned = %u, actual AP number ap_info holds = %u", ap_count, number);

    httpd_resp_set_type(req, "application/json");
    cJSON *fld = NULL;
    cJSON *root = cJSON_CreateArray();
    for (int i = 0; i < number; i++) {
        ESP_LOGI(TAG, "SSID %s RSSI %d CHANNEL %d", ap_info[i].ssid, ap_info[i].rssi, ap_info[i].primary);
        cJSON_AddItemToArray(root, fld = cJSON_CreateObject());
        cJSON_AddStringToObject(fld, "ssid", (char *)ap_info[i].ssid);
        cJSON_AddNumberToObject(fld, "rssi", ap_info[i].rssi);
        cJSON_AddNumberToObject(fld, "auth", ap_info[i].authmode);
    }

    const char *scanap = cJSON_Print(root);
    httpd_resp_sendstr(req, scanap);
    free((void *)scanap);
    cJSON_Delete(root);
    return ESP_OK;
}

static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_open_sockets = 13;
    config.lru_purge_enable = true;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting http server on port: %d", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        const httpd_uri_t favicon_uri = { .uri = "/favicon.ico", .method = HTTP_GET, .handler = favicon_get_handler };
        httpd_register_uri_handler(server, &favicon_uri);
        const httpd_uri_t root_uri =    { .uri = "/wifi", .method = HTTP_GET, .handler = root_get_handler };
        httpd_register_uri_handler(server, &root_uri);
        const httpd_uri_t scanap_uri =  { .uri = "/scanap", .method = HTTP_GET, .handler = scanap_get_handler };
        httpd_register_uri_handler(server, &scanap_uri);
        const httpd_uri_t savewifi_uri =  { .uri = "/savewifi", .method = HTTP_GET, .handler = savewifi_get_handler };
        httpd_register_uri_handler(server, &savewifi_uri);
        const httpd_uri_t nvserase_uri =  { .uri = "/nvserase", .method = HTTP_GET, .handler = nvserase_get_handler };
        httpd_register_uri_handler(server, &nvserase_uri);
        httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, http_404_error_handler);
    }
    return server;
}

// Switch Wi-FI from STA mode to STA+SoftAP mode. Start Wi-Fi provisioning with captive portal.
static void wifi_init_softap(void *pvParameters)
{
    ESP_LOGI(TAG, "SoftAP init.");
    vTaskDelay( 3000 / portTICK_PERIOD_MS );

    // Initialize SoftAP with default config
    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = "",
            .password = "",
            .authmode = WIFI_AUTH_OPEN,
            .max_connection = 4,
        },
    };
    if ( pvParameters != NULL && strlen( (char *)pvParameters ) > 0 ) {
        // Open SoftAP SSID with provided name
        strncpy((char *)wifi_config.ap.ssid, (char *)pvParameters, sizeof(wifi_config.ap.ssid));
    } else {
        // Open SoftAP SSID with TAG+MAC
        uint8_t ap_mac[6];
        ESP_ERROR_CHECK(esp_read_mac(ap_mac, ESP_MAC_EFUSE_FACTORY));
        sprintf((char *)wifi_config.ap.ssid, "%s-%02X%02X", TAG, ap_mac[4], ap_mac[5]);
    }

    // Reconfigure DHCPd. With IP 192.168.*.* 10.*.*.* 172.16-31.*.* captive portal page do not popup automatically
    ESP_ERROR_CHECK(esp_netif_dhcps_stop(ap_netif));
    esp_netif_ip_info_t ip_info;
    IP4_ADDR(&ip_info.ip, 200, 200, 200, 2);
    IP4_ADDR(&ip_info.gw, 200, 200, 200, 1);
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
    ESP_ERROR_CHECK(esp_netif_set_ip_info(ap_netif, &ip_info));
    ESP_ERROR_CHECK(esp_netif_dhcps_start(ap_netif));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA)); // STA -> APSTA
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM)); // Do not use NVS for SoftAP config
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "SoftAP started. Connect to SSID:'%s' with password:'%s'", wifi_config.ap.ssid, wifi_config.ap.password);
    start_webserver();

    // Start the DNS server that will redirect all queries to the softAP IP
    esp_log_level_set("example_dns_redirect_server", ESP_LOG_NONE); // turn off DNS server logging
    dns_server_config_t dns_config = DNS_SERVER_CONFIG_SINGLE("*" /* all A queries */, "WIFI_AP_DEF" /* softAP netif ID */);
    start_dns_server(&dns_config);
    vTaskSuspend(NULL); // There is only esp_restart(); (and variables in memory may cause undefined behavior)
}

static int s_retry_num = 0; // Retry attempts counter
static void sta_wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    // event_base == WIFI_EVENT
    switch (event_id)
    {
    case WIFI_EVENT_STA_START:
        ESP_LOGI(TAG, "Wi-Fi iface up. Connecting ...");
        esp_wifi_connect();
        break;

    case WIFI_EVENT_STA_CONNECTED:
        s_retry_num = 0;
        esp_netif_create_ip6_linklocal(s_wifi_sta_netif);
        break;

    case WIFI_EVENT_STA_DISCONNECTED:
        s_retry_num++;
        if ( s_retry_num > 5 ) {
            ESP_LOGE(TAG, "Connection attempt failed. Starting Wi-Fi Provisioning AP.");
            xTaskCreate(wifi_init_softap, "start_softap", 4096, arg, (tskIDLE_PRIORITY + 2), NULL); // Start Wi-Fi SoftAP
        } else {
            ESP_LOGI(TAG, "Connection attempt %d of 5 is failed, retry in 3 seconds to connect to the AP.", s_retry_num);
            vTaskDelay(3000 / portTICK_PERIOD_MS);
            esp_wifi_connect();
        }
        break;

    default:
        break;
    }
}

static void sta_ip_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    // event_base == IP_EVENT 
    switch (event_id)
    {
    case IP_EVENT_STA_GOT_IP:
        ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
        ESP_LOGI(TAG, "Got IPv4 address: " IPSTR, IP2STR(&event->ip_info.ip));
        xSemaphoreGive(s_semph_get_ip_addrs);
        break;

    case IP_EVENT_GOT_IP6:
        ip_event_got_ip6_t *event6 = (ip_event_got_ip6_t *)event_data;
        ESP_LOGI(TAG, "Got IPv6 : Interface \"%s\" address: " IPV6STR, esp_netif_get_desc(event6->esp_netif),
             IPV62STR(event6->ip6_info.ip));
        xSemaphoreGive(s_semph_get_ip6_addrs);
        break;
    
    default:
        break;
    }
}

void wifi_provision_care(char *ap_ssid_name)
{
    s_semph_get_ip_addrs = xSemaphoreCreateBinary();
    if (s_semph_get_ip_addrs == NULL) {
        return ;
    }
    s_semph_get_ip6_addrs = xSemaphoreCreateBinary();
    if (s_semph_get_ip6_addrs == NULL) {
        vSemaphoreDelete(s_semph_get_ip_addrs);
        return ;
    }
    char ap_ssid_name_copy[33];
    strncpy((char *)ap_ssid_name_copy, ap_ssid_name, sizeof(ap_ssid_name_copy));

    ESP_LOGI(TAG, "Wi-Fi interface init.");

    // Secondary NVS init is fine
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        ESP_ERROR_CHECK(nvs_flash_erase());
        // Retry nvs_flash_init
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    s_wifi_sta_netif = esp_netif_create_default_wifi_sta();
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));

    wifi_config_t wifi_config;
    if (esp_wifi_get_config(WIFI_IF_STA, &wifi_config) != ESP_OK) {
        // configuration not available, report error to restart provisioning
        ESP_LOGE(TAG, "Error (%s) reading Wi-Fi Credentials. Starting Wi-Fi provisioning AP.", esp_err_to_name(err));
        vSemaphoreDelete(s_semph_get_ip_addrs);
        vSemaphoreDelete(s_semph_get_ip6_addrs);
        wifi_init_softap(ap_ssid_name_copy); // Start WIFI SoftAP
        return;
    }
    if ( strlen((const char *) wifi_config.sta.ssid ) == 0 ) {
        // configuration not available, report error to restart provisioning
        ESP_LOGE(TAG, "Wi-Fi SSID empty. Starting Wi-Fi provisioning AP.");
        vSemaphoreDelete(s_semph_get_ip_addrs);
        vSemaphoreDelete(s_semph_get_ip6_addrs);
        wifi_init_softap(ap_ssid_name_copy); // Start WIFI SoftAP
        return;
    }
    ESP_LOGI(TAG, "Wi-Fi credentials loaded. SSID:'%s' Password:hidden", wifi_config.sta.ssid);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &sta_wifi_event_handler, ap_ssid_name_copy));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,   ESP_EVENT_ANY_ID, &sta_ip_event_handler, NULL));
    ESP_ERROR_CHECK(esp_wifi_start());

    xSemaphoreTake(s_semph_get_ip_addrs, portMAX_DELAY);
    xSemaphoreTake(s_semph_get_ip6_addrs, portMAX_DELAY);
    ESP_LOGI(TAG, "Wi-Fi interface up and ready");
    vSemaphoreDelete(s_semph_get_ip_addrs);
    vSemaphoreDelete(s_semph_get_ip6_addrs);
    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &sta_wifi_event_handler));
    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT,   ESP_EVENT_ANY_ID, &sta_ip_event_handler));
}
