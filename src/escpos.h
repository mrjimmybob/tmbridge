#ifndef ESCPOS_H
#define ESCPOS_H

#include <stdbool.h>
#include <stddef.h>

#include "buffer.h"

/*
 * Parse an ESC/POS print job and generate the corresponding
 * Epson ePOS-Print XML fragments.
 *
 * The generated XML does NOT include:
 *   - XML declaration
 *   - SOAP envelope
 *   - <epos-print> wrapper
 *
 * Example output:
 *
 *   <text align="left" em="false" ...>Hello&#10;</text>
 *   <cut type="feed"/>
 *
 * The caller passes the resulting buffer to epos_print().
 */

bool escpos_parse(const void *data, size_t length, buffer_t *xml);

#endif /* ESCPOS_H */
