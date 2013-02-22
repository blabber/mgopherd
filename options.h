/*-
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <tobias.rehbein@web.de> wrote this file. As long as you retain this notice
 * you can do whatever you want with this stuff. If we meet some day, and you
 * think this stuff is worth it, you can buy me a beer in return.
 */

#ifndef OPTIONS_H
#define OPTIONS_H

struct opt_options;

struct opt_options *opt_parse(int _argc, char **_argv);
void opt_free(struct opt_options *_options);

char *opt_get_host(struct opt_options *_options);
char *opt_get_root(struct opt_options *_options);
char *opt_get_port(struct opt_options *_options);

#endif /* !OPTIONS_H */
