/*-
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <tobias.rehbein@web.de> wrote this file. As long as you retain this notice
 * you can do whatever you want with this stuff. If we meet some day, and you
 * think this stuff is worth it, you can buy me a beer in return.
 */

#define _POSIX_C_SOURCE 200809

#include <assert.h>
#include <syslog.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <errno.h>

#include "gophermap.h"
#include "tools.h"

bool
gophermap_parse_item(struct opt_options *options, struct item *item,
    const char *selector, const char *line, FILE *out)
{
	assert(item != NULL);
	assert(line != NULL);
	assert(out != NULL);

	const char *p = line;

	if (*p == '\t') {
		syslog(LOG_NOTICE, "malformed gophermap line: \"%s\"", line);
		send_error(out, "E: Malformed line", line);
		return (false);
	}
	char type = *p++;

	size_t l = strcspn(p, "\t");
	if (l == 0) {
		syslog(LOG_NOTICE, "malformed gophermap line: \"%s\"", line);
		send_error(out, "E: Malformed line", line);
		return (false);
	}
	char *display = malloc(l+1);
	if (display == NULL) {
		syslog(LOG_ERR, "malloc error: %m");
		send_error(out, "E: malloc", strerror(errno));
		send_info(out, "I: I could not allocate memory.", NULL);
		exit(EXIT_FAILURE);
	}
	strncpy(display, p, l);
	display[l] = '\0';
	p += l;

	if (*p == '\0') {
		syslog(LOG_NOTICE, "malformed gophermap line: \"%s\"", line);
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
			syslog(LOG_ERR, "malloc error: %m");
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
			syslog(LOG_ERR, "malloc error: %m");
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
		syslog(LOG_ERR, "malloc error: %m");
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
		syslog(LOG_ERR, "malloc error: %m");
		send_error(out, "E: malloc", strerror(errno));
		send_info(out, "I: I could not allocate "
		    "memory.", NULL);
		exit(EXIT_FAILURE);
	}
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

void
gophermap_free_item(struct item *item)
{
	assert(item != NULL);

	free(item->display);
	free(item->selector);
	free(item->host);
	free(item->port);
}
