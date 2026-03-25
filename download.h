#ifndef DOWNLOAD_H
#define DOWNLOAD_H

void download_files(const char **urls, const char **dests, int n);
void download_file(const char *url, const char *dest);

#endif