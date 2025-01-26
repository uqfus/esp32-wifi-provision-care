/*
    ESP-IDF examples code used
    Public domain
*/
#include "esp_system.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"

// component espressif/mdns
#include <mdns.h>

// component uqfus/esp32-wifi-provision-care https://github.com/uqfus/esp32-wifi-provision-care.git
#include "esp32-wifi-provision-care.h"

static const char *TAG = "esp32-wifi-provision-care-example";

// MARK: NVS stats
// dump NVS partition stats
static void nvs_list_all(void)
{
    const char *TAG = __func__;
    nvs_stats_t nvs_stats;
    ESP_ERROR_CHECK(nvs_get_stats(NULL, &nvs_stats));
    ESP_LOGI(TAG, "NVS: UsedEntries = %u, FreeEntries = %u, AvailableEntries = %u, AllEntries = %u, AllNamespaces= (%u)",
          nvs_stats.used_entries, nvs_stats.free_entries, nvs_stats.available_entries, nvs_stats.total_entries, nvs_stats.namespace_count);
    nvs_iterator_t it = NULL;

    esp_err_t res = nvs_entry_find("nvs", NULL, NVS_TYPE_ANY, &it);
    while(res == ESP_OK)
    {
        nvs_entry_info_t info;
        nvs_entry_info(it, &info); // Can omit error check if parameters are guaranteed to be non-NULL
        ESP_LOGI(TAG, "NVS: namespace '%s' : key '%s', type '%d' ", info.namespace_name, info.key, info.type );
        res = nvs_entry_next(&it);
    }
    nvs_release_iterator(it);
}

// MARK: app_main
void app_main(void)
{
    ESP_LOGI(TAG, "Example entry point.");

    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // print "nvs" partition usage
    nvs_list_all();

    wifi_provision_care((char *)TAG);

    ESP_LOGI(TAG, "Publish mDNS hostname %s.local.", TAG);
    ESP_ERROR_CHECK(mdns_init());
    ESP_ERROR_CHECK(mdns_hostname_set(TAG));

    while(true)
    {
        ESP_LOGI(TAG, "Minimum free heap size: %" PRIu32 " bytes", esp_get_minimum_free_heap_size());

        esp_netif_ip_info_t ip_info;
        esp_netif_get_ip_info(esp_netif_get_default_netif(), &ip_info);
        ESP_LOGI(TAG, "- IP address: " IPSTR, IP2STR(&ip_info.ip));
 
        esp_ip6_addr_t ip6[5];
        int ip6_addrs = esp_netif_get_all_ip6(esp_netif_get_default_netif(), ip6);
        for (int j = 0; j < ip6_addrs; ++j)
        {
            esp_ip6_addr_type_t ipv6_type = esp_netif_ip6_get_addr_type(&(ip6[j]));
            ESP_LOGI(TAG, "- IPv6 address: " IPV6STR ", type: %d", IPV62STR(ip6[j]), ipv6_type);
        }
        vTaskDelay(15000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL); // Task functions should never return.
}