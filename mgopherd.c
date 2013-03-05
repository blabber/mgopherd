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

#include "itemtypes.h"
#include "options.h"
#include "send.h"
#include "tools.h"

#define BINBLOCK	1024

static void write_menu(struct opt_options *options, const char *selector,
    FILE *out);
static char itemtype(const char *path, FILE *out);
static void write_file(const char *path, FILE *out);
static int entry_select(const struct dirent *entry);
static bool check_rights(const char *path, char type, FILE *out);

int
main(int argc, char **argv)
{
	struct opt_options *options;

	options = opt_parse(argc, argv);
	assert(options != NULL);

	char request[LINE_MAX];
	if (fgets(request, sizeof(request), stdin) == NULL) {
		if (ferror(stdin)) {
			send_error(stdout, "E: fgets", strerror(errno));
			send_info(stdout, "I: I have a problem reading your "
			    "request.", NULL);
			send_eom(stdout);
			exit(EXIT_FAILURE);
		}
	}
	tool_strip_crlf(request);

	if (((*request != '/') && (*request != '\0')) ||
	    ((*request == '/') && (*(request+1) == '.'))) {
		send_error(stdout, "E: request", request);
		send_info(stdout, "I: Your request seems to be invalid.", NULL);
		send_eom(stdout);
		exit(EXIT_FAILURE);
	}

	if (*request == '\0') {
		request[0] = '/';
		request[1] = '\0';
	}

	char *path = tool_join_path(opt_get_root(options), request, stdout);

	switch (itemtype(path, stdout)){
	case IT_FILE:
	case IT_ARCHIVE:
	case IT_BINARY:
	case IT_GIF:
	case IT_HTML:
	case IT_IMAGE:
	case IT_AUDIO:
		write_file(path, stdout);
		break;
	case IT_DIR:
		write_menu(options, request, stdout);
		break;
	case IT_IGNORE:
	default:
		send_error(stdout, "E: request", request);
		send_info(stdout, "I: You requested an invalid item.", NULL);
		send_eom(stdout);
		exit(EXIT_FAILURE);
	}

	free(path);
	opt_free(options);
}

static void
write_menu(struct opt_options *options, const char *selector, FILE *out)
{
	assert(options != NULL);
	assert(selector != NULL);
	assert(out != NULL);

	char *dir = tool_join_path(opt_get_root(options), selector, out);

	struct dirent **dirents;
	int entries = scandir(dir, &dirents, &entry_select, &alphasort);
	if (entries == -1) {
		send_error(stdout, "E: scandir", strerror(errno));
		send_info(stdout, "I: I have a problem scanning a directory",
		    dir);
		send_eom(stdout);
		exit(EXIT_FAILURE);
	}
	for (int i = 0; i < entries; i++) {
		char *item = dirents[i]->d_name;
		char *path = tool_join_path(dir, item, out);
		char type = itemtype(path, out);
		char *sel = tool_join_path(selector, item, out);

		if (!check_rights(path, type, out)) {
			free(sel);
			free(path);
			free(dirents[i]);
			continue;
		}

		struct item it = {
			.type = type,
			.display = item,
			.selector = sel,
			.host = opt_get_host(options),
			.port = opt_get_port(options)
		};

		send_item(out, &it);

		free(sel);
		free(path);
		free(dirents[i]);
	}
	send_eom(out);

	free(dirents);
	free(dir);
}

static bool
check_rights(const char *path, char type, FILE *out)
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
			send_error(out, "E: accesss", strerror(errno));
			send_info(out, "I: I couldn't check access rights "
			    "for an item", path);
		}
		return (false);
	}

	return (true);
}

static int
entry_select(const struct dirent *entry)
{
	assert(entry != NULL);

	if (entry->d_name[0] == '.')
		return (0);

	return (1);
}

static char
itemtype(const char *path, FILE *out)
{
	assert(path != NULL);

	struct stat s;
	if (lstat(path, &s) == -1) {
		send_error(out, "E: lstat", strerror(errno));
		send_info(out, "I: I could not get file status", path);
		return (IT_IGNORE);
	}

	char it;
	if (S_ISREG(s.st_mode)) {
		char *mime = tool_mimetype(path, out);

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
write_file(const char *path, FILE *out)
{
	assert(path != NULL);
	assert(out != NULL);

	FILE *in = fopen(path, "r");
	if (in == NULL) {
		send_error(out, "E: fopen", strerror(errno));
		send_info(out, "I: I could not open the requested item", path);
		send_eom(stdout);
		exit(EXIT_FAILURE);
	}

	void *block = malloc(BINBLOCK);
	if (block == NULL) {
		send_error(out, "E: malloc", strerror(errno));
		send_info(out, "I: I could not allocate memory.", NULL);
		send_eom(stdout);
		exit(EXIT_FAILURE);
	}

	size_t r;
	while ((r = fread(block, 1, BINBLOCK, in)) > 0) {
		size_t w = fwrite(block, 1, r, out);
		if (w < r) {
			send_error(out, "E: fwrite", strerror(errno));
			send_info(out, "I: I have a problem writing your "
			    "requested item", path);
			send_eom(stdout);

			free(block);
			exit(EXIT_FAILURE);
		}
		
		if (r < BINBLOCK) {
			if (ferror(in)) {
				send_error(out, "E: fread", strerror(errno));
				send_info(out, "I: I have a problem reading "
				    "your requested item", path);
				send_eom(stdout);
				exit(EXIT_FAILURE);
			}
			break;
		}
	}

	free(block);
}
