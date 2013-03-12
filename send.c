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
send_line(FILE *out, const char *line)
{
	assert(out != NULL);
	assert(line != NULL);

	/*
	 * RFC 1436 states the following note for Textfile Entities:
	 *
	 *     Lines beginning with periods must be prepended with an extra
	 *     period to ensure that the transmission is not terminated early.
	 *     The client should strip extra periods at the beginning of the
	 *     line.
	 *
	 * Nonetheless every tested client (gopher, lynx, OverbiteFF) is not
	 * stripping leading periods. That is the reason the following lines are
	 * commented out. If you want a strict RFC compliant server, feel free
	 * to uncomment these lines.
	 */
	// if (*line == '.')
	// 	fputc('.', out);

	fprintf(out, "%s\r\n", line);
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

	char *selector = malloc(sizeof(FAKESELECTOR));
	if (display == NULL) {
		/* Explicitely do not use gopherized messages to avoid
		 * recursions. */
		fprintf(stderr, "malloc: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	strncpy(selector, FAKESELECTOR, sizeof(FAKESELECTOR));

	char *host = malloc(sizeof(FAKEHOST));
	if (display == NULL) {
		/* Explicitely do not use gopherized messages to avoid
		 * recursions. */
		fprintf(stderr, "malloc: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	strncpy(host, FAKEHOST, sizeof(FAKEHOST));

	char *port = malloc(sizeof(FAKEPORT));
	if (display == NULL) {
		/* Explicitely do not use gopherized messages to avoid
		 * recursions. */
		fprintf(stderr, "malloc: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	strncpy(selector, FAKEPORT, sizeof(FAKEPORT));

	struct item it = {
		.type = type,
		.display = display,
		.selector = selector,
		.host = host,
		.port = port
	};

	send_item(out, &it);

	free(port);
	free(host);
	free(selector);
	free(display);
}
