/*-
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <tobias.rehbein@web.de> wrote this file. As long as you retain this notice
 * you can do whatever you want with this stuff. If we meet some day, and you
 * think this stuff is worth it, you can buy me a beer in return.
 */

#ifndef TOOLS_H
#define TOOLS_H

#include <stdio.h>

char *tool_mimetype(const char *_path, FILE *_out);
char *tool_join_path(const char *_part1, const char *_part2, FILE *_out);
void tool_strip_crlf(char *_line);

#endif /* !TOOLS_H */
