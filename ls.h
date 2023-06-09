#ifndef	LS_H
#define LS_H

#define handle_error(msg)		\
	do {				\
		perror(msg);		\
		exit(EXIT_FAILURE);	\
	} while (0)

/* create html page for given directory, listing the files */
char *mkhtml(const char *dir, const char *uri);

/* append to $str; realloc if necessary */
char *appendstr(char *str, size_t *bufsize, size_t bufinc, char *fmt, ...);

/* get file type of $filename and store in $filetype */
void get_filetype(const char *filename, char *filetype, size_t size);

#endif	/* LS_H */
