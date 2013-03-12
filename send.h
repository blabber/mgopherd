/*-
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <tobias.rehbein@web.de> wrote this file. As long as you retain this notice
 * you can do whatever you want with this stuff. If we meet some day, and you
 * think this stuff is worth it, you can buy me a beer in return.
 */

#ifndef SEND_H
#define SEND_H

#define FAKEHOST	"fake"
#define FAKEPORT	"1"
#define FAKESELECTOR	"/"

struct item {
	char type;
	char *display;
	char *selector;
	char *host;
	char *port;
};

void send_item(FILE *_out, struct item *_it);
void send_error(FILE *_out, const char *_error, const char *_detail);
void send_info(FILE *_out, const char *_info, const char *_detail);
void send_line(FILE *_out, const char *_line);
void send_eom(FILE *_out);

#endif /* !SEND_H */
