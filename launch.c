#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <cjson/cJSON.h>

#include "include/utils.h"
#include "include/fabric.h"
#include "include/auth.h"
#include "include/jvm_args.h"
#include "include/natives.h"

#include "include/launch.h"

static void free_launch_resources(char *classpath, char **jvm_args,
                                  char **game_args, char *asset_index,
                                  cJSON *json, cJSON *auth_json,
                                  char *jsonbuf) {
  free(classpath);
  if (jvm_args) {
    for (int i = 0; jvm_args[i]; i++)
      free(jvm_args[i]);
    free(jvm_args);
  }
  if (game_args) {
    for (int i = 0; game_args[i]; i++)
      free(game_args[i]);
    free(game_args);
  }
  free(asset_index);
  cJSON_Delete(json);
  cJSON_Delete(auth_json);
  free(jsonbuf);
}

void launchmc(int dry, float mem, char *version) {
  char jsonpath[BUF_MID];
  snprintf(jsonpath, sizeof(jsonpath), "versions/%s/%s.json",
           version, version);

  char *jsonbuf = read_file(jsonpath);
  if (!jsonbuf) {
    printlog("ERROR", __func__, "Failed to READ %s", jsonpath);
    return;
  }

  cJSON *json = cJSON_Parse(jsonbuf);
  if (!json) {
    printlog("ERROR", __func__, "Failed to PARSE version JSON, is it corrupted?");
    free(jsonbuf);
    return;
  }

  if (cJSON_GetObjectItem(json, "inheritsFrom")) {
    download_fabric_libraries(json);
    launch_loader_handler(json);
  }

  cJSON *main_class = cJSON_GetObjectItem(json, "mainClass");
  cJSON *java_version = cJSON_GetObjectItem(json, "javaVersion");

  if (!main_class || !main_class->valuestring) {
    printlog("ERROR", __func__, "mainClass not found in version JSON, is it corrupted?");
    cJSON_Delete(json);
    free(jsonbuf);
    return;
  }

  int java_version_required = 21;
  if (java_version) {
    cJSON *major = cJSON_GetObjectItem(java_version, "majorVersion");
    if (major)
      java_version_required = major->valueint;
  }

  char *asset_index = get_asset_index(json);
  char *classpath = build_classpath(json);

  size_t acc_path_size = strlen(minecraft_path) + 16;
  char accounts_path[acc_path_size];

  snprintf(accounts_path, acc_path_size, "%s/zap/creds.json", minecraft_path);

  char *accounts_buf = read_file(accounts_path);
  cJSON *accounts_json = cJSON_Parse(accounts_buf);
  if (!accounts_buf) {
    printlog("ERROR", __func__, "Failed to READ accounts.json, create one from PrismLauncher");
    free(classpath);
    free(asset_index);
    cJSON_Delete(json);
    free(jsonbuf);
    return;
  }
  free(accounts_buf);

  Account *acc = get_account_details(accounts_json);
  char natives_dir[BUF_LARGE];
  snprintf(natives_dir, sizeof(natives_dir), "natives/%s", version);

  char game_dir[BUF_LARGE];
  getcwd(game_dir, sizeof(game_dir));

  LaunchContext ctx = {
      .version = version,
      .natives_dir = natives_dir,
      .classpath = classpath,
      .assets_dir = "assets/",
      .asset_index = asset_index,
      .game_dir = game_dir,
      .username = acc->name,
      .uuid = acc->uuid,
      .access_token = acc->mc_token,
  };

  char **jvm_args = build_jvm_args(json, &ctx);
  char **game_args = build_game_args(json, &ctx);

  extract_wrapper(json);

  int jvm_count = 0, game_count = 0;
  while (jvm_args[jvm_count])
    jvm_count++;
  while (game_args[game_count])
    game_count++;

  int total = 2 + jvm_count + 1 + game_count + 1;
  if (total <= 0) {
    printlog("ERROR", __func__, "An error occured while constructing JVM arguments");
    return;
  }
  char **java_args = malloc(sizeof(char *) * (size_t)total);

  if (!java_args) {
    printlog("ERROR", __func__, "malloc failed: %s", strerror(errno));
    free_launch_resources(classpath, jvm_args, game_args, asset_index, json,
                          accounts_json, jsonbuf);
    return;
  }

  char javacmd[BUF_SMALL];
  if (java_version_required < 9)
    snprintf(javacmd, sizeof(javacmd),
             "/usr/lib/jvm/java-%d-openjdk/jre/bin/java",
             java_version_required);
  else
    snprintf(javacmd, sizeof(javacmd), "/usr/lib/jvm/java-%d-openjdk/bin/java",
             java_version_required);

  int idx = 0;

  char memarg[10];
  snprintf(memarg, sizeof(memarg), "-Xmx%dM", (int)(mem * 1024));

  java_args[idx++] = javacmd;
  java_args[idx++] = memarg;
  for (int i = 0; i < jvm_count; i++)
    java_args[idx++] = jvm_args[i];
  java_args[idx++] = main_class->valuestring;
  for (int i = 0; i < game_count; i++)
    java_args[idx++] = game_args[i];
  java_args[idx] = NULL;

  if (dry) {
    printf("cmdline:\n");
    for (int i = 0; java_args[i]; i++)
      printf("%s ", java_args[i]);
    printf("\n");
    free(java_args);
    free_launch_resources(classpath, jvm_args, game_args, asset_index, json,
                          accounts_json, jsonbuf);
    return;
  }

  execvp(javacmd, java_args);

  perror("execvp failed");
  free(java_args);
  free_launch_resources(classpath, jvm_args, game_args, asset_index, json,
                        accounts_json, jsonbuf);
}