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
	int ret;
	int len;

	va_start(ap, fmt);
	len = strlen(str);
	ret = vsnprintf(str+len, *bufsize-len, fmt, ap);
	if (ret >= (*bufsize-len)) {
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

char *mkhtml(const char *uri, const char *dir)
{
	struct dirent *dentry;
	size_t bufsize = BUFFSIZE;
	char *html = malloc(sizeof(*html) * bufsize);
	struct stat sb;
	DIR *dirp;

	if ((dirp = opendir(dir)) == NULL)
		return NULL;
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
	char *currdir = getcwd(NULL, 0);
	while ((dentry = readdir(dirp)) != NULL) {
		if (strcmp(dentry->d_name, ".")) {
			char *fname = dentry->d_name;
			if (stat(fname, &sb) < 0) {
				perror("stat");
				continue;
			}
			int isdir = sb.st_mode & S_IFDIR;
			char fsize[64] = "";
			if (!S_ISDIR(sb.st_mode))
				sprintf(fsize, "%ld", sb.st_size);
			appendstr(html, &bufsize, BUFFSIZEINC,
				"<tr>\n"
				"<td><a href=file://%s/%s>%s</a></td>\n"
				"<td>%s</td>\n"
				"</tr>\n",
				currdir, fname, fname, fsize);
		}
	}
	free(currdir);
	appendstr(html, &bufsize, BUFFSIZEINC, "</body>\n</html>");
	return html;
}

void getfiletype(const char *filename, char *filetype, size_t size)
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
