#include <cjson/cJSON.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "include/utils.h"

#include "include/fast.h"

int fastcreate(int argc, char *argv[]) {
  char command[128];
  size_t offset = 0;

  for (int i = 0; i < argc; i++) {
    int written = snprintf(command + offset, sizeof(command), "%s %s",
                           command + offset, argv[i]);

    if (written > 0 && (size_t)written < sizeof(command) - offset) {
      offset += written;
    } else
      break;
  }

  cJSON *fastl_root = cJSON_CreateObject();
  cJSON_AddStringToObject(fastl_root, "fast_arguments", command);

  char *out = cJSON_Print(fastl_root);
  cJSON_Delete(fastl_root);

  char path[512];
  snprintf(path, sizeof(path), "%s/zap/fastlaunch.json", minecraft_path);
  FILE *f = fopen(path, "w");
  if (!f) {
    printlog("ERROR", __func__, "Failed to open fastlaunch.json: %s",
             strerror(errno));
    return 1;
  }

  fputs(out, f);
  fclose(f);

  free(out);

  return 0;
}
int fastlaunch(void) {
char path[512];
  snprintf(path, sizeof(path), "%s/zap/fastlaunch.json", minecraft_path);
  char *fastl_buf = read_file(path);
  cJSON *fastl_json = cJSON_Parse(fastl_buf);

  cJSON *args = cJSON_GetObjectItem(fastl_json, "fast_arguments");
  char string[128];
  snprintf(string, sizeof(string), "%s", args->valuestring);

  cJSON_Delete(fastl_json);

  char *arg[128];

  int i = 0;
  char *context = NULL;

  size_t argsize = sizeof(arg)/sizeof(char *);
  char *token = strtok_r(string, " ", &context);
  while (token != NULL && i < argsize) {
    arg[i] = token;
    i++;
    token = strtok_r(NULL, " ", &context);
  }

  arg[i] = NULL;
  execvp(arg[0], arg);
  return 0;
}