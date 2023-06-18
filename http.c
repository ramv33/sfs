#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

#include "common.h"
#include "ssl.h"
#include "ls.h"

#define LINE_MAXLEN	4096
#define	METHOD_MAXLEN	128
#define	URI_MAXLEN	4096
#define	VERSION_MAXLEN	128

void http_read_req(SSL *ssl, char *method, size_t method_size,
		   char *uri, size_t uri_size)
{
	char buf[LINE_MAXLEN];
	int err = 0;

	PDEBUG("%s: Waiting to read\n", __func__);
	if (ssl_readline(ssl, buf, LINE_MAXLEN, &err) == 0) {
		if (err)
			fprintf(stderr, "error reading request headers\n");
		/* nothing to read */
		PDEBUG("%s: nothing to read\n", __func__);
		return;
	}
	PDEBUG("%s: read request line\n", __func__);
	puts(buf);
	sscanf(buf, "%s %s", method, uri);
	PDEBUG("%s: reading request headers\n", __func__);
	printf("\n\nRequest Headers\n"
		"===============\n");
	/* check for empty line that terminates the request headers */
	while (strcmp(buf, "\r\n")) {
		printf("%s", buf);
		if (ssl_readline(ssl, buf, sizeof(buf), &err) == 0) {
			if (err)
				fprintf(stderr, "error reading request headers\n");
			return;
		}
	}
	PDEBUG("%s: finished reading request headers\n", __func__);
}

/*
 * get_filename - Get filename relative to sfs_argopts.dir from the URI after
 * 		  resolving any symlinks.
 * 		  Return -1 if invalid filename after perror.
 */
static int get_filename(char *filename, const char *uri)
{
	return 0;
}

static int get_file_fd(const char *filename)
{

}

static void send_file(SSL *ssl, const char *filename)
{

}

static int is_dir(const char *filename)
{
	struct stat sb;

	if (lstat(filename, &sb) == -1) {
		perror("[*] lstat");
		return -1;
	}

	return (sb.st_mode & S_IFMT) == S_IFDIR;
}

/**
 * serve_file - Serve file given in @uri in response to a GET request
 *
 * @ssl: SSL connection
 * @uri: path to the file from current directory
 */
void serve_file(SSL *ssl, char *uri)
{
	char filename[PATH_MAX];
	int ret;

	rm_trailing_slash(uri);
	if (get_filename(filename, sizeof(filename), uri) == -1)
		return;
	PDEBUG("%s: file='%s'\n", __func__, filename);
	if (ret = is_dir(filename)) {
		if (ret == -1)
			return;
		char *html = mkhtml(filename);
		/* send directory contents */
		free(html);
	} else {
		/* send file */
	}
}

/**
 * head_file - Send response header for file given in @uri
 *
 * @ssl: SSL connection
 * @uri: path to the file from current directory
 */
void head_file(SSL *ssl, char *uri)
{
	PDEBUG("%s: file='%s'\n", __func__, uri);
}

void http_handle(SSL *ssl)
{
	ssize_t ret;
	char buf[LINE_MAXLEN];
	char method[METHOD_MAXLEN], uri[URI_MAXLEN];
	char filename[LINE_MAXLEN];

	while (1) {
		http_read_req(ssl, method, sizeof(method), uri, sizeof(uri));
		printf("method='%s', uri='%s'\n", method, uri);
		if (strncasecmp(method, "GET", 4) && strncasecmp(method, "HEAD", 5)) {
			printf("Method not implemented\n");
			return;
		}

		if (!strcmp(method, "GET"))
			serve_file(ssl, uri);
		else
			head_file(ssl, uri);
	}
}
