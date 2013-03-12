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
#include <unistd.h>

#include "itemtypes.h"
#include "options.h"
#include "send.h"
#include "tools.h"

#define GOPHERMAP	"gophermap"
#define REQUESTREGEX	"^(/|(/[^\\.][^/]*)*)$"
#define BINBLOCK	1024

static void handle_directory(struct opt_options *options, const char *selector,
    FILE *out);
static void write_menu(struct opt_options *options, const char *selector,
    FILE *out);
static void write_gophermap(struct opt_options *options, const char *selector,
    FILE *out);
static char itemtype(const char *path, FILE *out);
static void write_binary_file(const char *path, FILE *out);
static void write_text_file(const char *path, FILE *out);
static int entry_select(const struct dirent *entry);
static bool check_rights(const char *path, char type, FILE *out);
static bool check_request(const char *request, FILE *out);
static bool parse_gophermap_item(struct opt_options *options, struct item *item,
    const char *selector, const char *line, FILE *out);
static void free_gophermap_item(struct item *item);

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

	if (!check_request(request, stdout)) {
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
		write_text_file(path, stdout);
		break;
	case IT_ARCHIVE:
	case IT_BINARY:
	case IT_GIF:
	case IT_HTML:
	case IT_IMAGE:
	case IT_AUDIO:
		write_binary_file(path, stdout);
		break;
	case IT_DIR:
		handle_directory(options, request, stdout);
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
handle_directory(struct opt_options *options, const char *selector, FILE *out)
{
	assert(options != NULL);
	assert(selector != NULL);
	assert(out != NULL);

	char *path = tool_join_path(opt_get_root(options), selector, out);
	char *map = tool_join_path(path, GOPHERMAP, out);

	if (check_rights(map, IT_FILE, out))
		write_gophermap(options, selector, out);
	else
		write_menu(options, selector, out);

	free(map);
	free(path);
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
		send_error(out, "E: scandir", strerror(errno));
		send_info(out, "I: I have a problem scanning a directory",
		    dir);
		send_eom(out);
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
write_binary_file(const char *path, FILE *out)
{
	assert(path != NULL);
	assert(out != NULL);

	FILE *in = fopen(path, "r");
	if (in == NULL) {
		send_error(out, "E: fopen", strerror(errno));
		send_info(out, "I: I could not open the requested item", path);
		send_eom(out);
		exit(EXIT_FAILURE);
	}

	void *block = malloc(BINBLOCK);
	if (block == NULL) {
		send_error(out, "E: malloc", strerror(errno));
		send_info(out, "I: I could not allocate memory.", NULL);
		send_eom(out);
		exit(EXIT_FAILURE);
	}

	size_t r;
	while ((r = fread(block, 1, BINBLOCK, in)) > 0) {
		size_t w = fwrite(block, 1, r, out);
		if (w < r) {
			send_error(out, "E: fwrite", strerror(errno));
			send_info(out, "I: I have a problem writing your "
			    "requested item", path);
			send_eom(out);

			free(block);
			exit(EXIT_FAILURE);
		}
		
		if (r < BINBLOCK) {
			if (ferror(in)) {
				send_error(out, "E: fread", strerror(errno));
				send_info(out, "I: I have a problem reading "
				    "your requested item", path);
				send_eom(out);
				exit(EXIT_FAILURE);
			}
			break;
		}
	}

	free(block);
}

static void
write_text_file(const char *path, FILE *out)
{
	assert(path != NULL);
	assert(out != NULL);

	FILE *in = fopen(path, "r");
	if (in == NULL) {
		send_error(out, "E: fopen", strerror(errno));
		send_info(out, "I: I could not open the requested item", path);
		send_eom(out);
		exit(EXIT_FAILURE);
	}

	void *line = malloc(LINE_MAX);
	if (line == NULL) {
		send_error(out, "E: malloc", strerror(errno));
		send_info(out, "I: I could not allocate memory.", NULL);
		send_eom(out);
		exit(EXIT_FAILURE);
	}

	while (fgets(line, LINE_MAX, in) != NULL) {
		tool_strip_crlf(line);
		send_line(out, line);
	}
	if (ferror(stdin)) {
		send_error(out, "E: fgets", strerror(errno));
		send_info(out, "I: I have a problem reading a requested text "
		    "file", path);
		send_eom(out);
		exit(EXIT_FAILURE);
	}
	send_eom(out);

	free(line);
}

static void
write_gophermap(struct opt_options *options, const char *selector, FILE *out)
{
	assert(options != NULL);
	assert(selector != NULL);
	assert(out != NULL);

	char *path = tool_join_path(opt_get_root(options), selector, out);
	char *map = tool_join_path(path, GOPHERMAP, out);

	FILE *in = fopen(map, "r");
	if (in == NULL) {
		send_error(out, "E: fopen", strerror(errno));
		send_info(out, "I: I could not open a gophermap", map);
		send_eom(out);
		exit(EXIT_FAILURE);
	}

	void *line = malloc(LINE_MAX);
	if (line == NULL) {
		send_error(out, "E: malloc", strerror(errno));
		send_info(out, "I: I could not allocate memory.", NULL);
		send_eom(out);
		exit(EXIT_FAILURE);
	}

	while (fgets(line, LINE_MAX, in) != NULL) {
		tool_strip_crlf(line);
		if (strchr(line, '\t') != NULL) {
			struct item item;
			if (!parse_gophermap_item(options, &item, selector,
			    line, out)) {
				send_info(out, "I: I encountered a problem "
				    "parsing a gophermap", map);
				continue;
			}
			send_item(out, &item);
			free_gophermap_item(&item);
		} else
			send_info(out, line, NULL);
	}
	if (ferror(stdin)) {
		send_error(out, "E: fgets", strerror(errno));
		send_info(out, "I: I have a problem reading a gophermap",
		    map);
		send_eom(out);
		exit(EXIT_FAILURE);
	}
	send_eom(out);

	free(line);
	free(map);
	free(path);
}

static bool
parse_gophermap_item(struct opt_options *options, struct item *item,
    const char *selector, const char *line, FILE *out)
{
	assert(item != NULL);
	assert(line != NULL);
	assert(out != NULL);

	const char *p = line;

	if (*p == '\t') {
		send_error(out, "E: Malformed line", line);
		return (false);
	}
	char type = *p++;

	size_t l = strcspn(p, "\t");
	if (l == 0) {
		send_error(out, "E: Malformed line", line);
		return (false);
	}
	char *display = malloc(l+1);
	if (display == NULL) {
		send_error(out, "E: malloc", strerror(errno));
		send_info(out, "I: I could not allocate memory.", NULL);
		exit(EXIT_FAILURE);
	}
	strncpy(display, p, l);
	display[l] = '\0';
	p += l;

	if (*p == '\0') {
		send_error(out, "E: Malformed line", line);
		free(display);
		return (false);
	}
	p++;

	l = strcspn(p, "\t");
	bool relative = (l > 0 && *p != '/' && strncasecmp(p, "GET ", 4) != 0);
	char *sel;
	if (relative) {
		char *rel = malloc(l+1);
		if (rel == NULL) {
			send_error(out, "E: malloc", strerror(errno));
			send_info(out, "I: I could not allocate memory.", NULL);
			exit(EXIT_FAILURE);
		}
		strncpy(rel, p, l);
		rel[l] = '\0';
		sel = tool_join_path(selector, rel, out);
		free(rel);
	} else {
		sel = malloc(l+1);
		if (sel == NULL) {
			send_error(out, "E: malloc", strerror(errno));
			send_info(out, "I: I could not allocate memory.", NULL);
			exit(EXIT_FAILURE);
		}
		strncpy(sel, p, l);
		sel[l] = '\0';
	}
	p += l;

	if (*p != '\0')
		p++;

	size_t ll = l = strcspn(p, "\t");
	bool use_default = (l == 0 || (l == 1 && *p == '+'));
	if (use_default)
		ll = strlen(opt_get_host(options));
	char *host = malloc(ll+1);
	if (host == NULL) {
		send_error(out, "E: malloc", strerror(errno));
		send_info(out, "I: I could not allocate memory.", NULL);
		exit(EXIT_FAILURE);
	}
	if (use_default)
		strncpy(host, opt_get_host(options), ll);
	else
		strncpy(host, p, ll);
	host[ll] = '\0';
	p += l;

	if (*p != '\0')
		p++;

	ll = l = strcspn(p, "\t");
	use_default = (l == 0 || (l == 1 && *p == '+'));
	if (use_default)
		ll = strlen(opt_get_port(options));
	char *port = malloc(ll+1);
	if (host == NULL) {
		send_error(out, "E: malloc", strerror(errno));
		send_info(out, "I: I could not allocate "
		    "memory.", NULL);
		exit(EXIT_FAILURE);
	}
	strncpy(port, p, l);
	if (use_default)
		strncpy(port, opt_get_port(options), ll);
	else
		strncpy(port, p, ll);
	port[ll] = '\0';

	item->type = type;
	item->display = display;
	item->selector = sel;
	item->host = host;
	item->port = port;

	return (true);
}

static void free_gophermap_item(struct item *item)
{
	assert(item != NULL);

	free(item->display);
	free(item->selector);
	free(item->host);
	free(item->port);
}
