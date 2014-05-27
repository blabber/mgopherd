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
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <syslog.h>
#include <unistd.h>

#include "gophermap.h"
#include "itemtypes.h"
#include "options.h"
#include "send.h"
#include "tools.h"

#define GOPHERMAP	"gophermap"
#define REQUESTREGEX	"^(/|(/[^\\.][^/]*)*)$"
#define BINBLOCK	1024

struct context {
	const char *selector;
	const char *path;
	FILE *out;
};

static void handle_directory(struct opt_options *options,
    struct context *context);
static void write_menu(struct opt_options *options, struct context *context);
static void write_gophermap(struct opt_options *options,
    struct context *context, const char *map);
static char itemtype(const char *path, FILE *out);
static void write_binary_file(struct context *context);
static void write_text_file(struct context *context);
static int entry_select(const struct dirent *entry);
static bool check_rights(const char *path, char type, FILE *out);
static bool check_request(const char *request, FILE *out);

int
main(int argc, char **argv)
{
	openlog("mgopherd", LOG_PID, LOG_USER);

	struct opt_options *options;

	options = opt_parse(argc, argv);
	assert(options != NULL);

	char *request = malloc(LINE_MAX);
	if (request == NULL) {
		syslog(LOG_ERR, "malloc error: %m");
		send_error(stdout, "E: malloc", strerror(errno));
		send_info(stdout, "I: I could not allocate memory.", NULL);
		send_eom(stdout);
		exit(EXIT_FAILURE);
	}

	if (fgets(request, LINE_MAX, stdin) == NULL) {
		if (ferror(stdin)) {
			syslog(LOG_ERR, "fgets error: %m");
			send_error(stdout, "E: fgets", strerror(errno));
			send_info(stdout, "I: I have a problem reading your "
			    "request.", NULL);
			send_eom(stdout);
			exit(EXIT_FAILURE);
		}
	}
	tool_strip_crlf(request);

	if (!check_request(request, stdout)) {
		syslog(LOG_NOTICE, "invalid request: \"%s\"", request);
		send_error(stdout, "E: request", request);
		send_info(stdout, "I: Your request seems to be invalid.", NULL);
		send_eom(stdout);
		exit(EXIT_FAILURE);
	}

	if (*request == '\0') {
		request[0] = '/';
		request[1] = '\0';
	}
	syslog(LOG_INFO, "selector: \"%s\"", request);

	char *path = tool_join_path(opt_get_root(options), request, stdout);
	syslog(LOG_DEBUG, "path: \"%s\"", path);

	struct context context = {
		.selector = request,
		.path = path,
		.out = stdout
	};

	switch (itemtype(context.path, context.out)){
	case IT_FILE:
		syslog(LOG_DEBUG, "serving text file");
		write_text_file(&context);
		break;
	case IT_ARCHIVE:
	case IT_BINARY:
	case IT_GIF:
	case IT_HTML:
	case IT_IMAGE:
	case IT_AUDIO:
		syslog(LOG_DEBUG, "serving binary file");
		write_binary_file(&context);
		break;
	case IT_DIR:
		syslog(LOG_DEBUG, "serving directory");
		handle_directory(options, &context);
		break;
	case IT_IGNORE:
	default:
		syslog(LOG_NOTICE, "invalid item: \"%s\"", context.path);
		send_error(context.out, "E: request", request);
		send_info(context.out, "I: You requested an invalid item.",
		    NULL);
		send_eom(context.out);
		exit(EXIT_FAILURE);
	}

	free(path);
	free(request);
	opt_free(options);

	closelog();
}

static bool
check_request(const char *request, FILE *out)
{
	assert(request != NULL);
	assert(out != NULL);

	bool isvalid = true;
	regex_t re;

	int ret = regcomp(&re, REQUESTREGEX, REG_EXTENDED | REG_NOSUB);
	if (ret != 0) {
		char errbuf[128];
		regerror(ret, &re, errbuf, sizeof(errbuf));
		syslog(LOG_ERR, "regcomp: %s", errbuf);
		send_error(out, "E: regcomp", errbuf);
		send_info(out, "I: I could not compile a regular expression.",
		    NULL);
		send_eom(out);
		exit(EXIT_FAILURE);
	}

	ret = regexec(&re, request, 0, NULL, 0);
	if (ret != 0) {
		if (ret != REG_NOMATCH) {
			char errbuf[128];
			regerror(ret, &re, errbuf, sizeof(errbuf));
			syslog(LOG_ERR, "regexec: %s", errbuf);
			send_error(out, "E: regcomp", errbuf);
			send_info(out, "I: I could not compile a regular "
			    "expression.", NULL);
			send_eom(out);
			exit(EXIT_FAILURE);
		}

		isvalid = false;
	}

	regfree(&re);

	return (isvalid);
}

static void
handle_directory(struct opt_options *options, struct context *context)
{
	assert(options != NULL);
	assert(context != NULL);

	char *map = tool_join_path(context->path, GOPHERMAP, context->out);

	if (check_rights(map, IT_FILE, context->out))
		write_gophermap(options, context, map);
	else
		write_menu(options, context);

	free(map);
}

static void
write_menu(struct opt_options *options, struct context *context)
{
	assert(options != NULL);
	assert(context != NULL);

	struct dirent **dirents;
	int entries = scandir(context->path, &dirents, &entry_select,
	    &alphasort);
	if (entries == -1) {
		syslog(LOG_ERR, "scandir error: %m");
		send_error(context->out, "E: scandir", strerror(errno));
		send_info(context->out, "I: I have a problem scanning a "
		    "directory.", context->path);
		send_eom(context->out);
		exit(EXIT_FAILURE);
	}
	for (int i = 0; i < entries; i++) {
		char *item = dirents[i]->d_name;
		char *path = tool_join_path(context->path, item, context->out);
		char type = itemtype(path, context->out);
		char *sel = tool_join_path(context->selector, item,
		    context->out);

		if (!check_rights(path, type, context->out)) {
			syslog(LOG_DEBUG, "missing rights: \"%s\"", path);
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

		send_item(context->out, &it);

		free(sel);
		free(path);
		free(dirents[i]);
	}
	send_eom(context->out);

	free(dirents);
}

static bool
check_rights(const char *path, char type, FILE *out)
{
	assert(path != NULL);
	assert(out != NULL);

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
		if (errno != EACCES && errno != ENOENT) {
			syslog(LOG_ERR, "access error: %m");
			send_error(out, "E: accesss", strerror(errno));
			send_info(out, "I: I couldn't check access rights "
			    "for an item.", path);
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
		syslog(LOG_ERR, "lstat error: %m");
		send_error(out, "E: lstat", strerror(errno));
		send_info(out, "I: I could not get file status.", path);
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
write_binary_file(struct context *context)
{
	assert(context != NULL);

	FILE *in = fopen(context->path, "r");
	if (in == NULL) {
		syslog(LOG_ERR, "fopen error: %m");
		send_error(context->out, "E: fopen", strerror(errno));
		send_info(context->out, "I: I could not open the requested "
		    "item.", context->path);
		send_eom(context->out);
		exit(EXIT_FAILURE);
	}

	void *block = malloc(BINBLOCK);
	if (block == NULL) {
		syslog(LOG_ERR, "malloc error: %m");
		send_error(context->out, "E: malloc", strerror(errno));
		send_info(context->out, "I: I could not allocate memory.",
		    NULL);
		send_eom(context->out);
		exit(EXIT_FAILURE);
	}

	size_t r;
	while ((r = fread(block, 1, BINBLOCK, in)) > 0) {
		size_t w = fwrite(block, 1, r, context->out);
		if (w < r) {
			syslog(LOG_ERR, "fwrite error: %m");
			send_error(context->out, "E: fwrite", strerror(errno));
			send_info(context->out, "I: I have a problem writing "
			    "your requested item.", context->path);
			send_eom(context->out);

			free(block);
			exit(EXIT_FAILURE);
		}
		
		if (r < BINBLOCK) {
			if (ferror(in)) {
				syslog(LOG_ERR, "fread error: %m");
				send_error(context->out, "E: fread",
				    strerror(errno));
				send_info(context->out, "I: I have a problem "
				    "reading your requested item.",
				    context->path);
				send_eom(context->out);
				exit(EXIT_FAILURE);
			}
			break;
		}
	}

	free(block);
}

static void
write_text_file(struct context *context)
{
	assert(context != NULL);

	FILE *in = fopen(context->path, "r");
	if (in == NULL) {
		syslog(LOG_ERR, "fopen error: %m");
		send_error(context->out, "E: fopen", strerror(errno));
		send_info(context->out, "I: I could not open the requested "
		    "item.", context->path);
		send_eom(context->out);
		exit(EXIT_FAILURE);
	}

	void *line = malloc(LINE_MAX);
	if (line == NULL) {
		syslog(LOG_ERR, "malloc error: %m");
		send_error(context->out, "E: malloc", strerror(errno));
		send_info(context->out, "I: I could not allocate memory.",
		    NULL);
		send_eom(context->out);
		exit(EXIT_FAILURE);
	}

	while (fgets(line, LINE_MAX, in) != NULL) {
		tool_strip_crlf(line);
		send_line(context->out, line);
	}
	if (ferror(stdin)) {
		syslog(LOG_ERR, "fgets error: %m");
		send_error(context->out, "E: fgets", strerror(errno));
		send_info(context->out, "I: I have a problem reading a "
		    "requested text file.", context->path);
		send_eom(context->out);
		exit(EXIT_FAILURE);
	}
	send_eom(context->out);

	free(line);
}

static void
write_gophermap(struct opt_options *options, struct context *context,
    const char *map)
{
	assert(options != NULL);
	assert(context != NULL);

	FILE *in = fopen(map, "r");
	if (in == NULL) {
		syslog(LOG_ERR, "fopen error: %m");
		send_error(context->out, "E: fopen", strerror(errno));
		send_info(context->out, "I: I could not open a gophermap.",
		    map);
		send_eom(context->out);
		exit(EXIT_FAILURE);
	}

	void *line = malloc(LINE_MAX);
	if (line == NULL) {
		syslog(LOG_ERR, "malloc error: %m");
		send_error(context->out, "E: malloc", strerror(errno));
		send_info(context->out, "I: I could not allocate memory.",
		    NULL);
		send_eom(context->out);
		exit(EXIT_FAILURE);
	}

	while (fgets(line, LINE_MAX, in) != NULL) {
		tool_strip_crlf(line);
		if (strchr(line, '\t') != NULL) {
			struct item item;
			if (!gophermap_parse_item(options, &item,
			    context->selector, line, context->out)) {
				send_info(context->out, "I: I encountered a "
				    "problem parsing a gophermap.", map);
				continue;
			}
			send_item(context->out, &item);
			gophermap_free_item(&item);
		} else
			send_info(context->out, line, NULL);
	}
	if (ferror(stdin)) {
		syslog(LOG_ERR, "fgets error: %m");
		send_error(context->out, "E: fgets", strerror(errno));
		send_info(context->out, "I: I have a problem reading a "
		    "gophermap.", map);
		send_eom(context->out);
		exit(EXIT_FAILURE);
	}
	send_eom(context->out);

	free(line);
}
