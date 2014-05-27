/*-
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <tobias.rehbein@web.de> wrote this file. As long as you retain this notice
 * you can do whatever you want with this stuff. If we meet some day, and you
 * think this stuff is worth it, you can buy me a beer in return.
 */

#ifndef GOPHERMAP_H
#define GOPHERMAP_H

#include <stdbool.h>

#include "send.h"
#include "options.h"

bool gophermap_parse_item(struct opt_options *_options,
    struct item *_item, const char *_selector, const char *_line, FILE *_out);
void gophermap_free_item(struct item *_item);

#endif /* !GOPHERMAP_H */
