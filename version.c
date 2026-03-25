// handles the downloading of stuff,

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>

#include "utils.h"
#include "download.h"

void list_available_versions(cJSON *json, char *vertype) {
    
    cJSON *versions = cJSON_GetObjectItem(json, "versions");
    size_t size = cJSON_GetArraySize(versions);
    
    for (int i = size; i >= 0; i--) {
        cJSON *ver = cJSON_GetArrayItem(versions, i);
        cJSON *id = cJSON_GetObjectItem(ver, "id");
        cJSON *type = cJSON_GetObjectItem(ver, "type");
        if (strcmp(vertype, type->valuestring)  == 0) {
            printf("%-30s %s\n", id->valuestring,type->valuestring );
        }
    }
}

void download_version_json(cJSON *json, char *id_c) {
    
// okay so we get the json, iterate through the array and strcmp the id, 
// if it matches, we break out of the loop

    cJSON *versions = cJSON_GetObjectItem(json, "versions");
    cJSON *ver;
    char *version_url = NULL;
    cJSON_ArrayForEach(ver, versions) {
        cJSON *id_j = cJSON_GetObjectItem(ver, "id");
        cJSON *url = cJSON_GetObjectItem(ver, "url");
        if (strcmp(id_j->valuestring, id_c) == 0) {
            version_url = url->valuestring;
            break;
        } else {
            printf("No such version found!\n");
            return;
        }
    }

    char dest_path[256];
    snprintf(dest_path, sizeof(dest_path), MINECRAFT_PATH "/versions/%s/%s.json", id_c, id_c);
    download_file(version_url, dest_path);
}

void download_libraries(cJSON *version_json) {
    cJSON *libraries = cJSON_GetObjectItem(version_json, "libraries");
    cJSON *library;

    size_t libcount = cJSON_GetArraySize(libraries);
    
    cJSON_ArrayForEach(library, libraries) {
        if (is_allowed_on_linux(library) == 0) { continue; }
        cJSON *downloads = cJSON_GetObjectItem(library, "downloads");
        cJSON *artifact = cJSON_GetObjectItem(downloads, "artifact");

        cJSON *path = cJSON_GetObjectItem(artifact, "path");
        cJSON *url = cJSON_GetObjectItem(artifact, "url");

    }
}
