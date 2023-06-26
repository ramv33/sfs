#ifndef COMMON_H
#define COMMON_H 1

#include <string.h>

#undef PDEBUG
#ifdef DEBUG
#	define PDEBUG(fmt, args...) fprintf(stderr, fmt, ## args)
#else
#	define PDEBUG(fmt, args...)	/* not debugging; do nothing */
#endif

#define DEFAULT_NTHREADS	2
#define DEFAULT_KEY_FILE	"key.pem"
#define DEFAULT_CERT_FILE	"cert.pem"

struct argopts {
	int		no_recurse;
	int		show_hidden;
	int		port;
	int		nthreads;
	const char 	*dir;
	const char	*cert;
	const char	*key;
};

extern struct argopts sfs_argopts;

static void rm_trailing_slash(char *dir)
{
	char *endp = dir + strlen(dir) - 1;
	char *p = endp;

	while (p >= dir && *p == '/')
		--p;
	if (++p <= endp)
		*++p = '\0';
}

#endif /* ifndef COMMON_H */
