#ifndef VERSION_H
#define VERSION_H

#include <cjson/cJSON.h>

int list_available_versions(char *vertype);
int download_libraries(cJSON *libraries);
int download_version(char *req_v);

#endif