#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
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

static int get_method_and_uri(char *buf, char *method, size_t method_size,
				char *uri, size_t uri_size)
{
	char *saveptr;
	char *token;

	token = strtok_r(buf, " \t", &saveptr);
	if (!token)
		return 0;
	strncpy(method, token, method_size-1);

	token = strtok_r(NULL, " \t", &saveptr);
	if (!token)
		return 1;
	strncpy(uri, token, uri_size-1);

	return 2;
}

int http_read_req(SSL *ssl, char *method, size_t method_size,
		   char *uri, size_t uri_size, int *close)
{
	char buf[LINE_MAXLEN];
	int err = 0;

	PDEBUG("%s: Waiting to read\n", __func__);
	if (ssl_readline(ssl, buf, LINE_MAXLEN, &err) == 0) {
		if (err) {
			fprintf(stderr, "[*] error reading request headers\n");
			return -1;
		}
		/* nothing to read */
		PDEBUG("%s: nothing to read\n", __func__);
		return 0;
	}
	PDEBUG("%s: read request line\n", __func__);
	puts(buf);
	if (get_method_and_uri(buf, method, method_size, uri, uri_size) != 2) {
		fprintf(stderr, "[*] invalid request '%s'\n", buf);
		return 1;
	}
	PDEBUG("\n\nRequest Headers\n"
		"===============\n");
	/* TODO: check for Connection: close header and set *close=1 */
	*close = 0;
	/* check for empty line that terminates the request headers */
	while (strcmp(buf, "\r\n") && strcmp(buf, "\n")) {
		PDEBUG("%s", buf);
		if (ssl_readline(ssl, buf, sizeof(buf), &err) == 0) {
			if (err) {
				fprintf(stderr, "[*] error reading request headers\n");
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
	PDEBUG("===============\n"
		"%s: finished reading request headers\n\n", __func__);
	return 1;
}

void send_404(SSL *ssl, const char *filename)
{
	const char *response = "<html>\n"
				"<body>\n"
				"<p> 404 Not found </p>\n"
				"</body>\n"
				"</html>\n";
	const char *type = "text/html\r\n";
	char content_len[32];

	sprintf(content_len, "%ld", strlen(response));

	PDEBUG("%s\n", __func__);
	SSL_write(ssl, http_notfound, strlen(http_notfound));
	SSL_write(ssl, http_close, strlen(http_close));
	SSL_write(ssl, http_content_type, strlen(http_content_type));
	SSL_write(ssl, type, strlen(type));
	SSL_write(ssl, http_content_len, strlen(http_content_len));
	SSL_write(ssl, content_len, strlen(content_len));
	SSL_write(ssl, "\r\n\r\n", 4);
	SSL_write(ssl, response, strlen(response));
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
	strncpy(filename, sfs_argopts.dir, size-1);
	strncat(filename, uri, size-1);

	PDEBUG("%s: filename='%s'\n", __func__, filename);
	r = realpath(filename, NULL);
	if (!r) {
		perror("Failed to find URI");
		return -1;
	}
	strncpy(filename, r, size-1);
	free(r);

	return 0;
}

static void send_file_stats(SSL *ssl, int fd, const char *filename)
{
	char filetype[64], content_len[32];
	struct stat sb;

	if (fstat(fd, &sb) == -1) {
		send_404(ssl, filename);
		return;
	}
	get_filetype(filename, filetype, sizeof(filetype));
	sprintf(content_len, "%ld", sb.st_size);

	SSL_write(ssl, http_ok, strlen(http_ok));
	SSL_write(ssl, http_server, strlen(http_server));
	SSL_write(ssl, http_keep_alive, strlen(http_keep_alive));
	SSL_write(ssl, http_content_type, strlen(http_content_type));
	SSL_write(ssl, filetype, strlen(filetype));
	SSL_write(ssl, "\r\n", 2);
	SSL_write(ssl, http_content_len, strlen(http_content_len));
	SSL_write(ssl, content_len, strlen(content_len));
	SSL_write(ssl, "\r\n\r\n", 4);

}

static void send_file(SSL *ssl, const char *filename)
{
	char buf[4096];
	ssize_t nread;
	int fd;

	fd = open(filename, O_RDONLY);
	if (fd == -1) {
		send_404(ssl, filename);
		return;
	}

	send_file_stats(ssl, fd, filename);
	/* TODO: error checking */
	while ((nread = read(fd, buf, sizeof(buf))) > 0)
		SSL_write(ssl, buf, nread);

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

	sprintf(content_len, "%ld", len);
	
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
	if (get_filename(filename, sizeof(filename), uri) == -1) {
		send_404(ssl, filename);
		return;
	}
	PDEBUG("%s: file='%s'\n", __func__, filename);
	if ((ret = is_dir(filename))) {
		if (ret == -1) {
			send_404(ssl, filename);
			return;
		}
		char *html = mkhtml(filename, uri);
		send_response_headers(ssl, "text/html", strlen(html));
		SSL_write(ssl, html, strlen(html));
		/* send directory contents */
		free(html);
	} else {
		send_file(ssl, filename);
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
	char method[METHOD_MAXLEN], uri[URI_MAXLEN];
	int close = 0;

	method[0] = uri[0] = '\0';
	while (http_read_req(ssl, method, sizeof(method), uri, sizeof(uri),
			     &close) > 0) {
		printf("[-] method='%s', uri='%s'\n", method, uri);
		if (strncasecmp(method, "GET", 4) && strncasecmp(method, "HEAD", 5)) {
			printf("[*] method not implemented\n");
			return;
		}

		if (!strcmp(method, "GET"))
			serve_file(ssl, uri);
		else
			head_file(ssl, uri);

		if (close)
			break;
	}
	printf("[-] connection closed\n");
}
