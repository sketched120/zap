#ifndef UTILS_H
#define UTILS_H

#define MINECRAFT_PATH "/home/sketched/.minecraftz"
#define nullchk(x) if (!(x)) { fprintf(stderr, "null check failed: %s at %s:%d\n", #x, __FILE__, __LINE__); return; }
#define nullchkr(x, r) if (!(x)) { fprintf(stderr, "null check failed: %s at %s:%d\n", #x, __FILE__, __LINE__); return r; }
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