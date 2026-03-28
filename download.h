#ifndef DOWNLOAD_H
#define DOWNLOAD_H

void mkdirs(const char *path);
void download_files(char **urls, char **dests, int n);
int download_file(char *url, char *dest);

#endif