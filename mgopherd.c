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

#define sendinfo(OUT, INFO, DETAIL)					\
	send_info_error((OUT), (IT_INFO), (INFO), (DETAIL))
#define senderror(OUT, ERROR, DETAIL)					\
	send_info_error((OUT), (IT_ERROR), (ERROR), (DETAIL))


struct item {
	char type;
	const char *display;
	const char *selector;
	const char *host;
	const char *port;
};

static void writemenu(struct opt_options *options, const char *selector,
    FILE *out);
static char itemtype(const char *path);
static void writefile(const char *path, FILE *out);
static int entryselect(const struct dirent *entry);
static bool checkrights(const char *path, char type);
static void senditem(FILE *out, struct item *it);
static void send_info_error(FILE *out, char type, const char *info,
    const char *detail);
static void sendeom(FILE *out);

int
main(int argc, char **argv)
{
	struct opt_options *options;

	options = opt_parse(argc, argv);
	assert(options != NULL);

	char request[LINE_MAX];
	if (fgets(request, sizeof(request), stdin) == NULL) {
		if (ferror(stdin)) {
			sendinfo(stdout, "I: I have a problem reading your "
			    "request.", NULL);
			senderror(stdout, "E: fgets", strerror(errno));
			sendeom(stdout);
			free(options);
			exit(EXIT_FAILURE);
		}
	}
	tool_stripcrlf(request);

	if (((*request != '/') && (*request != '\0')) ||
	    ((*request == '/') && (*(request+1) == '.'))) {
		sendinfo(stdout, "I: Your request seems to be invalid.", NULL);
		senderror(stdout, "E: request", request);
		sendeom(stdout);
		opt_free(options);
		exit(EXIT_FAILURE);
	}

	if (*request == '\0') {
		request[0] = '/';
		request[1] = '\0';
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
		sendinfo(stdout, "I: You requested an invalid item.", NULL);
		senderror(stdout, "E: request", request);
		sendeom(stdout);
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
		sendinfo(stdout, "I: I have a problem scanning a directory",
		    dir);
		senderror(stdout, "E: scandir", strerror(errno));
		sendeom(stdout);
		free(dirents);
		exit(EXIT_FAILURE);
	}
	for (int i = 0; i < entries; i++) {
		char *item = dirents[i]->d_name;
		char *path = tool_joinpath(dir, item);
		char type = itemtype(path);
		char *sel = tool_joinpath(selector, item);
		if (!checkrights(path, type)) {
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

		senditem(out, &it);

		free(sel);
		free(path);
		free(dirents[i]);
	}
	sendeom(out);

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
			sendinfo(stdout, "I: I couldn't check access rights "
			    "for an item", path);
			senderror(stdout, "E: accesss", strerror(errno));
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
senditem(FILE *out, struct item *it)
{
	assert(out != NULL);
	assert(it != NULL);

	fprintf(out, "%c%s\t%s\t%s\t%s\r\n", it->type, it->display,
	    it->selector, it->host, it->port);
}

static void
send_info_error(FILE *out, char type, const char *info, const char *detail)
{
	assert(out != NULL);
	assert(info != NULL);

	char *display = malloc(LINE_MAX);
	if (display == NULL) {
		/* Explicitely do not use gopherized messages to avoid
		 * recursions. */
		fprintf(stderr, "malloc: %s\r\n", strerror(errno));
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

	senditem(out, &it);

	free(display);
}

static void
sendeom(FILE *out)
{
	assert(out != NULL);

	fputs(".\r\n", out);
}

static void
writefile(const char *path, FILE *out)
{
	assert(path != NULL);
	assert(out != NULL);

	FILE *in = fopen(path, "r");
	if (in == NULL) {
		sendinfo(out, "I: I could not open the requested item", path);
		senderror(out, "E: fopen", strerror(errno));
		sendeom(stdout);
		exit(EXIT_FAILURE);
	}

	void *block = malloc(BINBLOCK);
	if (block == NULL) {
		sendinfo(out, "I: I could not allocate memory.", NULL);
		senderror(out, "E: malloc", strerror(errno));
		sendeom(stdout);
		free(block);
		exit(EXIT_FAILURE);
	}

	size_t r;
	while ((r = fread(block, 1, BINBLOCK, in)) > 0) {
		size_t w = fwrite(block, 1, r, out);
		if (w < r) {
			sendinfo(out, "I: I have a problem writing your "
			    "requested item", path);
			senderror(out, "E: fwrite", strerror(errno));
			sendeom(stdout);
			free(block);
			exit(EXIT_FAILURE);
		}
		
		if (r < BINBLOCK) {
			if (ferror(in)) {
				sendinfo(out, "I: I have a problem reading "
				    "your requested item", path);
				senderror(out, "E: fread", strerror(errno));
				sendeom(stdout);
				free(block);
				exit(EXIT_FAILURE);
			}
			break;
		}
	}

	free(block);
}
