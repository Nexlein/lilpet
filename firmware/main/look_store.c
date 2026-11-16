#include "look_store.h"

#include "nvs.h"
#include "nvs_flash.h"

#include <stdio.h>

#define NAMESPACE "lilpet"
#define KEY       "look" /* the look as text (lp_look_format), readable whatever the struct layout */

void look_store_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        /* Storage full or from another format: start from scratch */
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    if (err != ESP_OK)
        printf("flash storage unavailable (%s): looks will not be kept\n", esp_err_to_name(err));
}

bool look_store_load(lp_look *look)
{
    nvs_handle_t nvs;
    if (nvs_open(NAMESPACE, NVS_READONLY, &nvs) != ESP_OK)
        return false; /* nothing kept yet (the first save creates the namespace), or no flash storage, said at init */
    char text[LP_LOOK_TEXT_SIZE];
    size_t size = sizeof text;
    const esp_err_t err = nvs_get_str(nvs, KEY, text, &size);
    nvs_close(nvs);
    /* A value damaged in flash fails its CRC: NVS erases it and answers this too, as if nothing was kept */
    if (err == ESP_ERR_NVS_NOT_FOUND)
        return false;
    /* Written by a firmware that knew another pupil type, or of the wrong length: the board shows the default look */
    if (err != ESP_OK || !lp_look_parse(text, look)) {
        /* Not the text itself: a damaged one could put a line on the port that reads like a reply */
        printf("saved look unreadable (%s): the default look instead\n",
               err == ESP_OK ? "not a look" : esp_err_to_name(err));
        return false;
    }
    return true;
}

bool look_store_save(const lp_look *look)
{
    char text[LP_LOOK_TEXT_SIZE];
    if (lp_look_format(look, text) < 0)
        return false;
    nvs_handle_t nvs;
    if (nvs_open(NAMESPACE, NVS_READWRITE, &nvs) != ESP_OK)
        return false;
    const bool ok = nvs_set_str(nvs, KEY, text) == ESP_OK && nvs_commit(nvs) == ESP_OK;
    nvs_close(nvs);
    return ok;
}
