/*
 * Frame timing of the displays without a TE signal: the AtomS3R's small LCD, or no display at all on a bare board.
 * Built for every board: one whose board_config.h gives BOARD_LCD_TE waits for that signal instead (display_co5300.c).
 */
#include "board_config.h"
#include "display.h"

#ifndef BOARD_LCD_TE
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define FRAME_MS 16 /* 62.5 frames per second: ticks of 1 ms cannot do 60 */

void display_wait_frame(void)
{
    static TickType_t wake;
    if (!wake)
        wake = xTaskGetTickCount();
    vTaskDelayUntil(&wake, pdMS_TO_TICKS(FRAME_MS));
}
#endif
