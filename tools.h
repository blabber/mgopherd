/*-
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <tobias.rehbein@web.de> wrote this file. As long as you retain this notice
 * you can do whatever you want with this stuff. If we meet some day, and you
 * think this stuff is worth it, you can buy me a beer in return.
 */

#ifndef TOOLS_H
#define TOOLS_H

char *tool_mimetype(const char *path);
char *tool_join_path(const char *part1, const char *part2);
void tool_strip_crlf(char *line);

#endif /* !TOOLS_H */
