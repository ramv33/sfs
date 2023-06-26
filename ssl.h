#ifndef SSL_H
#define SSL_H

#include <openssl/ssl.h>
#include <openssl/err.h>

/* create SOCK_STREAM socket and bind it to server's address and set it as listening */
int create_socket(int port);

/* create SSL context using TLS_server_method and return a pointer to it */
SSL_CTX *create_context(void);

/* configure server context with certifcate and private key */
void configure_context(SSL_CTX *ctx, const char *cert, const char *key);

/* read from SSL connection */
ssize_t ssl_read(SSL *ssl, char *rxbuf, size_t size);

/* read line from SSL connection */
ssize_t ssl_readline(SSL *ssl, char *buf, size_t size, int *err);

#endif	/* ifndef SSL_H */
