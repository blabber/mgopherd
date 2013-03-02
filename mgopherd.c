/*-
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <tobias.rehbein@web.de> wrote this file. As long as you retain this notice
 * you can do whatever you want with this stuff. If we meet some day, and you
 * think this stuff is worth it, you can buy me a beer in return.
 */

#define _POSIX_C_SOURCE 200809

#include <sys/stat.h>

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include "options.h"
#include "tools.h"

#define FAKEHOST	"fake"
#define FAKEPORT	"1"

#define BINBLOCK	1024

#define IT_IGNORE	'?'
#define IT_FILE		'0'
#define IT_DIR		'1'
#define IT_ERROR	'3'
#define IT_ARCHIVE	'5'
#define IT_BINARY	'9'
#define IT_GIF		'g'
#define IT_HTML		'h'
#define IT_INFO		'i'
#define IT_IMAGE	'I'
#define IT_AUDIO	's'

#define W_FAKEITEM(OUT, FMT, TYPE, ...)					\
    fprintf(OUT, "%c" FMT "\t\t" FAKEHOST "\t" FAKEPORT "\r\n", TYPE,	\
    __VA_ARGS__);
#define W_ERR(OUT,FMT, ...)	W_FAKEITEM(OUT, FMT, IT_ERROR, __VA_ARGS__)
#define W_INFO(OUT,FMT, ...)	W_FAKEITEM(OUT, FMT, IT_INFO, __VA_ARGS__)
#define W_END(OUT)	fputs(".\r\n", OUT);

static void writemenu(struct opt_options *options, const char *selector,
    FILE *out);
static char itemtype(const char *path);
static void writefile(const char *path, FILE *out);
static int entryselect(const struct dirent *entry);
static bool checkrights(const char *path, char type);

int
main(int argc, char **argv)
{
	struct opt_options *options;

	options = opt_parse(argc, argv);
	assert(options != NULL);

	char request[LINE_MAX];
	if (fgets(request, sizeof(request), stdin) == NULL) {
		if (ferror(stdin)) {
			W_INFO(stdout, "I: %s", "I have a problem reading your "
			    "request.");
			W_ERR(stdout, "E: fgets: %s", strerror(errno));
			W_END(stdout);
			free(options);
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

	if (((*request != '/') && (*request != '\0')) ||
	    ((*request == '/') && (*(request+1) == '.'))) {
		W_INFO(stdout, "I: %s", "Your request seems to be invalid.");
		W_ERR(stdout, "E: request: %s", request);
		W_END(stdout);
		opt_free(options);
		exit(EXIT_FAILURE);
	}

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
	case IT_IGNORE:
	default:
		W_INFO(stdout, "I: %s", "You requested an invalid item.");
		W_ERR(stdout, "E: request: %s", request);
		W_END(stdout);
		free(path);
		opt_free(options);
		exit(EXIT_FAILURE);
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

	struct dirent **dirents;
	int entries = scandir(dir, &dirents, &entryselect, &alphasort);
	if (entries == -1) {
		W_INFO(stdout, "I: I have a problem scanning a directory (%s).",
		    dir);
		W_ERR(stdout, "E: scandir: %s", strerror(errno));
		W_END(stdout);
		free(dirents);
		exit(EXIT_FAILURE);
	}
	for (int i = 0; i < entries; i++) {
		char *item = dirents[i]->d_name;
		char *path = tool_joinpath(dir, item);
		char type = itemtype(path);
		if (!checkrights(path, type)) {
			free(path);
			free(dirents[i]);
			continue;
		}

		char *sel = tool_joinpath(selector, item);
		char *host = opt_get_host(options);
		char *port = opt_get_port(options);

		const char *delimeter = "";
		if (selector[0] != '/')
			delimeter = "/";

		fprintf(out, "%c%s\t%s%s\t%s\t%s\r\n", type, item, delimeter,
		    sel, host, port);

		free(sel);
		free(path);
		free(dirents[i]);
	}
	fputs(".\r\n", out);

	free(dirents);
	free(dir);
}

static bool
checkrights(const char *path, char type)
{
	assert(path != NULL);

	int mode;
	switch (type) {
	case IT_FILE:
	case IT_ARCHIVE:
	case IT_BINARY:
	case IT_GIF:
	case IT_HTML:
	case IT_IMAGE:
	case IT_AUDIO:
		mode = R_OK;
		break;
	case IT_DIR:
		mode = R_OK | X_OK;
		break;
	case IT_IGNORE:
	default:
		return (false);
	}

	if (access(path, mode) == -1) {
		if (errno != EACCES) {
			W_INFO(stdout, "I: I couldn't check access rights for "
			    "an item (%s).", path);
			W_ERR(stdout, "E: accesss: %s", strerror(errno));
			W_END(stdout);
		}
		return (false);
	}

	return (true);
}

static int
entryselect(const struct dirent *entry)
{
	assert(entry != NULL);

	if (entry->d_name[0] == '.')
		return (0);

	return (1);
}

static char
itemtype(const char *path)
{
	assert(path != NULL);

	struct stat s;
	if (lstat(path, &s) == -1) {
		fprintf(stderr, "stat %s: %s\n", path, strerror(errno));
		return (IT_IGNORE);
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
		it = IT_IGNORE;;

	return (it);
}

static void
writefile(const char *path, FILE *out)
{
	assert(path != NULL);
	assert(out != NULL);

	FILE *in = fopen(path, "r");
	if (in == NULL) {
		W_INFO(out, "I: I could not open the requested item (%s).",
		    path);
		W_ERR(out, "E: fopen: %s", strerror(errno));
		W_END(stdout);
		exit(EXIT_FAILURE);
	}

	void *block = malloc(BINBLOCK);
	if (block == NULL) {
		W_INFO(out, "I: %s", "I could not allocate memory.");
		W_ERR(out, "E: malloc: %s", strerror(errno));
		W_END(stdout);
		free(block);
		exit(EXIT_FAILURE);
	}

	size_t r;
	while ((r = fread(block, 1, BINBLOCK, in)) > 0) {
		size_t w = fwrite(block, 1, r, out);
		if (w < r) {
			W_INFO(out, "I: I have a problem writing your "
			    "requested item (%s).", path);
			W_ERR(out, "E: fwrite: %s", strerror(errno));
			W_END(stdout);
			free(block);
			exit(EXIT_FAILURE);
		}
		
		if (r < BINBLOCK) {
			if (ferror(in)) {
				W_INFO(out, "I: I have a problem reading your "
				    "requested item (%s).", path);
				W_ERR(out, "E: fread: %s", strerror(errno));
				W_END(stdout);
				free(block);
				exit(EXIT_FAILURE);
			}
			break;
		}
	}

	free(block);
}
