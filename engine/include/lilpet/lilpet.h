/*
 * lilpet eye engine, shared by the ESP32-S3 firmware and the WebAssembly build (see notes/architecture.md).
 * Portable C17: no allocation, no global state, float only (doubles are emulated in software on the ESP32-S3).
 */
#ifndef LILPET_LILPET_H
#define LILPET_LILPET_H

#ifdef __cplusplus
extern "C" {
#endif

/* Bumped when the public API or the rendering changes */
#define LP_VERSION_MAJOR 0
#define LP_VERSION_MINOR 1
#define LP_VERSION_PATCH 0

/* Engine version as "MAJOR.MINOR.PATCH" */
const char *lp_version(void);

#ifdef __cplusplus
}
#endif

#endif /* LILPET_LILPET_H */
