#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>

#include "utils.h"
#include "jvm_args.h"

static char *resolve_var(const char *arg, LaunchContext *ctx) {
    size_t buf_size = strlen(ctx->classpath) + 8192;
    char *out = malloc(buf_size);
    int i = 0, j = 0;

    while (arg[i] != '\0') {
        if (arg[i] == '$' && arg[i+1] == '{') {
            i += 2;
            char key[128];
            int k = 0;
            while (arg[i] != '}' && arg[i] != '\0')
                key[k++] = arg[i++];
            key[k] = '\0';
            i++;

            const char *val = NULL;
            if      (strcmp(key, "natives_directory")  == 0) val = ctx->natives_dir;
            else if (strcmp(key, "classpath")          == 0) val = ctx->classpath;
            else if (strcmp(key, "version_name")       == 0) val = ctx->version;
            else if (strcmp(key, "assets_root")        == 0) val = ctx->assets_dir;
            else if (strcmp(key, "assets_index_name")  == 0) val = ctx->asset_index;
            else if (strcmp(key, "game_directory")     == 0) val = ctx->game_dir;
            else if (strcmp(key, "auth_player_name")   == 0) val = ctx->username;
            else if (strcmp(key, "auth_uuid")          == 0) val = ctx->uuid;
            else if (strcmp(key, "auth_access_token")  == 0) val = ctx->access_token;
            else if (strcmp(key, "launcher_name")      == 0) val = "tnt";
            else if (strcmp(key, "launcher_version")   == 0) val = "alpha";
            else if (strcmp(key, "user_type")          == 0) val = "mojang";
            else if (strcmp(key, "version_type")       == 0) val = "release";

            if (val) {
                size_t vlen = strlen(val);
                if ((size_t)j + vlen + 1 >= buf_size) {
                    buf_size = (size_t)j + vlen + 8192;
                    out = realloc(out, buf_size);
                }
                while (*val)
                    out[j++] = *val++;
            }
        } else {
            out[j++] = arg[i++];
        }
    }
    out[j] = '\0';
    return out;
}

char **build_jvm_args(cJSON *version_json, LaunchContext *ctx) {
    char **args = malloc(sizeof(char *) * 256);
    int idx = 0;

    cJSON *arguments = cJSON_GetObjectItem(version_json, "arguments");
    if (arguments) {
        cJSON *jvm = cJSON_GetObjectItem(arguments, "jvm");
        cJSON *entry;
        cJSON_ArrayForEach(entry, jvm) {
            if (cJSON_IsString(entry)) {
                args[idx++] = resolve_var(entry->valuestring, ctx);
            } else if (cJSON_IsObject(entry)) {
                if (!is_allowed_on_linux(entry)) continue;
                cJSON *value = cJSON_GetObjectItem(entry, "value");
                if (cJSON_IsString(value)) {
                    args[idx++] = resolve_var(value->valuestring, ctx);
                } else if (cJSON_IsArray(value)) {
                    cJSON *v;
                    cJSON_ArrayForEach(v, value)
                        args[idx++] = resolve_var(v->valuestring, ctx);
                }
            }
        }
    } else {
        char libpath[256];
        snprintf(libpath, sizeof(libpath), "-Djava.library.path=%s", ctx->natives_dir);
        args[idx++] = strdup(libpath);
        args[idx++] = strdup("-cp");
        args[idx++] = resolve_var("${classpath}", ctx);
    }

    args[idx] = NULL;
    return args;
}

char **build_game_args(cJSON *version_json, LaunchContext *ctx) {
    char **args = malloc(sizeof(char *) * 256);
    int idx = 0;

    cJSON *arguments = cJSON_GetObjectItem(version_json, "arguments");
    if (arguments) {
        cJSON *game = cJSON_GetObjectItem(arguments, "game");
        cJSON *entry;
        cJSON_ArrayForEach(entry, game) {
            if (cJSON_IsString(entry)) {
                args[idx++] = resolve_var(entry->valuestring, ctx);
            } else if (cJSON_IsObject(entry)) {
                cJSON *rules    = cJSON_GetObjectItem(entry, "rules");
                cJSON *rule     = cJSON_GetArrayItem(rules, 0);
                cJSON *features = cJSON_GetObjectItem(rule, "features");
                if (features) continue;
            }
        }
    } else {
        cJSON *old_args = cJSON_GetObjectItem(version_json, "minecraftArguments");
        if (old_args) {
            char buf[2048];
            strncpy(buf, old_args->valuestring, sizeof(buf));
            char *token = strtok(buf, " ");
            while (token) {
                args[idx++] = resolve_var(token, ctx);
                token = strtok(NULL, " ");
            }
        }
    }

    args[idx] = NULL;
    return args;
}
