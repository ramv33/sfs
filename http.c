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

/* HTTP response codes */
const char *http_ok = "HTTP/1.0 200 OK\r\n";
const char *http_forbidden = "HTTP/1.0 403 Forbidden\r\n";
const char *http_notfound = "HTTP/1.0 404 Not Found\r\n";

const char *http_server = "Server: sfs\r\n";
const char *http_content_type = "Content-type: ";
const char *http_content_len = "Content-length: ";
const char *http_keep_alive = "Connection: keep-alive\r\n";
const char *http_close = "Connection: close\r\n";

int http_read_req(SSL *ssl, char *method, size_t method_size,
		   char *uri, size_t uri_size, int *close)
{
	char buf[LINE_MAXLEN];
	int err = 0;

	PDEBUG("%s: Waiting to read\n", __func__);
	if (ssl_readline(ssl, buf, LINE_MAXLEN, &err) == 0) {
		if (err) {
			fprintf(stderr, "error reading request headers\n");
			return -1;
		}
		/* nothing to read */
		PDEBUG("%s: nothing to read\n", __func__);
		return 0;
	}
	PDEBUG("%s: read request line\n", __func__);
	puts(buf);
	sscanf(buf, "%s %s", method, uri);
	PDEBUG("\n\nRequest Headers\n"
		"===============\n");
	/* TODO: check for Connection: close header and set *close=1 */
	*close = 0;
	/* check for empty line that terminates the request headers */
	while (strcmp(buf, "\r\n")) {
		PDEBUG("%s", buf);
		if (ssl_readline(ssl, buf, sizeof(buf), &err) == 0) {
			if (err) {
				fprintf(stderr, "error reading request headers\n");
				return -1;
			}
			return 0;
		}
		// if (!*close && !cmp_header_key(buf, "Connection")) {
		// 	if (!cmp_header_val(buf, "close"))
		// 		*close = 1;
		// }
#define CONN_CLOSE_HDR "Connection: close"
		if (!*close && !strncasecmp(buf, CONN_CLOSE_HDR,
					    strlen(CONN_CLOSE_HDR))) {
			PDEBUG("close\n");
			*close = 1;
		}
	}
	PDEBUG("%s: finished reading request headers\n", __func__);
	return 1;
}

/*
 * get_filename - Get filename relative to sfs_argopts.dir from the URI after
 * 		  resolving any symlinks.
 * 		  Return -1 if invalid filename after perror.
 * @filename: The filename is stored here.
 * @size: Size of @filename. Should be PATH_MAX.
 * @uri: URI given in HTTP request.
 */
static int get_filename(char *filename, size_t size, const char *uri)
{
	char *r = NULL;

	/* need to check their usage again */
	strncpy(filename, sfs_argopts.dir, size);
	strncat(filename, uri, size-1);

	PDEBUG("%s: filename='%s'\n", __func__, filename);
	r = realpath(filename, NULL);
	if (!r) {
		perror("Failed to find URI");
		return -1;
	}
	strncpy(filename, r, size);
	free(r);

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

static void send_response_headers(SSL *ssl, const char *type, const size_t len)
{
	char content_len[32];

	sprintf(content_len, "%d", len);
	
	SSL_write(ssl, http_ok, strlen(http_ok));
	SSL_write(ssl, http_server, strlen(http_server));
	SSL_write(ssl, http_keep_alive, strlen(http_keep_alive));
	SSL_write(ssl, http_content_type, strlen(http_content_type));
	SSL_write(ssl, type, strlen(type));
	SSL_write(ssl, "\r\n", 2);
	SSL_write(ssl, http_content_len, strlen(http_content_len));
	SSL_write(ssl, content_len, strlen(content_len));
	SSL_write(ssl, "\r\n\r\n", 4);
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
		send_response_headers(ssl, "text/html", strlen(html));
		SSL_write(ssl, html, strlen(html));
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
	int close = 0;

	while (http_read_req(ssl, method, sizeof(method), uri, sizeof(uri),
			     &close) >= 0) {
		printf("method='%s', uri='%s'\n", method, uri);
		if (strncasecmp(method, "GET", 4) && strncasecmp(method, "HEAD", 5)) {
			printf("Method not implemented\n");
			return;
		}

		if (!strcmp(method, "GET"))
			serve_file(ssl, uri);
		else
			head_file(ssl, uri);

		if (close)
			break;
	}
}
