#include <cjson/cJSON.h>
#include <stdio.h>
#include <string.h>
#include <zip.h>

#include "include/utils.h"
#include <sys/stat.h>

void extract_natives(char *jar, char *dest) {
    zip_t *zip = zip_open(jar, 0, NULL);
    if (!zip) {
        printf("Failed to open jar %s.\n", jar);
        return;
    }
    mkdir(dest, 0755);
    int cnt = zip_get_num_entries(zip, 0);

    for (int i = 0; i < cnt; i++) {
        const char *name = zip_get_name(zip, i, 0);

        if (!strstr(name, ".so")) continue;

        const char *filename = strrchr(name, '/');
        if (filename)
            filename = filename + 1;
        else
            filename = name;

        char dest_path[512];
        snprintf(dest_path, sizeof(dest_path), "%s/%s", dest, filename);

        if (file_exists(dest_path)) continue;

        printf("Extracting native: %s\n", name);

        zip_file_t *file = zip_fopen_index(zip, i, 0);
        if (!file) { printf("zip_fopen_index failed\n"); continue; }

        FILE *out = fopen(dest_path, "wb");
        if (!out) { printf("fopen failed for %s\n", dest_path); continue; }

        char buf[4096];
        zip_int64_t bytes;

        while ((bytes = zip_fread(file, buf, sizeof(buf))) > 0)
            fwrite(buf, 1, bytes, out);

        fclose(out);
        zip_fclose(file);
    }

    zip_close(zip);
}

void extract_wrapper(cJSON *json) {
    cJSON *id        = cJSON_GetObjectItem(json, "id");
    cJSON *libraries = cJSON_GetObjectItem(json, "libraries");
    cJSON *lib;

    char natives_dir[256];
    snprintf(natives_dir, sizeof(natives_dir), MINECRAFT_PATH "/natives/%s", id->valuestring);
    mkdir(natives_dir, 0755);

    cJSON_ArrayForEach(lib, libraries) {
        if (!is_allowed_on_linux(lib)) continue;

        cJSON *downloads = cJSON_GetObjectItem(lib, "downloads");
        if (!downloads) continue;

        char jar[512];
        bool found = false;

        cJSON *classifiers = cJSON_GetObjectItem(downloads, "classifiers");
        if (classifiers) {
            cJSON *natives_linux = cJSON_GetObjectItem(classifiers, "natives-linux");
            if (natives_linux) {
                cJSON *path = cJSON_GetObjectItem(natives_linux, "path");
                if (path) {
                    snprintf(jar, sizeof(jar), MINECRAFT_PATH "/libraries/%s", path->valuestring);
                    found = true;
                }
            }
        }

        if (!found) {
            cJSON *name = cJSON_GetObjectItem(lib, "name");
            if (!name || !strstr(name->valuestring, "natives-linux")) continue;
            cJSON *artifact = cJSON_GetObjectItem(downloads, "artifact");
            if (!artifact) continue;
            cJSON *path = cJSON_GetObjectItem(artifact, "path");
            if (!path) continue;
            snprintf(jar, sizeof(jar), MINECRAFT_PATH "/libraries/%s", path->valuestring);
            found = true;
        }

        if (!found) continue;
        extract_natives(jar, natives_dir);
    }
}
