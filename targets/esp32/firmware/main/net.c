// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 SedonaPro contributors
//
// Network for the Sedona VM: Wi-Fi station, OpenCores Ethernet (QEMU) or none
// (menuconfig -> Sedona VM -> Network).  Logs the address when it is up.
//
#include <string.h>
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "sdkconfig.h"
#include "net.h"
#if CONFIG_SEDONA_NET_WIFI
#include "esp_wifi.h"
#endif
#if CONFIG_SEDONA_NET_OPENETH
#include "esp_eth.h"
#endif

static const char* TAG = "net";

static void onGotIp(void* arg, esp_event_base_t base, int32_t id, void* data)
{
  ip_event_got_ip_t* e = (ip_event_got_ip_t*)data;
  ESP_LOGI(TAG, "address " IPSTR ", SOX udp/1876", IP2STR(&e->ip_info.ip));
}

#if CONFIG_SEDONA_NET_WIFI
static void onWifi(void* arg, esp_event_base_t base, int32_t id, void* data)
{
  if (id == WIFI_EVENT_STA_START || id == WIFI_EVENT_STA_DISCONNECTED)
    esp_wifi_connect();       // and keep retrying while the AP is away
}

static void wifiStart(void)
{
  wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
  wifi_config_t cfg = { 0 };
  esp_netif_create_default_wifi_sta();
  ESP_ERROR_CHECK(esp_wifi_init(&init));
  esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, onWifi, NULL);
  esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, onGotIp, NULL);
  strlcpy((char*)cfg.sta.ssid, CONFIG_SEDONA_WIFI_SSID, sizeof(cfg.sta.ssid));
  strlcpy((char*)cfg.sta.password, CONFIG_SEDONA_WIFI_PASSWORD, sizeof(cfg.sta.password));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &cfg));
  esp_wifi_set_ps(WIFI_PS_NONE);       // no power save: stable SOX latency
  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_LOGI(TAG, "Wi-Fi station, SSID '%s'", CONFIG_SEDONA_WIFI_SSID);
}
#endif

#if CONFIG_SEDONA_NET_OPENETH
static void openethStart(void)
{
  eth_mac_config_t macCfg = ETH_MAC_DEFAULT_CONFIG();
  eth_phy_config_t phyCfg = ETH_PHY_DEFAULT_CONFIG();
  esp_eth_handle_t eth = NULL;
  esp_netif_config_t netifCfg = ESP_NETIF_DEFAULT_ETH();
  esp_netif_t* netif = esp_netif_new(&netifCfg);
  phyCfg.autonego_timeout_ms = 100;
  esp_eth_mac_t* mac = esp_eth_mac_new_openeth(&macCfg);
  esp_eth_phy_t* phy = esp_eth_phy_new_generic(&phyCfg);   // IDF v6: chip-specific PHYs moved to the registry
  esp_eth_config_t cfg = ETH_DEFAULT_CONFIG(mac, phy);
  ESP_ERROR_CHECK(esp_eth_driver_install(&cfg, &eth));
  ESP_ERROR_CHECK(esp_netif_attach(netif, esp_eth_new_netif_glue(eth)));
  esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, onGotIp, NULL);
  ESP_ERROR_CHECK(esp_eth_start(eth));
  ESP_LOGI(TAG, "OpenCores Ethernet (QEMU)");
}
#endif

void netStart(void)
{
#if CONFIG_SEDONA_NET_NONE
  ESP_LOGI(TAG, "network disabled");
#else
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
#if CONFIG_SEDONA_NET_WIFI
  wifiStart();
#elif CONFIG_SEDONA_NET_OPENETH
  openethStart();
#endif
#endif
}
