/*
 *  Copyright 2022 The OpenSSL Project Authors. All Rights Reserved.
 *
 *  Licensed under the Apache License 2.0 (the "License").  You may not use
 *  this file except in compliance with the License.  You can obtain a copy
 *  in the file LICENSE in the source distribution or at
 *  https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "ssl.h"

/*
 * This flag won't be useful until both accept/read (TCP & SSL) methods
 * can be called with a timeout. TBD.
 */
static volatile bool    server_running = true;

int create_socket(int port)
{
	int s = -1;
	int optval = 1;
	struct sockaddr_in addr;

	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0) {
		perror("Unable to create socket");
		return -1;
	}

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;

	/* Reuse the address */
	if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0)
		perror("setsockopt(SO_REUSEADDR) failed");

	if (bind(s, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
		perror("Unable to bind");
		return -1;
	}

	if (listen(s, 1) < 0) {
		perror("Unable to listen");
		return -1;
	}

	return s;
}

SSL_CTX* create_context(void)
{
	const SSL_METHOD *method;
	SSL_CTX *ctx;

	method = TLS_server_method();
	ctx = SSL_CTX_new(method);
	if (ctx == NULL) {
		perror("Unable to create SSL context");
		ERR_print_errors_fp(stderr);
		return NULL;
	}

	return ctx;
}

void configure_context(SSL_CTX *ctx, const char *cert, const char *key)
{
	if (SSL_CTX_use_certificate_chain_file(ctx, cert) <= 0) {
		ERR_print_errors_fp(stderr);
		exit(EXIT_FAILURE);
	}

	if (SSL_CTX_use_PrivateKey_file(ctx, key, SSL_FILETYPE_PEM) <= 0) {
		ERR_print_errors_fp(stderr);
		exit(EXIT_FAILURE);
	}
}

ssize_t read_ssl(SSL *ssl, char *rxbuf, size_t size)
{
	int rxlen;

	if ((rxlen = SSL_read(ssl, rxbuf, size-1)) > 0)
		rxbuf[rxlen] = '\0';
	return rxlen;
}

/*
 * ssl_readline:
 * 	Read atmost $size-1 characters into $buf and nul-terminates the buffer.
 * 	Reads one line, i.e upto first '\n' character.
 * 	Returns the total no: of characters read excluding the nul-terminator.
 * 	If an error has occured, *err is set to 1; the user should set this to
 * 	zero before calling the function.
 */
ssize_t ssl_readline(SSL *ssl, char *buf, size_t size, int *err)
{
	size_t i;
	char *p = buf;
	int ret;

	// change till reading "\r\n"?
	for (i = 0; i < (size-1); ++i) {
		ret = SSL_read(ssl, p, 1);
		if (!ret)
			break;
		switch (SSL_get_error(ssl, ret)) {
		case SSL_ERROR_NONE:
		case SSL_ERROR_WANT_READ:
			break;
			// goto out;
		case SSL_ERROR_ZERO_RETURN:
			*p = '\0';
			return i;
		default:
			if (err)
				*err = 1;
			*p = '\0';
			return i;
		}
		if (*p++ == '\n') {
			++i;
			break;
		}
	}
	*p = '\0';
	return i;
}
