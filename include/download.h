#ifndef DOWNLOAD_H
#define DOWNLOAD_H

void download_files(char **urls, char **dests, int n);
int download_file(const char *url, char *dest);

#endif