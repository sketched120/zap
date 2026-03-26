#ifndef DOWNLOAD_H
#define DOWNLOAD_H

void download_files(char **urls, char **dests, int n);
void download_file(char *url, char *dest);

#endif