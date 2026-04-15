#include <cjson/cJSON.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "include/download.h"
#include "include/fabric.h"
#include "include/utils.h"
#include "include/version.h"

#ifndef FABRIC_MANIFEST_LINK
#define FABRIC_MANIFEST_LINK "https://meta.fabricmc.net/v2/versions/loader"
#endif

void launch_loader_handler(cJSON *child_json) {

  cJSON *inherits_from = cJSON_GetObjectItem(child_json, "inheritsFrom");
  nullchk(inherits_from);

  char *version = inherits_from->valuestring;

  char parent_json_path[BUF_MID];
  snprintf(parent_json_path, sizeof(parent_json_path), "versions/%s/%s.json",
           version, version);

  char *parent_buf = read_file(parent_json_path);
  if (!parent_buf) {
    printlog("ERROR", __func__,
             "Failed to find parent version JSON! Install the parent version "
             "before launching a Fabric instance for the first time.");
    exit(1);
  }

  cJSON *parent_json = cJSON_Parse(parent_buf);
  if (!parent_json) {
    printlog("ERROR", __func__,
             "Failed to parse parent JSON! Is it corrupted?");
    return;
  }

  cJSON_AddItemReferenceToObject(
      child_json, "assetIndex", cJSON_GetObjectItem(parent_json, "assetIndex"));
  cJSON_AddItemReferenceToObject(
      child_json, "javaVersion",
      cJSON_GetObjectItem(parent_json, "javaVersion"));
  cJSON_AddItemReferenceToObject(child_json, "downloads",
                                 cJSON_GetObjectItem(parent_json, "downloads"));

  cJSON *vanilla_libs = cJSON_GetObjectItem(parent_json, "libraries");
  cJSON *fabric_libs = cJSON_GetObjectItem(child_json, "libraries");
  download_libraries(vanilla_libs);
  cJSON *lib;
  cJSON_ArrayForEach(lib, vanilla_libs)
      cJSON_AddItemReferenceToArray(fabric_libs, lib);

  cJSON *parent_args = cJSON_GetObjectItem(parent_json, "arguments");
  cJSON *child_args = cJSON_GetObjectItem(child_json, "arguments");
  if (parent_args && child_args) {
    cJSON *arg;
    cJSON *parent_jvm = cJSON_GetObjectItem(parent_args, "jvm");
    cJSON *child_jvm = cJSON_GetObjectItem(child_args, "jvm");
    if (parent_jvm && child_jvm)
      cJSON_ArrayForEach(arg, parent_jvm)
          cJSON_AddItemReferenceToArray(child_jvm, arg);

    cJSON *parent_game = cJSON_GetObjectItem(parent_args, "game");
    cJSON *child_game = cJSON_GetObjectItem(child_args, "game");
    if (parent_game && child_game)
      cJSON_ArrayForEach(arg, parent_game)
          cJSON_AddItemReferenceToArray(child_game, arg);
  }

  free(parent_buf);
}

void download_fabric_libraries(cJSON *json) {
  cJSON *libraries = cJSON_GetObjectItem(json, "libraries");

  int start = 0;
  size_t libcount = (size_t)cJSON_GetArraySize(libraries);

  char **urls = malloc(libcount * sizeof(char *));
  char **dests = malloc(libcount * sizeof(char *));

  cJSON *library;
  cJSON_ArrayForEach(library, libraries) {

    cJSON *mvn_name = cJSON_GetObjectItem(library, "name");
    cJSON *mvn_url = cJSON_GetObjectItem(library, "url");

    if (!mvn_name || !mvn_url) {
      printlog("ERROR", __func__, "Maven name/URL is NULL!");
      return;
    }

    char *path = get_jar_path(mvn_name->valuestring);
    char url[BUF_LARGE];
    snprintf(url, sizeof(url), "%s%s", mvn_url->valuestring, path);

    size_t len = strlen(minecraft_path) + strlen(path) + 50;
    dests[start] = malloc(len);
    snprintf(dests[start], len, "libraries/%s", path);
    urls[start] = strdup(url);
    start++;
  }

  download_files(urls, dests, start);

  for (int i = 0; i < start; i++) {
    free(dests[i]);
    free(urls[i]);
  }
  free(urls);
  free(dests);
}

static void download_main_manifest(char *mc_version) {

  char fabric_url[BUF_MID];
  char tmp_path[64];

  snprintf(fabric_url, sizeof(fabric_url), FABRIC_MANIFEST_LINK "/%s",
           mc_version);
  snprintf(tmp_path, sizeof(tmp_path), "tnt/fabric_temp_%s.json",
           mc_version);

  if (file_exists(tmp_path))
    unlink(tmp_path);

  int status = download_file(fabric_url, tmp_path);
  if (status == 1) {
    printlog("ERROR", __func__, "Failed to download manifest!");
    unlink(tmp_path);
    return;
  }
}
void list_fabric_versions(char *mc_version) {

  download_main_manifest(mc_version);
  char tmp_path[64];

  snprintf(tmp_path, sizeof(tmp_path), "tnt/fabric_temp_%s.json",
           mc_version);

  char *temp_buf = read_file(tmp_path);
  if (!temp_buf) {
    printlog("ERROR", __func__, "Failed to read %s!", tmp_path);
    return;
  }
  cJSON *json = cJSON_Parse(temp_buf);
  if (!json) {
    printlog("ERROR", __func__, "Failed to parse JSON! Is it corrupted?");
    free(temp_buf);
    return;
  }

  int count = cJSON_GetArraySize(json);
  for (int i = count - 1; i >= 0; i--) {
    cJSON *item = cJSON_GetArrayItem(json, i);
    cJSON *loader = cJSON_GetObjectItem(item, "loader");
    cJSON *version = cJSON_GetObjectItem(loader, "version");
    printlog("INFO", __func__,"%-30s %s\n", version->valuestring, mc_version);
  }
  cJSON_Delete(json);
  free(temp_buf);
}

char *get_latest_loader(char *mc_version) {
  download_main_manifest(mc_version);
  char tmp_path[64];

  snprintf(tmp_path, sizeof(tmp_path), "tnt/fabric_temp_%s.json",
           mc_version);

  char *temp_buf = read_file(tmp_path);
  cJSON *json = cJSON_Parse(temp_buf);
  if (!json) {
    free(temp_buf);
    return NULL;
  }

  cJSON *first = cJSON_GetArrayItem(json, 0);
  if (!first) {
    printf("%s %d", __func__ , __LINE__);
    return NULL;
  }
  cJSON *loader = cJSON_GetObjectItem(first, "loader");
  if (!loader) {
    printf("%s %d", __func__ , __LINE__);
    return NULL;
  }
  cJSON *version = cJSON_GetObjectItem(loader, "version");
  if (!version) {
    printf("%s %d", __func__ , __LINE__);
    return NULL;
  }

  char *out = strdup(version->valuestring);
  cJSON_Delete(json);
  free(temp_buf);
  return out;
}

void download_fabric_manifest(char *mc_version, char *loader_version) {
  printlog("INFO", __func__,"Downloading Fabric loader %s manifest for %s...\n", loader_version,
         mc_version);

  char manifest_path[BUF_MID];
  snprintf(manifest_path, sizeof(manifest_path),
           "versions/fabric-loader-%s-%s/fabric-loader-%s-%s.json",
           loader_version, mc_version, loader_version, mc_version);

  char url_path[256];
  snprintf(url_path, sizeof(url_path), "%s/%s/%s/profile/json",
           FABRIC_MANIFEST_LINK, mc_version, loader_version);

  int status = download_file(url_path, manifest_path); 
  if (status == 1) {
    printlog("ERROR", __func__,"Failed to download Fabric loader manifest for %s!", mc_version);
    return;
  }
}


