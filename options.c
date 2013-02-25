/*-
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <tobias.rehbein@web.de> wrote this file. As long as you retain this notice
 * you can do whatever you want with this stuff. If we meet some day, and you
 * think this stuff is worth it, you can buy me a beer in return.
 */

#define _POSIX_C_SOURCE 200809

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "options.h"

#define GOPHERPORT "70"

void usage(void);

struct opt_options {
	char *host;
	char *port;
	char *root;
};

struct opt_options *opt_parse(int argc, char **argv)
{
	assert(argc > 0);
	assert(argv != NULL && *argv != NULL);

	struct opt_options *options = malloc(sizeof(struct opt_options));
	if (options == NULL) {
		fprintf(stderr, "malloc options: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	options->root = NULL;
	options->host = NULL;
	options->port = NULL;

	int opt;
	while ((opt = getopt(argc, argv, "r:H:p:h")) != -1) {
		switch (opt){
		case 'r':
			free(options->root);
			options->root = realpath(optarg, NULL);
			if (options->root == NULL) {
				fprintf(stderr, "realpath options->root: %s\n",
				    strerror(errno));
				exit(EXIT_FAILURE);
			}
			break;
		case 'H':
			free(options->host);
			options->host = strdup(optarg);
			if (options->host == NULL) {
				fprintf(stderr, "strdup options->host: %s\n",
				    strerror(errno));
				exit(EXIT_FAILURE);
			}
			break;
		case 'p':
			free(options->port);
			options->port = strdup(optarg);
			if (options->port == NULL) {
				fprintf(stderr, "strdup options->port: %s\n",
				    strerror(errno));
				exit(EXIT_FAILURE);
			}
			break;
		case 'h':
			usage();
			exit(EXIT_SUCCESS);
		default:
			usage();
			exit(EXIT_FAILURE);
		}
	}

	if (options->root == NULL) {
		options->root = realpath(".", NULL);
		if (options->root == NULL) {
			fprintf(stderr, "realpath options->root: %s\n",
			    strerror(errno));
			exit(EXIT_FAILURE);
		}
	}

	if (options->host == NULL) {
		long hnlen = sysconf(_SC_HOST_NAME_MAX);
		options->host = malloc(hnlen+1);
		if (options->host == NULL) {
			fprintf(stderr, "malloc options->host: %s\n",
			    strerror(errno));
			exit(EXIT_FAILURE);
		}
		gethostname(options->host, hnlen);
	}

	if (options->port == NULL) {
		options->port = strdup(GOPHERPORT);
		if (options->port == NULL) {
			fprintf(stderr, "realpath options->port: %s\n",
			    strerror(errno));
			exit(EXIT_FAILURE);
		}
	}

	return (options);
}

void
opt_free(struct opt_options *options)
{
	assert(options != NULL);

	free(options->root);
	free(options->host);
	free(options->port);
	free(options);
}

char *
opt_get_host(struct opt_options *options)
{
	assert(options != NULL);
	assert(options->host != NULL);

	return (options->host);
}

char *
opt_get_root(struct opt_options *options)
{
	assert(options != NULL);
	assert(options->root != NULL);

	return (options->root);
}

char *
opt_get_port(struct opt_options *options)
{
	assert(options != NULL);
	assert(options->port != NULL);

	return (options->port);
}

void
usage(void)
{
	fputs("Usage: mgopherd -r root -H host -p port\n", stderr);
	fputs("       mgopherd -h\n", stderr);
}
