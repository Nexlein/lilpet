/* Firmware entry point. For now it only links the engine and logs its version. */
#include "esp_log.h"
#include "lilpet/lilpet.h"

static const char *TAG = "lilpet";

void app_main(void)
{
    ESP_LOGI(TAG, "lilpet engine %s", lp_version());
}
