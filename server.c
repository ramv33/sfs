#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <getopt.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "common.h"
#include "ls.h"
#include "ssl.h"
#include "http.h"

#define	DEFAULT_PORT	6969
#define BACKLOG		8

#define errmsg(msg)	perror("[*] " msg);

static struct {
	int no_recurse;
	int show_hidden;
	int port;
} argopts;

static void parse_args(int *argc, char **argv);
int create_socket(int port);

int main(int argc, char **argv)
{
	struct sockaddr_in clientaddr;
	char *dir = NULL, addrstr[INET_ADDRSTRLEN];
	int sockfd, connfd;
	SSL_CTX *ssl_ctx = NULL;
	SSL *ssl = NULL;

	parse_args(&argc, argv);
	/* If argc is non-zero, it points to the directory argument */
	if (argc)
		dir = argv[argc];
	else
		dir = getcwd(NULL, 0);	// NOTE: has to be freed
	puts(dir);

	sockfd = create_socket(argopts.port);
	if (sockfd == -1)
		exit(EXIT_FAILURE);
	/* create SSL context */
	ssl_ctx = create_context();
	/* configure server context with appropriate key files */
	configure_context(ssl_ctx);

	while (true) {
		socklen_t clientlen = sizeof(clientaddr);
		connfd = accept(sockfd, (struct sockaddr *)&clientaddr, &clientlen);
		if (connfd == -1) {
			errmsg("accept");
			close(sockfd);
			exit(EXIT_FAILURE);
		}
		printf("%s:%d connected\n", inet_ntop(AF_INET, &clientaddr.sin_addr,
			addrstr, sizeof(addrstr)), clientaddr.sin_port);

		/* create server SSL structure using newly accepted client socket */
		ssl = SSL_new(ssl_ctx);
		SSL_set_fd(ssl, connfd);

		/* wait for SSL connection from client */
		if (SSL_accept(ssl) <= 0) {
			fprintf(stderr, "SSL_accept failed: ");
			ERR_print_errors_fp(stderr);
			break;
		} else {
			puts("Client SSL connection accepted");
		}
		http_handle(ssl);
		PDEBUG("closing connection...\n");
		SSL_shutdown(ssl);
		SSL_free(ssl);
		close(connfd);
	}
exit:
	if (ssl != NULL) {
		SSL_shutdown(ssl);
		SSL_free(ssl);
	}
	SSL_CTX_free(ssl_ctx);

	if (!argc)
		free(dir);
	if (connfd != -1)
		close(connfd);
	if (sockfd != -1)
		close(connfd);

	return 0;
}

static void parse_args(int *argc, char **argv)
{
	int c;

	static struct option long_options[] = {
		{"port", required_argument, NULL, 'p'},
		{"all", no_argument, NULL, 'a'},
		{"no-recurse", no_argument, NULL, 'R'}
	};

	/* defaults */
	argopts.port = DEFAULT_PORT;

	while (1) {
		if ((c = getopt_long(*argc, argv, "p:aR", long_options, NULL)) == -1)
			break;
		switch (c) {
			case 'p':
				argopts.port = strtol(optarg, NULL, 10);
				PDEBUG("port=%d\n", argopts.port);
				break;
			case 'a':
				PDEBUG("show_hidden\n");
				argopts.show_hidden = 1;
				break;
			case 'R':
				PDEBUG("no_recurse\n");
				argopts.no_recurse = 1;
		}
	}
	/* If a directory name is passed, serve files from that directory.
	 * Set argc to the index of the directory in argv. If no directory
	 * specified, set argc to 0. */
	if (optind < *argc)
		*argc = optind;
	else
		*argc = 0;
}
