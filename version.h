#ifndef VERSION_H
#define VERSION_H

#include <cjson/cJSON.h>

void list_available_versions(char *vertype);
void download_libraries(cJSON *libraries);
void download_version(char *req_v);

#endif