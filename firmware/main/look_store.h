/* The look chosen in the PC app, kept in flash (NVS) across power cycles */
#ifndef LILPET_LOOK_STORE_H
#define LILPET_LOOK_STORE_H

#include "lilpet/lilpet.h"

#include <stdbool.h>

/* Prepares the flash storage; call once at startup. Tells what it found, for hello: "ok", "erased" when it had to wipe
 * a storage that was full or in another format (the saved look with it), "none" when it cannot be used. */
const char *look_store_init(void);
/* false when no look was saved yet, or when the saved one is unreadable, which it logs */
bool look_store_load(lp_look *look);
bool look_store_save(const lp_look *look);

#endif
