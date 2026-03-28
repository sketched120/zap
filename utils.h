#ifndef UTILS_H
#define UTILS_H

#define MINECRAFT_PATH "/home/sketched/.minecraftz"
#include <cjson/cJSON.h>
//#include <stdbool.h>

bool is_allowed_on_linux(cJSON *library);
char *read_file(char *path);
char *get_jar_path(char *libname);
char *get_asset_index(cJSON *json);
bool file_exists(char *file);
char *build_classpath(cJSON *version_json);
void list_installed(void);


#endif