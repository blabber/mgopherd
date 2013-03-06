/*-
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <tobias.rehbein@web.de> wrote this file. As long as you retain this notice
 * you can do whatever you want with this stuff. If we meet some day, and you
 * think this stuff is worth it, you can buy me a beer in return.
 */

#define _POSIX_C_SOURCE 200809

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "itemtypes.h"
#include "send.h"

static void send_fake_item(FILE *out, char type, const char *info,
    const char *detail);

void
send_item(FILE *out, struct item *it)
{
	assert(out != NULL);
	assert(it != NULL);

	fprintf(out, "%c%s\t%s\t%s\t%s\r\n", it->type, it->display,
	    it->selector, it->host, it->port);
}

void
send_error(FILE *out, const char *error, const char *detail)
{
	assert(out != NULL);
	assert(error != NULL);

	send_fake_item(out, IT_ERROR, error, detail);
}

void
send_info(FILE *out, const char *info, const char *detail)
{
	assert(out != NULL);
	assert(info != NULL);

	send_fake_item(out, IT_INFO, info, detail);
}

void
send_eom(FILE *out)
{
	assert(out != NULL);

	fputs(".\r\n", out);
}

static void
send_fake_item(FILE *out, char type, const char *info, const char *detail)
{
	assert(out != NULL);
	assert(info != NULL);

	char *display = malloc(LINE_MAX);
	if (display == NULL) {
		/* Explicitely do not use gopherized messages to avoid
		 * recursions. */
		fprintf(stderr, "malloc: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (detail == NULL)
		strncpy(display, info, LINE_MAX-1);
	else
		snprintf(display, LINE_MAX, "%s: %s", info, detail);

	struct item it = {
		.type = type,
		.display = display,
		.selector = "/",
		.host = FAKEHOST,
		.port = FAKEPORT
	};

	send_item(out, &it);

	free(display);
}
