// handles the downloading of stuff,

#include <dirent.h>

#include <cjson/cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "download.h"
#include "utils.h"

void download_version_manifest() {
  download_file("https://piston-meta.mojang.com/mc/game/version_manifest.json",
                MINECRAFT_PATH "/version_manifest.json");
}
void list_available_versions(char *vertype) {

  download_version_manifest();

  char *vm_buf = read_file(MINECRAFT_PATH "/version_manifest.json");
  nullchk(vm_buf);

  cJSON *manifest_json = cJSON_Parse(vm_buf);
  nullchk(manifest_json);

  cJSON *versions = cJSON_GetObjectItem(manifest_json, "versions");
  size_t size = cJSON_GetArraySize(versions);

  for (int i = size - 1; i >= 0; i--) {
    cJSON *ver = cJSON_GetArrayItem(versions, i);
    cJSON *id = cJSON_GetObjectItem(ver, "id");
    cJSON *type = cJSON_GetObjectItem(ver, "type");
    if (strcmp(vertype, type->valuestring) == 0) {
      printf("%-30s %s\n", id->valuestring, type->valuestring);
    }
  }

  cJSON_Delete(manifest_json);
  free(vm_buf);
}

void download_version_json(cJSON *manifest_json, char *id_c) {

  // okay so we get the json, iterate through the array and strcmp the id,
  // if it matches, we break out of the loop
  nullchk(manifest_json);

  cJSON *versions = cJSON_GetObjectItem(manifest_json, "versions");
  cJSON *ver;
  char *version_url = NULL;
  cJSON_ArrayForEach(ver, versions) {
    cJSON *id_j = cJSON_GetObjectItem(ver, "id");
    cJSON *url = cJSON_GetObjectItem(ver, "url");
    if (strcmp(id_j->valuestring, id_c) == 0) {
      version_url = url->valuestring;
      break;
    }
  }

  if (!version_url) {
    fprintf(stderr, "No such version found.");
    return;
  }

  char dest_path[256];
  snprintf(dest_path, sizeof(dest_path), MINECRAFT_PATH "/versions/%s/%s.json",
           id_c, id_c);
  download_file(version_url, dest_path);
}

void download_libraries(cJSON *libraries) {
  cJSON *library;
  int start = 0;
  size_t libcount = cJSON_GetArraySize(libraries);

  char **urls = malloc(libcount * sizeof(char *));
  char **dests = malloc(libcount * sizeof(char *));

  cJSON_ArrayForEach(library, libraries) {
    if (is_allowed_on_linux(library) == false)
      continue;

    cJSON *downloads = cJSON_GetObjectItem(library, "downloads");
    if (!downloads)
      continue;

    // old format classifiers
    cJSON *classifiers = cJSON_GetObjectItem(downloads, "classifiers");
    if (classifiers) {
      cJSON *natives_linux = cJSON_GetObjectItem(classifiers, "natives-linux");

      if (natives_linux) {

        cJSON *path_j = cJSON_GetObjectItem(natives_linux, "path");
        cJSON *url_j = cJSON_GetObjectItem(natives_linux, "url");

        if (!path_j || !url_j) continue;

        char *path = path_j->valuestring;
        char *url = url_j->valuestring;

        size_t len = strlen(MINECRAFT_PATH) + strlen(path) + 50;
        dests[start] = malloc(len);
        snprintf(dests[start], len, MINECRAFT_PATH "/libraries/%s", path);
        urls[start] = url;
        start++;
      }
    }

    // artifact
    cJSON *artifact = cJSON_GetObjectItem(downloads, "artifact");
    if (!artifact)
      continue;

    cJSON *path_j = cJSON_GetObjectItem(artifact, "path");
    cJSON *url_j = cJSON_GetObjectItem(artifact, "url");
    
    if (!path_j || !url_j) continue;

    char *path = path_j->valuestring;
    char *url = url_j->valuestring;

    size_t len = strlen(MINECRAFT_PATH) + strlen(path) + 50;
    dests[start] = malloc(len);
    snprintf(dests[start], len, MINECRAFT_PATH "/libraries/%s", path);

    urls[start] = url;
    start++;
  }

  download_files(urls, dests, start);

  for (int i = 0; i < start; i++)
    free(dests[i]);
  free(urls);
  free(dests);
}

void download_client(cJSON *version_json) {

  nullchk(version_json);
  cJSON *id = cJSON_GetObjectItem(version_json, "id");

  cJSON *downloads = cJSON_GetObjectItem(version_json, "downloads");
  cJSON *client = cJSON_GetObjectItem(downloads, "client");
  cJSON *cl_url = cJSON_GetObjectItem(client, "url");

  char cl_dest[256];

  nullchk(id);
  nullchk(cl_url);
  snprintf(cl_dest, sizeof(cl_dest), MINECRAFT_PATH "/versions/%s/%s.jar",
           id->valuestring, id->valuestring);
  download_file(cl_url->valuestring, cl_dest);
}

void download_assets(cJSON *version_json) {

  nullchk(version_json) 
  char *index_id = get_asset_index(version_json); // free this
  cJSON *index_details = cJSON_GetObjectItem(version_json, "assetIndex");
  cJSON *index_url = cJSON_GetObjectItem(index_details, "url");

  nullchk(index_id);
  nullchk(index_url);

  char dest_path[256];
  snprintf(dest_path, sizeof(dest_path),
           MINECRAFT_PATH "/assets/indexes/%s.json", index_id);

  int status = download_file(index_url->valuestring, dest_path);
  if (status == 1) {
    printf("Failed to download asset index.\n");
    return;
  }

  free(index_id);

  char *index_buf = read_file(dest_path);

  cJSON *asset_index = cJSON_Parse(index_buf);
  nullchk(asset_index);

  cJSON *objects = cJSON_GetObjectItem(asset_index, "objects");
  
  int start = 0;
  size_t a_count = cJSON_GetArraySize(objects);

  char **urls = malloc(a_count * (sizeof(char *)));
  char **dests = malloc(a_count * (sizeof(char *)));

  char *res_url = "https://resources.download.minecraft.net";

  cJSON *object = objects->child;

  while (object) {
    cJSON *hash = cJSON_GetObjectItem(object, "hash");
    nullchk(hash);

    char url[256];
    char dest_path[256];

    snprintf(url, sizeof(url), "%s/%.2s/%s", res_url, hash->valuestring,
             hash->valuestring);
    snprintf(dest_path, sizeof(dest_path),
             MINECRAFT_PATH "/assets/objects/%.2s/%s", hash->valuestring,
             hash->valuestring);

    urls[start] = strdup(url);
    dests[start] = strdup(dest_path);
    start++;

    object = object->next;
  }
  printf("beginning download...\n");
  download_files(urls, dests, start);

  for (int i = 0; i < a_count; i++) {
    free(urls[i]);
    free(dests[i]);
  }

  free(urls);
  free(dests);

  cJSON_Delete(asset_index);
  free(index_buf);
}

void download_version(char *req_v) {
  printf("Downloading version manifest...\n");
  download_version_manifest();

  char *v_man = read_file(MINECRAFT_PATH "/version_manifest.json");
  nullchk(v_man);

  cJSON *v_json = cJSON_Parse(v_man);
  nullchk(v_json);

  printf("Downloading %s.json...\n", req_v);
  download_version_json(v_json, req_v);

  cJSON_Delete(v_json);
  free(v_man);

  char vj_path[256];
  snprintf(vj_path, sizeof(vj_path), MINECRAFT_PATH "/versions/%s/%s.json",
           req_v, req_v);

  char *vj_buf = read_file(vj_path);
  nullchk(vj_buf); 

  cJSON *vj_json = cJSON_Parse(vj_buf);
  nullchk(vj_json);

  cJSON *libraries = cJSON_GetObjectItem(vj_json, "libraries");
  nullchk(libraries);
  
  printf("downloading libraries...\n");
  download_libraries(libraries);

  printf("downloading client...\n");
  download_client(vj_json);

  printf("downloading assets...\n");
  download_assets(vj_json);

  printf("%s successfully installed.", req_v);
  printf("launch with -l %s\n", req_v);

  cJSON_Delete(vj_json);
  free(vj_buf);
}
