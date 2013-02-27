/*-
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <tobias.rehbein@web.de> wrote this file. As long as you retain this notice
 * you can do whatever you want with this stuff. If we meet some day, and you
 * think this stuff is worth it, you can buy me a beer in return.
 */

#define _POSIX_C_SOURCE 200809

#include <sys/stat.h>

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "options.h"
#include "tools.h"

#define BINBLOCK	1024

#define IT_UNKNOWN	'?'
#define IT_FILE		'0'
#define IT_DIR		'1'
#define IT_ARCHIVE	'5'
#define IT_BINARY	'9'
#define IT_GIF		'g'
#define IT_HTML		'h'
#define IT_IMAGE	'I'
#define IT_AUDIO	's'

static void writemenu(struct opt_options *options, const char *selector,
    FILE *out);
static char itemtype(const char *path);
static void writefile(const char *path, FILE *out);

int
main(int argc, char **argv)
{
	struct opt_options *options;

	options = opt_parse(argc, argv);
	assert(options != NULL);

	char request[LINE_MAX];
	if (fgets(request, sizeof(request), stdin) == NULL) {
		if (ferror(stdin)) {
			fprintf(stderr, "fgets request: %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
	}
	char *p;
	if ((p = strstr(request, "\r\n")) != NULL)
		*p = '\0';
	else if ((p = strchr(request, '\n')) != NULL)
		/*
		 * requests are expected to be terminated by \r\n. Fall back to
		 * \n anyway...
		 */
		*p = '\0';

	char *path = tool_joinpath(opt_get_root(options), request);

	switch (itemtype(path)){
	case IT_FILE:
	case IT_ARCHIVE:
	case IT_BINARY:
	case IT_GIF:
	case IT_HTML:
	case IT_IMAGE:
	case IT_AUDIO:
		writefile(path, stdout);
		break;
	case IT_DIR:
		writemenu(options, request, stdout);
		break;
	case IT_UNKNOWN:
	default:
		fprintf(stderr, "unknown itemtype: %s\n", request);
	}

	free(path);
	opt_free(options);
}

static void
writemenu(struct opt_options *options, const char *selector, FILE *out)
{
	assert(options != NULL);
	assert(selector != NULL);
	assert(out != NULL);

	char *dir = tool_joinpath(opt_get_root(options), selector);
	char **items = tool_getitems(dir);

	for (char **item = items; *item != NULL; item++) {
		if (*item[0] == '.')
			continue;

		char *path = tool_joinpath(dir, *item);
		char type = itemtype(path);
		if (type == '?') {
			free(path);
			continue;
		}

		char *sel = tool_joinpath(selector, *item);
		char *host = opt_get_host(options);
		char *port = opt_get_port(options);

		fprintf(out, "%c%s\t%s\t%s\t%s\r\n", type, *item, sel, host,
		    port);

		free(sel);
		free(path);
	}
	fputs(".\r\n", out);

	tool_freeitems(items);
	free(dir);
}

static char
itemtype(const char *path)
{
	assert(path != NULL);

	struct stat s;
	if (stat(path, &s) == -1) {
		fprintf(stderr, "stat %s: %s\n", path, strerror(errno));
		return (IT_UNKNOWN);
	}

	char it;
	if (S_ISREG(s.st_mode)) {
		char *mime = tool_mimetype(path);
		assert(mime != NULL);

		if (strcmp(mime, "text/html") == 0)
			it = IT_HTML;
		else if (strncmp(mime, "text/", 5) == 0)
			it = IT_FILE;
		else if (strcmp(mime, "image/gif") == 0)
			it = IT_GIF;
		else if (strncmp(mime, "image/", 6) == 0)
			it = IT_IMAGE;
		else if ((strncmp(mime, "audio/", 6) == 0) ||
		    (strcmp(mime, "application/ogg") == 0))
			it = IT_AUDIO;
		else if ((strcmp(mime, "application/x-bzip2") == 0) ||
		    (strcmp(mime, "application/x-gzip") == 0) ||
		    (strcmp(mime, "application/zip") == 0))
			it = IT_ARCHIVE;
		else
			it = IT_BINARY;

		free(mime);
	} else if (S_ISDIR(s.st_mode))
		it = IT_DIR;
	else
		it = IT_UNKNOWN;;

	return (it);
}

static void
writefile(const char *path, FILE *out)
{
	assert(path != NULL);
	assert(out != NULL);

	FILE *in = fopen(path, "r");
	if (in == NULL) {
		fprintf(stderr, "fopen in: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	void *block = malloc(BINBLOCK);
	if (block == NULL) {
		fprintf(stderr, "malloc block: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	size_t read;
	while ((read = fread(block, 1, BINBLOCK, in)) > 0) {
		size_t written = fwrite(block, 1, read, out);
		if (written < read) {
			fprintf(stderr, "fwrite: %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
		
		if (read < BINBLOCK) {
			if (ferror(stdin)) {
				fprintf(stderr, "fgets request: %s\n",
				    strerror(errno));
				exit(EXIT_FAILURE);
			}
			break;
		}
	}

	free(block);
}
