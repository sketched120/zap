#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>

#include "utils.h"
#include "download.h"
#include "fabric.h"
#include "version.h"

#ifndef FABRIC_MANIFEST_LINK
#define FABRIC_MANIFEST_LINK "https://meta.fabricmc.net/v2/versions/loader/"
#endif

void launch_loader_handler(cJSON *child_json)
{
    cJSON *inherits_from = cJSON_GetObjectItem(child_json, "inheritsFrom");
    char *version = inherits_from->valuestring;

    char parent_json_path[256];
    snprintf(parent_json_path, sizeof(parent_json_path),
             MINECRAFT_PATH "/versions/%s/%s.json", version, version);

    char *parent_buf   = read_file(parent_json_path);
    cJSON *parent_json = cJSON_Parse(parent_buf);

    cJSON_AddItemReferenceToObject(child_json, "assetIndex",  cJSON_GetObjectItem(parent_json, "assetIndex"));
    cJSON_AddItemReferenceToObject(child_json, "javaVersion", cJSON_GetObjectItem(parent_json, "javaVersion"));
    cJSON_AddItemReferenceToObject(child_json, "downloads",   cJSON_GetObjectItem(parent_json, "downloads"));

    cJSON *vanilla_libs = cJSON_GetObjectItem(parent_json, "libraries");
    cJSON *fabric_libs  = cJSON_GetObjectItem(child_json,  "libraries");
    download_libraries(vanilla_libs);
    cJSON *lib;
    cJSON_ArrayForEach(lib, vanilla_libs)
        cJSON_AddItemReferenceToArray(fabric_libs, lib);

    cJSON *parent_args = cJSON_GetObjectItem(parent_json, "arguments");
    cJSON *child_args  = cJSON_GetObjectItem(child_json,  "arguments");
    if (parent_args && child_args) {
        cJSON *arg;
        cJSON *parent_jvm = cJSON_GetObjectItem(parent_args, "jvm");
        cJSON *child_jvm  = cJSON_GetObjectItem(child_args,  "jvm");
        if (parent_jvm && child_jvm)
            cJSON_ArrayForEach(arg, parent_jvm)
                cJSON_AddItemReferenceToArray(child_jvm, arg);

        cJSON *parent_game = cJSON_GetObjectItem(parent_args, "game");
        cJSON *child_game  = cJSON_GetObjectItem(child_args,  "game");
        if (parent_game && child_game)
            cJSON_ArrayForEach(arg, parent_game)
                cJSON_AddItemReferenceToArray(child_game, arg);
    }

    free(parent_buf);
    // do NOT cJSON_Delete(parent_json) — child holds references into it
}

void download_fabric_libraries(cJSON *json) {
    cJSON *libraries = cJSON_GetObjectItem(json, "libraries");
    download_libraries(libraries);
    /*cJSON *lib;
    cJSON_ArrayForEach(lib, libraries) {
        cJSON *name = cJSON_GetObjectItem(lib, "name");
        cJSON *url  = cJSON_GetObjectItem(lib, "url");

        if (!url || !url->valuestring) continue;

        char *libname = get_jar_path(name->valuestring);
        if (!libname) continue;

        size_t len = strlen(url->valuestring);
        if (len > 0 && url->valuestring[len - 1] == '/')
            url->valuestring[len - 1] = '\0';

        char libpath[512];
        char urlstring[512];
        snprintf(urlstring, sizeof(urlstring), "%s/%s", url->valuestring, libname);
        snprintf(libpath,   sizeof(libpath),   MINECRAFT_PATH "/libraries/%s", libname);

        if (file_exists(libpath)) {
            printf("downloading %s from %s\n", name->valuestring, urlstring);
            download_file(urlstring, libpath);
        }
        free(libname);
    }*/
}

void list_fabric_versions(char *req_mc_version)
{
    char fabric_url[128];
    char tmp_path[64];

    snprintf(fabric_url, sizeof(fabric_url), FABRIC_MANIFEST_LINK "%s", req_mc_version);
    snprintf(tmp_path,   sizeof(tmp_path),   "/tmp/tnt/fabric_temp_%s.json", req_mc_version);

    int status = download_file(fabric_url, tmp_path);
    if (status == 1) {
        fprintf(stderr, "failed to download manifest, does that version exist?\n");
        return;
        }
    

    char *temp_buf = read_file(tmp_path);
    cJSON *json = cJSON_Parse(temp_buf);
    if (!json) {
        fprintf(stderr, "failed to parse json\n");
        free(temp_buf);
        return;
    }

    int count = cJSON_GetArraySize(json);
    for (int i = count - 1; i >= 0; i--) {
        cJSON *item    = cJSON_GetArrayItem(json, i);
        cJSON *loader  = cJSON_GetObjectItem(item, "loader");
        cJSON *version = cJSON_GetObjectItem(loader, "version");
        printf("%-30s %s\n", version->valuestring, req_mc_version);
        
    }
    cJSON_Delete(json);
    free(temp_buf);
}

char *get_latest_fabric_loader(char *req_mc_version)
{
    char fabric_url[128];
    char tmp_path[64];

    snprintf(fabric_url, sizeof(fabric_url), FABRIC_MANIFEST_LINK "%s", req_mc_version);
    snprintf(tmp_path,   sizeof(tmp_path),   "/tmp/tnt/fabric_temp_%s.json", req_mc_version);

    
        int status = download_file(fabric_url, tmp_path);
        if (status == 1) {
            fprintf(stderr, "failed to download manifest\n");
            return NULL;
        }
    

    char *temp_buf = read_file(tmp_path);
    cJSON *json = cJSON_Parse(temp_buf);
    if (!json) { free(temp_buf); return NULL; }

    cJSON *first   = cJSON_GetArrayItem(json, 0);
    cJSON *loader  = cJSON_GetObjectItem(first, "loader");
    cJSON *version = cJSON_GetObjectItem(loader, "version");

    char *out = strdup(version->valuestring);
    cJSON_Delete(json);
    free(temp_buf);
    return out; // caller must free()
}

void download_fabric_manifest(char *req_mc_version, char *req_loader_version)
{
    printf("downloading fabric loader %s manifest for %s...\n", req_loader_version, req_mc_version);

    char manifest_path[256];
    snprintf(manifest_path, sizeof(manifest_path),
             MINECRAFT_PATH "/versions/fabric-loader-%s-%s/fabric-loader-%s-%s.json",
             req_loader_version, req_mc_version,
             req_loader_version, req_mc_version);

    char url_path[256];
    snprintf(url_path, sizeof(url_path),
             "https://meta.fabricmc.net/v2/versions/loader/%s/%s/profile/json",
             req_mc_version, req_loader_version);

    download_file(url_path, manifest_path);
}
