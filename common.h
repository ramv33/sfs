#ifndef COMMON_H
#define COMMON_H 1

#undef PDEBUG
#ifdef DEBUG
#	define PDEBUG(fmt, args...) fprintf(stderr, fmt, ## args)
#else
#	define PDEBUG(fmt, args...)	/* not debugging; do nothing */
#endif

struct argopts {
	int		no_recurse;
	int		show_hidden;
	int		port;
	const char 	*dir;
};

extern struct argopts sfs_argopts;

#endif /* ifndef COMMON_H */
