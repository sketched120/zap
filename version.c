// handles the downloading of stuff,

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>

#include "utils.h"
#include "download.h"

void list_available_versions(cJSON *manifest_json, char *vertype) {
    
    cJSON *versions = cJSON_GetObjectItem(manifest_json, "versions");
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

void download_version_json(cJSON *manifest_json, char *id_c) {
    
// okay so we get the json, iterate through the array and strcmp the id, 
// if it matches, we break out of the loop

    cJSON *versions = cJSON_GetObjectItem(manifest_json, "versions");
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

    int start = 0;
    size_t libcount = cJSON_GetArraySize(libraries);

    char **urls = malloc(libcount * sizeof(char *));
    char **dests = malloc(libcount * sizeof(char *));
    
    cJSON_ArrayForEach(library, libraries) {
        if (is_allowed_on_linux(library) == 0) { continue; }
        cJSON *downloads = cJSON_GetObjectItem(library, "downloads");
        cJSON *artifact = cJSON_GetObjectItem(downloads, "artifact");

        urls[start] = cJSON_GetObjectItem(artifact, "url")->valuestring;
        dests[start] = cJSON_GetObjectItem(artifact, "path")->valuestring;
        start++;
    }

    download_files(urls, dests, libcount);

    free(urls);
    free(dests);


}

void download_client(cJSON *version_json) {
    
    cJSON *id = cJSON_GetObjectItem(version_json, "id");

    cJSON *downloads = cJSON_GetObjectItem(version_json, "downloads");
    cJSON *client = cJSON_GetObjectItem(downloads, "client");
    cJSON *cl_url = cJSON_GetObjectItem(client, "url");

    char cl_dest[256];
    snprintf(cl_dest, sizeof(cl_dest), MINECRAFT_PATH"/versions/%s/%s.jar", id->valuestring, id->valuestring);
    download_file(cl_url->valuestring, cl_dest );

}

void download_assets(cJSON *version_json) {

    char *index_id = get_asset_index(version_json); // free this
    cJSON *index_details = cJSON_GetObjectItem(version_json, "assetIndex");
    cJSON *index_url = cJSON_GetObjectItem(index_details, "url");

    char dest_path[256];
    snprintf(dest_path, sizeof(dest_path), MINECRAFT_PATH "/versions/indexes/%s.json", index_id);

    download_file(index_url->valuestring, dest_path);

    free(index_id);

    char *index_buf = read_file(dest_path);
    cJSON *asset_index = cJSON_Parse(index_buf);

    cJSON *objects = cJSON_GetObjectItem(asset_index, "objects");

    int start = 0;
    size_t a_count = cJSON_GetArraySize(objects);

    char **urls = malloc(a_count * (sizeof(char *)));
    char **dests = malloc(a_count * (sizeof(char *)));


    char *res_url = "https://resources.download.minecraft.net";

    cJSON *object = objects->child;

    while(object) {
        cJSON *hash = cJSON_GetObjectItem(object, "hash");
        
        char url[256];
        char dest_path[256];

        snprintf(url, sizeof(url), "%s/%.2s/%s", res_url, hash->valuestring, hash->valuestring);
        snprintf(dest_path, sizeof(dest_path), MINECRAFT_PATH "/assets/objects/%.2s/%s", hash->valuestring, hash->valuestring);

        urls[start] = strdup(url);
        dests[start] = strdup(dest_path);
        start++;
    }

    download_files(urls, dests, a_count);

    for (int i = 0; i < a_count; i++) {
        free(urls[i]);
        free(dests[i]);
    }

    free(urls);
    free(dests);

    cJSON_Delete(asset_index);
    free(index_buf);
    

}
