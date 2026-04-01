#ifndef JVM_ARGS_H
#define JVM_ARGS_H

#include <cjson/cJSON.h>

typedef struct {
    const char *version;
    const char *natives_dir;
    const char *classpath;
    const char *assets_dir;
    const char *asset_index;
    const char *game_dir;
    const char *username;
    const char *uuid;
    const char *access_token;
} LaunchContext;

char **build_jvm_args(cJSON *version_json, LaunchContext *ctx);
char **build_game_args(cJSON *version_json, LaunchContext *ctx);


#endif