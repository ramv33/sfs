#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <getopt.h>
#include <pthread.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "common.h"
#include "ls.h"
#include "ssl.h"
#include "http.h"

#define	DEFAULT_PORT	6969
#define BACKLOG		8

#define errmsg(msg)	perror("[*] " msg);

struct argopts sfs_argopts;

struct thread_arg {
	SSL_CTX		*ssl_ctx;
	struct thread	*thread;
	int		connfd;
};

struct thread {
	pthread_t	tid;
	bool		running;
};

static void parse_args(int *argc, char **argv);
void *server_thread(void *arg);
int find_free_thread(struct thread *threads, int nthreads);
struct thread_arg *thread_arg_create(SSL_CTX *ssl_ctx, int connfd, struct thread *thread);
int create_socket(int port);

pthread_mutex_t threads_lock;

int main(int argc, char **argv)
{
	struct sockaddr_in clientaddr;
	struct thread_arg *targ = NULL;
	struct thread *threads = NULL;
	SSL_CTX *ssl_ctx = NULL;
	char addrstr[INET_ADDRSTRLEN];
	char *dir = NULL;
	int sockfd, connfd;
	int ti;

	parse_args(&argc, argv);
	/* If argc is non-zero, it points to the directory argument */
	if (argc)
		dir = argv[argc];
	else
		dir = getcwd(NULL, 0);	// NOTE: has to be freed
	puts(dir);

	threads = calloc(sfs_argopts.nthreads, sizeof(*threads));
	if (!threads) {
		fprintf(stderr, "cannot create threads (nthreads=%d)\n",
			sfs_argopts.nthreads);
		exit(EXIT_FAILURE);
	}

	rm_trailing_slash(dir);
	sfs_argopts.dir = dir;

	sockfd = create_socket(sfs_argopts.port);
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
			/* do not exit? */
			errmsg("accept");
			close(sockfd);
			exit(EXIT_FAILURE);
		}
		printf("%s:%d connected\n", inet_ntop(AF_INET, &clientaddr.sin_addr,
			addrstr, sizeof(addrstr)), clientaddr.sin_port);

		/* thread creation */
		ti = find_free_thread(threads, sfs_argopts.nthreads);
		threads[ti].running = true;
		if (ti == -1) {
			fprintf(stderr, "[*] out of threads, closing connection\n");
			close(connfd);
			continue;
		}
		targ = thread_arg_create(ssl_ctx, connfd, &threads[ti]);
		if (pthread_create(&threads[ti].tid, NULL, server_thread, targ)) {
			fprintf(stderr, "[*] error creating thread\n");
			threads[ti].running = false;
			close(connfd);
		}
	}

	SSL_CTX_free(ssl_ctx);

	if (!argc)
		free(dir);
	if (sockfd != -1)
		close(sockfd);

	return 0;
}

void *server_thread(void *arg)
{
	struct thread_arg *targ = arg;
	SSL *ssl = NULL;

	ssl = SSL_new(targ->ssl_ctx);
	SSL_set_fd(ssl, targ->connfd);

	if (SSL_accept(ssl) <= 0) {
		fprintf(stderr, "SSL_accept failed: ");
		ERR_print_errors_fp(stderr);
		pthread_exit(NULL);
	} else {
		printf("[+] Client SSL connection accepted\n");
	}

	http_handle(ssl);
	printf("[-] Closing connection\n");
	SSL_shutdown(ssl);
	SSL_free(ssl);
	close(targ->connfd);

	targ->thread->running = false;

	pthread_exit(NULL);
}

struct thread_arg *thread_arg_create(SSL_CTX *ssl_ctx, int connfd, struct thread *thread)
{
	struct thread_arg *targ = malloc(sizeof(*targ));

	if (!targ)
		return NULL;

	targ->ssl_ctx = ssl_ctx;
	targ->connfd = connfd;
	targ->thread = thread;

	return targ;
}

int find_free_thread(struct thread *threads, int nthreads)
{
	for (int i = 0; i < nthreads; i++)
		if (!threads[i].running)
			return i;
	return -1;
}

static void parse_args(int *argc, char **argv)
{
	int c;

	static struct option long_options[] = {
		{"port", required_argument, NULL, 'p'},
		{"all", no_argument, NULL, 'a'},
		{"no-recurse", no_argument, NULL, 'R'},
		{"num-threads", required_argument, NULL, 't'},
	};

	/* defaults */
	sfs_argopts.port = DEFAULT_PORT;
	sfs_argopts.nthreads = DEFAULT_NTHREADS;
	while (1) {
		if ((c = getopt_long(*argc, argv, "p:aR", long_options, NULL)) == -1)
			break;
		switch (c) {
			case 'p':
				sfs_argopts.port = strtol(optarg, NULL, 10);
				PDEBUG("port=%d\n", sfs_argopts.port);
				break;
			case 'a':
				PDEBUG("show_hidden\n");
				sfs_argopts.show_hidden = 1;
				break;
			case 'R':
				PDEBUG("no_recurse\n");
				sfs_argopts.no_recurse = 1;
				break;
			case 't':
				sfs_argopts.nthreads = strtol(optarg, NULL, 10);
				PDEBUG("nthreads=%d\n", sfs_argopts.nthreads);
				if (sfs_argopts.nthreads <= 0) {
					fprintf(stderr, "Number of threads must be > 0\n");
					exit(EXIT_FAILURE);
				}
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
