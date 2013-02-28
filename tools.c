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
#include <magic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tools.h"

#define INITIALCAPACITY	32

char *
tool_mimetype(const char *path)
{
	assert(path != NULL);

	magic_t mh = magic_open(MAGIC_MIME_TYPE);
	if (mh == NULL) {
		fprintf(stderr, "magic_open: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (magic_load(mh, NULL) == -1) {
		fprintf(stderr, "magic_load: %s\n", magic_error(mh));
		exit(EXIT_FAILURE);
	}

	const char *mime = magic_file(mh, path);
	if (mime == NULL) {
		fprintf(stderr, "magic_file %s: %s\n", path, magic_error(mh));
		exit(EXIT_FAILURE);
	}

	char *ret = strdup(mime);
	if (ret == NULL)
	{
		fprintf(stderr, "strdup mime: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	magic_close(mh);

	return (ret);
}

char *
tool_joinpath(const char *part1, const char *part2)
{
	assert(part1 != NULL);
	assert(part2 != NULL);

	char *joined = malloc(PATH_MAX);
	if (joined == NULL) {
		fprintf(stderr, "malloc joined: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	char *pj = stpncpy(joined, part1, PATH_MAX);
	if ((pj > joined) && (*(pj-1) != '/')) {
		*pj++ = '/';
		*pj = '\0';
	}
	
	size_t rl = PATH_MAX - (pj - joined);

	const char *p2 = part2;
	if (*p2 == '/')
		p2++;
	strncpy(pj, p2, rl);

	if (joined[PATH_MAX-1] != '\0') {
		fputs("joinpath: joined too long\n", stderr);
		exit(EXIT_FAILURE);
	}

	return (joined);
}
