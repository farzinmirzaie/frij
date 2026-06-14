#ifndef FRIJ_STORAGE_H
#define FRIJ_STORAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Storage — a board service. Reports the device's flash usage so Settings can
 * show a "12.3 MB free" readout. Emulator returns a believable mock; the device
 * reads the real flash chip size vs the installed firmware size.
 */

// Fill used/total in KB. Returns false if unknown (readout should show "—").
bool frij_storage_kb(uint32_t* used_kb, uint32_t* total_kb);

// Format the free space as "12.3 MB free" (or "—" when unknown) into buf.
void frij_storage_free_str(char* buf, size_t n);

#endif  // FRIJ_STORAGE_H
