#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <dirent.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "ls.h"

#define BUFFSIZE	4096
#define BUFFSIZEINC	4096

// int main(void)
// {
// 	char *html = mkhtml(NULL, ".");
// 	puts(html);
// 	free(html);
// 	return 0;
// }

int appendstr(char *str, size_t *bufsize, size_t bufinc, char *fmt, ...)
{
	va_list ap;
	size_t ret;
	int len;

	va_start(ap, fmt);
	len = strlen(str);
	ret = vsnprintf(str+len, *bufsize-len, fmt, ap);
	if (ret >= (*bufsize-len)) {
		/* ap is undefined after call to vsnprintf */
		va_start(ap, fmt);
		// more than bufinc extra space needed? 
		if ((ret - (*bufsize-len)) >= bufinc) {
			size_t newbufinc = ret - (*bufsize-len);
			// round newbufinc to next multiple of bufinc
			newbufinc += -newbufinc % bufinc;
			bufinc = newbufinc;
		}
		str = realloc(str, *bufsize+bufinc);
		if (str == NULL)
			return -1;
		*bufsize += bufinc;
		ret = vsnprintf(str, *bufsize, fmt, ap);
	}
	return *bufsize;
}

char *mkhtml(const char *dir, const char *uri)
{
	struct dirent *dentry;
	size_t bufsize = BUFFSIZE;
	char path[PATH_MAX], *dir_end;
	char *html = malloc(sizeof(*html) * bufsize);
	struct stat sb;
	DIR *dirp;

	if ((dirp = opendir(dir)) == NULL)
		return NULL;

	/* dir_end holds the end of the dir */
	dir_end = stpcpy(path, dir);
	/* replace '\0' with '/' for concatenating the filename */
	*dir_end = '/';
	*++dir_end = '\0';

	/* Set to empty string; without it, there may be garbage in the beginning */
	*html = '\0';
	appendstr(html, &bufsize, BUFFSIZEINC, "<!doctype html>\n"
			"<head>\n"
			"<title>Index of %s</title>\n"
			"</head>\n"
			"<body>\n"
			"<h1>Index of %s</h1>\n", dir, dir);
	appendstr(html, &bufsize, BUFFSIZEINC,
			"<table>\n"
			"<tr>\n"
			"<th>Name</th>\n"
			"<th>Size</th>\n"
			"</tr>");
	errno = 0;
	while ((dentry = readdir(dirp)) != NULL) {
		/* do not show hidden files */
		if (strcmp(dentry->d_name, ".")) {
			char *fname = dentry->d_name;
			strcat(path, fname);
			if (stat(path, &sb) < 0) {
				perror("stat");
				continue;
			}
			char fsize[64] = "";
			if (!S_ISDIR(sb.st_mode))
				sprintf(fsize, "%ld", sb.st_size);
			/* add URI in href; without it will try to access from root */
			int ret = appendstr(html, &bufsize, BUFFSIZEINC,
				"<tr>\n"
				"<td><a href=%s%s%s>%s%s</a></td>\n"
				"<td>%s</td>\n"
				"</tr>\n",
				uri, fname, S_ISDIR(sb.st_mode) ? "/" : "",
				fname, S_ISDIR(sb.st_mode) ? "/" : "", fsize);
			if (ret == -1) {
				fprintf(stderr, "realloc failed: out of memory\n");
				free(html);
				return NULL;
			}
		}
		*dir_end = '\0';
	}
	appendstr(html, &bufsize, BUFFSIZEINC, "</body>\n</html>");
	return html;
}

void get_filetype(const char *filename, char *filetype, size_t size)
{
	if (strcasestr(filename, ".html"))
		strncpy(filetype, "text/html", size);
	else if (strcasestr(filename, ".gif"))
		strncpy(filetype, "image/gif", size);
	else if (strcasestr(filename, ".jpg") || strcasestr(filename, ".jpeg"))
		strncpy(filetype, "image/jpeg", size);
	else if (strcasestr(filename, ".mpg"))
		strncpy(filetype, "video/mpg", size);
	else if (strcasestr(filename, ".mpeg"))
		strncpy(filetype, "video/mpeg", size);
	else
		strncpy(filetype, "text/plain", size);
}
