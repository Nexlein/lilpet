/* The look chosen in the PC app, kept in flash (NVS) across power cycles */
#ifndef LILPET_LOOK_STORE_H
#define LILPET_LOOK_STORE_H

#include "lilpet/lilpet.h"

#include <stdbool.h>

/* Prepares the flash storage; call once at startup */
void look_store_init(void);
/* false when no look was saved yet (or the saved one is unreadable) */
bool look_store_load(lp_look *look);
bool look_store_save(const lp_look *look);

#endif
