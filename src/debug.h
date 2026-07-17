#ifndef DEBUG_H
#define DEBUG_H


#include "buffer.h"

#include <stdbool.h>

/*
 * Dumps a raw ESC/POS job to disk when debug mode is enabled.
 *
 * The job is written to:
 *
 *     /var/log/tmbridge/job-YYYYMMDD-HHMMSS.escpos
 *
 * Any errors are logged but are otherwise ignored.
 */
bool debug_dump_escpos(const buffer_t *escpos);

#endif
