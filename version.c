// handles the downloading of stuff,

#include <dirent.h>

#include <cjson/cJSON.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/version.h"
#include "include/download.h"
#include "include/utils.h"

static void download_version_manifest(void) {
  download_file("https://piston-meta.mojang.com/mc/game/version_manifest.json",
                "version_manifest.json");
}
int list_available_versions(char *vertype) {

  download_version_manifest();

  char *vm_buf = read_file("version_manifest.json");
  if (!vm_buf) {
      printlog("ERROR", __func__, "Failed to READ version_manifest.json!");
      return 1;
  }

  cJSON *manifest_json = cJSON_Parse(vm_buf);
  if (!manifest_json) {
      printlog("ERROR", __func__, "Failed to PARSE version_manifest.json, is it corrupted?");
      free(vm_buf);
      return 1;
  }

  free(vm_buf);

  cJSON *versions = cJSON_GetObjectItem(manifest_json, "versions");
  size_t size = (size_t)cJSON_GetArraySize(versions);

  for (size_t i = size; i > 0; i--) {
    cJSON *ver = cJSON_GetArrayItem(versions, (int)(i - 1));
    cJSON *id = cJSON_GetObjectItem(ver, "id");
    cJSON *type = cJSON_GetObjectItem(ver, "type");
    if (strcmp(vertype, type->valuestring) == 0) {
      printf("%-30s %s\n", id->valuestring, type->valuestring);
    }
  }
  cJSON_Delete(manifest_json);
  return 0;
}

static int download_version_json(cJSON *manifest_json, char *id_c) {

  if (!manifest_json) {
      printlog("ERROR", __func__, "Obtained NULL version manifest, aborting!");
      return 1;
  }

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
    printlog("ERROR", __func__, "Specified version was not found in manifest!");
    return 1;
  }

  char dest_path[BUF_MID];
  snprintf(dest_path, sizeof(dest_path), "versions/%s/%s.json",
           id_c, id_c);
  download_file(version_url, dest_path);

  return 0;
}

int download_libraries(cJSON *libraries) {
  cJSON *library;
  int start = 0;
  size_t libcount = (size_t)cJSON_GetArraySize(libraries);

  char **urls = malloc(libcount * sizeof(char *));
  char **dests = malloc(libcount * sizeof(char *));
  if (!urls || !dests) {
      printlog("ERROR", __func__, "malloc failed: %s", strerror(errno));
      return 1;
  }

  cJSON_ArrayForEach(library, libraries) {
    if (is_allowed_on_linux(library) == false)
      continue;

    cJSON *downloads = cJSON_GetObjectItem(library, "downloads");
    if (!downloads)
      continue;

    /* we do this for the sake of old versions which use the classifier system */
    cJSON *classifiers = cJSON_GetObjectItem(downloads, "classifiers");
    if (classifiers) {
      cJSON *natives_linux = cJSON_GetObjectItem(classifiers, "natives-linux");

      if (natives_linux) {
        cJSON *path_j = cJSON_GetObjectItem(natives_linux, "path");
        cJSON *url_j = cJSON_GetObjectItem(natives_linux, "url");

        if (!path_j || !url_j) continue;

        char *path = path_j->valuestring;
        char *url = url_j->valuestring;

        size_t len = strlen(minecraft_path) + strlen(path) + 50;
        dests[start] = malloc(len);
        snprintf(dests[start], len, "libraries/%s", path);
        urls[start] = url;
        start++;
      }
    }

    /* the newer artifact system is handled here. */
    cJSON *artifact = cJSON_GetObjectItem(downloads, "artifact");
    if (!artifact)
      continue;

    cJSON *path_j = cJSON_GetObjectItem(artifact, "path");
    cJSON *url_j = cJSON_GetObjectItem(artifact, "url");
    
    if (!path_j || !url_j) continue;

    char *path = path_j->valuestring;
    char *url = url_j->valuestring;

    size_t len = strlen(minecraft_path) + strlen(path) + 50;
    dests[start] = malloc(len);
    snprintf(dests[start], len, "libraries/%s", path);

    urls[start] = url;
    start++;
  }

  download_files(urls, dests, start);

  for (int i = 0; i < start; i++)
    free(dests[i]);
  free(urls);
  free(dests);

  return 0;
}

static int download_client(cJSON *version_json) {

  if(!version_json) {
      printlog("ERROR", __func__, "Obtained NULL version JSON, aborting!");
      return 1;
  }

  cJSON *id = cJSON_GetObjectItem(version_json, "id");
  cJSON *downloads = cJSON_GetObjectItem(version_json, "downloads");
  cJSON *client = cJSON_GetObjectItem(downloads, "client");
  cJSON *cl_url = cJSON_GetObjectItem(client, "url");

  char cl_dest[BUF_MID];

  if (!id) {
      printlog("ERROR", __func__, "Failed to obtain version ID from JSON,"
        "Is it corrupted?");
      return 1;
  }
  if (!cl_url) {
      printlog("ERROR", __func__, "Failed to obtain client download URL from JSON,"
        "Is it corrupted?");
      return 1;
  }
  snprintf(cl_dest, sizeof(cl_dest), "versions/%s/%s.jar",
           id->valuestring, id->valuestring);
  download_file(cl_url->valuestring, cl_dest);
  return 0;
}

static int download_assets(cJSON *version_json) {

  if (!version_json) {
      printlog("ERROR", __func__, "Obtained NULL version JSON, aborting!");
      return 1;
  } 

  char *index_id = get_asset_index(version_json); 
  cJSON *index_details = cJSON_GetObjectItem(version_json, "assetIndex");
  cJSON *index_url = cJSON_GetObjectItem(index_details, "url");

  if (!index_id) {
    printlog("ERROR", __func__ ,"Failed to obtain asset index ID.");
    return 1;
  }
  if (!index_details || !index_url) {
    printlog("ERROR", __func__, "Failed to obtain information "
    "about the asset index, is the JSON corrupted?");
    return 1;
  }

  char dest_path[BUF_MID];
  snprintf(dest_path, sizeof(dest_path),
           "assets/indexes/%s.json", index_id);

  int status = download_file(index_url->valuestring, dest_path);
  if (status == 1) {
    printlog("ERROR", __func__, "Failed to download asset index!");
    return 1;
  }

  free(index_id);

  char *index_buf = read_file(dest_path);
  if (!index_buf) {
      printlog("ERROR", __func__, "Failed to READ asset index!");
      return 1;
  }
  cJSON *asset_index = cJSON_Parse(index_buf);
  if (!asset_index) {
      printlog("ERROR", __func__, "Failed to PARSE asset index, is it corrupted?");
      return 1;
  }

  cJSON *objects = cJSON_GetObjectItem(asset_index, "objects");
  
  int start = 0;
  size_t a_count = (size_t)cJSON_GetArraySize(objects);

  char **urls = malloc(a_count * (sizeof(char *)));
  char **dests = malloc(a_count * (sizeof(char *)));
  if (!urls || !dests) {
      printlog("ERROR", __func__, "malloc failed: %s", strerror(errno));
      return 1;
  }

  char *res_url = "https://resources.download.minecraft.net";

  cJSON *object = objects->child;

  while (object) {
    cJSON *hash = cJSON_GetObjectItem(object, "hash");
    if (!object) {
      printlog("ERROR", __func__, "Asset object doesn't have a valid hash,"
      " is the index corrupted?");
      return 1;
    }

    char url[256];

    snprintf(url, sizeof(url), "%s/%.2s/%s", res_url, hash->valuestring,
             hash->valuestring);
    snprintf(dest_path, sizeof(dest_path),
             "assets/objects/%.2s/%s", hash->valuestring,
             hash->valuestring);

    urls[start] = strdup(url);
    dests[start] = strdup(dest_path);
    start++;

    object = object->next;
  }
  
  download_files(urls, dests, start);

  for (int i = 0; i < start; i++) {
    free(urls[i]);
    free(dests[i]);
  }

  free(urls);
  free(dests);

  cJSON_Delete(asset_index);
  free(index_buf);

  return 0;
}

int download_version(char *req_v) {
  printlog("INFO", __func__, "Downloading version manifest...");
  download_version_manifest();

  char *vm_buf = read_file("version_manifest.json");
  if (!vm_buf) {
      printlog("ERROR", __func__, "Failed to READ version_manifest.json!");
      goto abort;
  }

  cJSON *v_json = cJSON_Parse(vm_buf);
  free(vm_buf);

  if (!v_json) {
      printlog("ERROR", __func__, "Failed to PARSE version_manifest.json, "
       "is it corrupted? ");
      goto abort;
  }

  printlog("INFO", __func__, "Downloading %s.json...", req_v);
  int st = download_version_json(v_json, req_v);

  if (st) {
    printlog("ERROR", __func__, "Failed to download %s.json!", req_v);
    goto abort;
  }

  cJSON_Delete(v_json);

  char vj_path[BUF_MID];
  snprintf(vj_path, sizeof(vj_path), "versions/%s/%s.json",
           req_v, req_v);

  char *vj_buf = read_file(vj_path);
  if (!vj_buf) {
      printlog("ERROR", __func__, "Failed to READ %s.json!", req_v);
      goto abort;
  }

  cJSON *vj_json = cJSON_Parse(vj_buf);
  if (!vj_json) {
      printlog("ERROR", __func__, "Failed to PARSE %s.json, is it corrupted?", req_v);
      free(vj_buf);
      goto abort;
  }

  cJSON *libraries = cJSON_GetObjectItem(vj_json, "libraries");
  if (!libraries) {
      printlog("ERROR", __func__, "Failed to obtain libraries from %s.json, is it corrupted?", req_v);
      goto abort;
  }
  
  printlog("INFO", __func__,"Downloading libraries...");
  st = download_libraries(libraries);
  if (st) {
    printlog("ERROR", __func__, "Failed to download libraries!");
    goto abort;
  }

  printlog("INFO", __func__,"Downloading client...");
  st = download_client(vj_json);
  if (st) {
    printlog("ERROR", __func__, "Failed to download client!");
    goto abort;
  }

  printlog("INFO", __func__,"Downloading assets...");
  st = download_assets(vj_json);
  if (st) {
    printlog("ERROR", __func__, "Failed to download assets!");
    goto abort;
  }

  printlog("INFO", __func__,"%s successfully installed.", req_v);
  printlog("INFO", __func__,"Launch with -l %s", req_v);

  cJSON_Delete(vj_json);
  free(vj_buf);

  return 0;

  abort: 
    printlog("INFO", __func__, "Aborting!");
    return 1;
}