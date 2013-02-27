/*-
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <tobias.rehbein@web.de> wrote this file. As long as you retain this notice
 * you can do whatever you want with this stuff. If we meet some day, and you
 * think this stuff is worth it, you can buy me a beer in return.
 */

#define _POSIX_C_SOURCE 200809

#include <sys/types.h>

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <magic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tools.h"

#define INITIALCAPACITY	32

static int itemcmp(const void *item1, const void *item2);

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

char **
tool_getitems(const char *path)
{
	assert(path != NULL);

	DIR *dh = opendir(path);
	if (dh == NULL) {
		fprintf(stderr, "opendir %s: %s\n", path, strerror(errno));
		exit(EXIT_FAILURE);
	}

	size_t capacity = INITIALCAPACITY;
	char **items = malloc(capacity * sizeof(char *));
	if (items == NULL) {
		fprintf(stderr, "malloc items: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	size_t item = 0;
	struct dirent *de;
	while ((de = readdir(dh)) != NULL) {
		if (item == capacity) {
			capacity *= 2;
			items = realloc(items, capacity * sizeof(char *));
			if (items == NULL) {
				fprintf(stderr, "realloc items: %s\n",
				    strerror(errno));
				exit(EXIT_FAILURE);
			}
		}

		items[item] = strdup(de->d_name);
		if (items[item] == NULL) {
			fprintf(stderr, "strdup item: %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
		item++;
	}
	items[item] = NULL;

	closedir(dh);

	qsort(items, item, sizeof(char *), &itemcmp);

	return (items);
}

void
tool_freeitems(char **items)
{
	assert(items != NULL);

	for (char **item = items; *item != NULL; item++)
		free(*item);

	free(items);
}

static int
itemcmp(const void *item1, const void *item2)
{
	assert(item1 != NULL);
	assert(item2 != NULL);

	const char *c1 = *(const char * const *)item1;
	const char *c2 = *(const char * const *)item2;

	return strcmp(c1, c2);
}
