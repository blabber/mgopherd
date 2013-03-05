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

#include "send.h"
#include "tools.h"

#define INITIALCAPACITY	32

char *
tool_mimetype(const char *path, FILE *out)
{
	assert(path != NULL);

	magic_t mh = magic_open(MAGIC_MIME_TYPE);
	if (mh == NULL) {
		send_error(out, "E: magic_open", strerror(errno));
		send_info(out, "I: I could not open a libmagic handle.", NULL);
		send_eom(out);
		exit(EXIT_FAILURE);
	}

	if (magic_load(mh, NULL) == -1) {
		send_error(out, "E: magic_load", magic_error(mh));
		send_info(out, "I: I could not load the magic database.", NULL);
		send_eom(out);
		exit(EXIT_FAILURE);
	}

	const char *mime = magic_file(mh, path);
	if (mime == NULL) {
		send_error(out, "E: magic_file", magic_error(mh));
		send_info(out, "I: I could not identify the content of this "
		    "file", path);
		send_eom(out);
		exit(EXIT_FAILURE);
	}

	char *ret = strdup(mime);
	if (ret == NULL)
	{
		send_error(out, "E: strdup mime", strerror(errno));
		send_info(out, "I: I could not copy a string", mime);
		send_eom(out);
		exit(EXIT_FAILURE);
	}

	magic_close(mh);

	return (ret);
}

char *
tool_join_path(const char *part1, const char *part2, FILE *out)
{
	assert(part1 != NULL);
	assert(part2 != NULL);

	char *joined = malloc(PATH_MAX);
	if (joined == NULL) {
		send_error(out, "E: malloc joined", strerror(errno));
		send_info(out, "I: I could not join path elements.", NULL);
		send_eom(out);
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
		send_error(out, "E: joinpath: joined too long", NULL);
		send_info(out, "I: A joined path was too long.", NULL);
		send_eom(out);
		exit(EXIT_FAILURE);
	}

	return (joined);
}

void
tool_strip_crlf(char *line)
{
	assert(line != NULL);

	char *p = strchr(line, '\n');
	if (p != NULL && p > line && *(p-1) == '\r')
		p--;
	if (p != NULL)
		*p = '\0';
}
