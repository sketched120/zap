#ifndef UTILS_H
#define UTILS_H

#define MINECRAFT_PATH "/home/sketched/.minecraft"
#include <cjson/cJSON.h>

int is_allowed_on_linux(cJSON *library);
char *read_file(char *path);
char *get_asset_index(cJSON *json);

#endif