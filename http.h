#ifndef HTTP_H
#define HTTP_H 1

/* read and display request headers */
void http_read_reqhdrs(SSL *ssl);

/* handle http requests */
void http_handle(SSL *ssl);

#endif /* ifndef HTTP_H */
