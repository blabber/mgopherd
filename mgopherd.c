/*-
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <tobias.rehbein@web.de> wrote this file. As long as you retain this notice
 * you can do whatever you want with this stuff. If we meet some day, and you
 * think this stuff is worth it, you can buy me a beer in return.
 */

#define _POSIX_C_SOURCE 200809

#include <sys/types.h>
#include <sys/stat.h>

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "options.h"

#define IT_UNKNOWN	'?'
#define IT_FILE		'0'
#define IT_DIR		'1'

static void writemenu(struct opt_options *options, const char *selector,
    FILE *out);
static char *joinpath(const char *part1, const char *part2);
static char itemtype(const char *path);

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

	char *path = joinpath(opt_get_root(options), request);

	switch (itemtype(path)){
	case IT_FILE:
		fputs("\r\n== Not implemented yet. ==\r\n.\r", stdout);
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

	char *dir = joinpath(opt_get_root(options), selector);
	DIR *dh = opendir(dir);
	if (dh == NULL) {
		fprintf(stderr, "opendir %s: %s\n", dir, strerror(errno));
		exit(EXIT_FAILURE);
	}

	struct dirent *de;
	while ((de = readdir(dh)) != NULL) {
		char *item = de->d_name;
		if (item[0] == '.')
			continue;

		char *path = joinpath(dir, de->d_name);
		char type = itemtype(path);
		if (type == '?') {
			free(path);
			continue;
		}

		char *sel = joinpath(selector, item);
		char *host = opt_get_host(options);
		char *port = opt_get_port(options);

		fprintf(out, "%c%s\t%s\t%s\t%s\r\n", type, item, sel, host,
		    port);

		free(sel);
		free(path);
	}
	fputs(".\r", out);

	closedir(dh);
	free(dir);
}

static char *
joinpath(const char *part1, const char *part2)
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
	pj = stpncpy(pj, p2, rl);

	if (joined[PATH_MAX-1] != '\0') {
		fputs("joinpath: joined too long", stderr);
		exit(EXIT_FAILURE);
	}

	return (joined);
}

static char
itemtype(const char *path)
{
	assert(path != NULL);

	struct stat s;
	if (stat(path, &s) == -1)
		fprintf(stderr, "stat %s: %s\n", path, strerror(errno));

	char it;
	if (S_ISREG(s.st_mode))
		it = IT_FILE;
	else if (S_ISDIR(s.st_mode))
		it = IT_DIR;
	else
		it = IT_UNKNOWN;;

	return (it);
}
