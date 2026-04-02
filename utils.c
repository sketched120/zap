#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <cjson/cJSON.h>

#include "include/utils.h"

int streq(const char *a, const char *b) {
    return strcmp(a, b) == 0;
}

bool is_allowed_on_linux(cJSON *library) {
    cJSON *rules = cJSON_GetObjectItem(library, "rules");
    if (!rules) return true;

    bool allowed = false;
    cJSON *rule;
    cJSON_ArrayForEach(rule, rules) {
        cJSON *action = cJSON_GetObjectItem(rule, "action");
        cJSON *os     = cJSON_GetObjectItem(rule, "os");

        if (strcmp(action->valuestring, "allow") == 0) {
            if (!os) {
                allowed = true;
            } else {
                cJSON *name = cJSON_GetObjectItem(os, "name");
                if (name && strcmp(name->valuestring, "linux") == 0)
                    allowed = true;
            }
        } else if (strcmp(action->valuestring, "disallow") == 0) {
            if (os) {
                cJSON *name = cJSON_GetObjectItem(os, "name");
                if (name && strcmp(name->valuestring, "linux") == 0)
                    allowed = false;
            }
        }
    }
    return allowed;
}

char *read_file(char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "Failed to read file: %s\n", path);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long bufsize = ftell(file);
    if (bufsize < 0) { fclose(file); return NULL; }
    rewind(file);

    char *buffer = malloc((size_t)bufsize + 1);
    if (!buffer) { fclose(file); return NULL; }
    fread(buffer, 1, (size_t)bufsize, file);
    buffer[bufsize] = '\0';

    fclose(file);
    return buffer;
}

char *get_jar_path(char *libname) {
    char namecp[256];
    snprintf(namecp, sizeof(namecp), "%s", libname);

    char *group      = strtok(namecp, ":");
    char *artifact   = strtok(NULL, ":");
    char *version    = strtok(NULL, ":");
    char *classifier = strtok(NULL, ":");

    if (!group || !artifact || !version) return NULL;

    for (int i = 0; group[i]; i++)
        if (group[i] == '.') group[i] = '/';

    char *out = malloc(1024);
    nullchkr(out, NULL);

    if (classifier)
        snprintf(out, 1024, "%s/%s/%s/%s-%s-%s.jar", group, artifact, version, artifact, version, classifier);
    else
        snprintf(out, 1024, "%s/%s/%s/%s-%s.jar", group, artifact, version, artifact, version);

    return out;
}

char *get_asset_index(cJSON *json) {
    cJSON *assetIndex = cJSON_GetObjectItem(json, "assetIndex");
    cJSON *id         = cJSON_GetObjectItem(assetIndex, "id");
    char *out = malloc(128);
    nullchkr(out, NULL);
    snprintf(out, 128, "%s", id->valuestring);
    return out;
}

void list_installed(void) {
    DIR *d = opendir(MINECRAFT_PATH "/versions");

    nullchk(d);

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        printf("%s\n", entry->d_name);
    }
    closedir(d);
}

char *build_classpath(cJSON *version_json) {
    size_t buf_size = 131072;
    char *classpath = malloc(buf_size);
    nullchkr(classpath, NULL);
    classpath[0] = '\0';

    cJSON *libraries     = cJSON_GetObjectItem(version_json, "libraries");
    cJSON *inherits_from = cJSON_GetObjectItem(version_json, "inheritsFrom");
    cJSON *id            = inherits_from ? inherits_from : cJSON_GetObjectItem(version_json, "id");
    cJSON *lib;
    size_t offset = 0;

    cJSON_ArrayForEach(lib, libraries) {
        if (is_allowed_on_linux(lib) == false) continue;

        if (offset + 1024 > buf_size) {
            buf_size *= 2;
            char *tmp = realloc(classpath, buf_size);
            if (!tmp) { free(classpath); return NULL; }
            classpath = tmp;
        }

        cJSON *downloads = cJSON_GetObjectItem(lib, "downloads");
        if (!downloads) {
            cJSON *maven_url = cJSON_GetObjectItem(lib, "url");
            if (!maven_url) continue;
            cJSON *name = cJSON_GetObjectItem(lib, "name");
            if (!name) continue;
            char *jarpath = get_jar_path(name->valuestring);
            if (!jarpath) continue;
            offset += (size_t)snprintf(classpath + offset, buf_size - offset,
                                       MINECRAFT_PATH "/libraries/%s:", jarpath);
            free(jarpath);
            continue;
        }

        cJSON *artifact = cJSON_GetObjectItem(downloads, "artifact");
        if (artifact) {
            cJSON *path = cJSON_GetObjectItem(artifact, "path");
            offset += (size_t)snprintf(classpath + offset, buf_size - offset,
                                       MINECRAFT_PATH "/libraries/%s:", path->valuestring);
        }

        cJSON *classifiers = cJSON_GetObjectItem(downloads, "classifiers");
        if (classifiers) {
            cJSON *natives_linux = cJSON_GetObjectItem(classifiers, "natives-linux");
            if (natives_linux) {
                cJSON *path = cJSON_GetObjectItem(natives_linux, "path");
                offset += (size_t)snprintf(classpath + offset, buf_size - offset,
                                           MINECRAFT_PATH "/libraries/%s:", path->valuestring);
            }
        }
    }

    if (offset + 512 > buf_size) {
        buf_size += 512;
        char *tmp = realloc(classpath, buf_size);
        if (!tmp) { free(classpath); return NULL; }
        classpath = tmp;
    }

    snprintf(classpath + offset, buf_size - offset,
             MINECRAFT_PATH "/versions/%s/%s.jar",
             id->valuestring, id->valuestring);

    return classpath;
}

bool file_exists(char *file) {
    struct stat f_inf;
    if (stat(file, &f_inf) == 0) {
        return true;
    } else return false;
}
