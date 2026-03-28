#include <cjson/cJSON.h>
#include <stdio.h>
#include <string.h>
#include <zip.h>

#include "utils.h"
#include <sys/stat.h>

void extract_natives(char *jar, char *dest) {
  zip_t *zip = zip_open(jar, 0, NULL);
  if (!zip) {
    printf("failed to open jar %s.\n", jar);
    return;
  }
  mkdir(dest, 0755);
  int cnt = zip_get_num_entries(zip, 0);

  for (int i = 0; i < cnt; i++) {
    const char *name = zip_get_name(zip, i, 0);

    if (!strstr(name, ".so"))
      continue;
    
    printf("found %s.\n", name);
    const char *filename = strrchr(name, '/');
    if (filename)
      filename = filename + 1;
    else
      filename = name;
    char dest_path[512];
    snprintf(dest_path, sizeof(dest_path), "%s/%s", dest, filename);
    /* size_t dest_size = strlen(dest) + strlen(name) + 20;
    snprintf(dest, dest_size, )*/
    if (file_exists(dest_path) == true) continue;
    printf("extracting to: %s\n", dest_path);
    zip_file_t *file = zip_fopen_index(zip, i, 0);
    if (!file) { printf("zip_fopen_index failed\n"); continue; }
    FILE *out = fopen(dest_path, "wb");
    if (!out) { printf("fopen failed for %s \n", dest_path); continue; }

    char buf[4096];
    zip_int64_t bytes;

    while ((bytes = zip_fread(file, buf, sizeof(buf))) > 0) {
      fwrite(buf, 1, bytes, out);
    }

    fclose(out);
    zip_fclose(file);
  }

  zip_close(zip);
}
void extract_wrapper(cJSON *json) {
    
    cJSON *id = cJSON_GetObjectItem(json, "id");
    printf("extract_wrapper called for: %s\n", id->valuestring);

    cJSON *libraries = cJSON_GetObjectItem(json, "libraries");
    printf("library count: %d\n", cJSON_GetArraySize(libraries));
    cJSON *lib;
  cJSON_ArrayForEach(lib, libraries) {
    cJSON *name = cJSON_GetObjectItem(lib, "name");
    if (!name) continue;
    if (!strstr(name->valuestring, "natives-linux")) continue;
    if (!is_allowed_on_linux(lib)) continue;

    cJSON *downloads = cJSON_GetObjectItem(lib, "downloads");
    if (!downloads) continue;
    cJSON *artifact = cJSON_GetObjectItem(downloads, "artifact");
    if (!artifact) continue;
    cJSON *path = cJSON_GetObjectItem(artifact, "path");
    if (!path) continue;

    char jar[512];
    snprintf(jar, sizeof(jar), MINECRAFT_PATH "/libraries/%s", path->valuestring);

    char natives_dir[256];
    snprintf(natives_dir, sizeof(natives_dir), MINECRAFT_PATH"/natives/%s",id->valuestring);
    extract_natives(jar, natives_dir );
}
    

   
  }

