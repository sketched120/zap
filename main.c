
#define _GNU_SOURCE

#include <cjson/cJSON.h>
#include <curl/curl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

#include "include/auth.h"
#include "include/fabric.h"
#include "include/jvm_args.h"
#include "include/natives.h"
#include "include/utils.h"
#include "include/version.h"

char *minecraft_path = NULL;

static void print_help(const char *prog) {
  printf("usage: %s [options]\n\n"
         "options:\n"
         "  -l <version>    launch a version\n"
         "  -D <version>    dry run (print java cmdline)\n"
         "  -L              list available versions\n"
         "  -d              download a version (requires -v and -t)\n"
         "  -v <version>    version string (e.g. 1.21.1)\n"
         "  -t <type>       version type (release, snapshot, fabric)\n"
         "  -f <version>    fabric loader version (optional, defaults to latest)\n"
         "  -p <path>       minecraft path (default: ~/.minecraft)\n"
         "  -i              list installed versions\n"
         "  -h              print this help and exit\n", prog);
}

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

static void launchmc(int dry, char *version) {
  char jsonpath[256];
  snprintf(jsonpath, sizeof(jsonpath), "versions/%s/%s.json",
           version, version);

  char *jsonbuf = read_file(jsonpath);
  if (!jsonbuf) {
    fprintf(stderr, "error: failed to read %s\n", jsonpath);
    return;
  }

  cJSON *json = cJSON_Parse(jsonbuf);
  if (!json) {
    fprintf(stderr, "error: failed to parse version json\n");
    free(jsonbuf);
    return;
  }

  if (cJSON_GetObjectItem(json, "inheritsFrom")) {
    download_fabric_libraries(json);
    launch_loader_handler(json);
  }

  cJSON *main_class = cJSON_GetObjectItem(json, "mainClass");
  cJSON *libraries = cJSON_GetObjectItem(json, "libraries");
  cJSON *java_version = cJSON_GetObjectItem(json, "javaVersion");

  if (!main_class || !main_class->valuestring) {
    fprintf(stderr, "error: mainClass not found\n");
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

  char *accounts_buf = read_file("accounts.json");
  cJSON *accounts_json = cJSON_Parse(accounts_buf);
  if (!accounts_buf) {
    fprintf(stderr,
            " Failed to read accounts.json, create one from PrismLauncher\n");
    free(classpath);
    free(asset_index);
    cJSON_Delete(json);
    free(jsonbuf);
    return;
  }
  free(accounts_buf);

  Account *acc = get_account_details(accounts_json);
  char natives_dir[256];
  snprintf(natives_dir, sizeof(natives_dir), "natives/%s",
           version);
  LaunchContext ctx = {
      .version = version,
      .natives_dir = natives_dir,
      .classpath = classpath,
      .assets_dir = "assets/",
      .asset_index = asset_index,
      .game_dir = minecraft_path,
      .username = acc->username,
      .uuid = acc->uuid,
      .access_token = acc->ygg_token,
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
  char **java_args = malloc(sizeof(char *) * total);
  if (!java_args) {
    fprintf(stderr, "error: malloc failed\n");
    free_launch_resources(classpath, jvm_args, game_args, asset_index, json,
                          accounts_json, jsonbuf);
    return;
  }

  char javacmd[128];
  if (java_version_required < 9)
    snprintf(javacmd, sizeof(javacmd),
             "/usr/lib/jvm/java-%d-openjdk/jre/bin/java",
             java_version_required);
  else
    snprintf(javacmd, sizeof(javacmd), "/usr/lib/jvm/java-%d-openjdk/bin/java",
             java_version_required);

  int idx = 0;
  java_args[idx++] = javacmd;
  java_args[idx++] = "-Xmx2G";
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

  
  // big boy executor
  execvp(javacmd, java_args);

  perror("execvp failed");
  free(java_args);
  free_launch_resources(classpath, jvm_args, game_args, asset_index, json,
                        accounts_json, jsonbuf);
}

int main(int argc, char *argv[]) {
  curl_global_init(CURL_GLOBAL_ALL);

  char *instance = NULL;
  char *dry_arg = NULL;
  int download = 0;
  int list = 0;
  char *type = NULL;
  char *version = NULL;
  char *fabric_version = NULL;
  int opt;

  opterr = 0;

  while ((opt = getopt(argc, argv, "l:D:L:dv:t:f:p:ih")) != -1) {

    switch (opt) {
    case 'l':
      instance = optarg;
      break;
    case 'D':
      dry_arg = optarg;
      break;
    case 'L':
      list = 1;
      break;
    case 'v':
      version = optarg;
      break;
    case 'd':
      download = 1;
      break;
    case 'p':
        minecraft_path = optarg;
        break;
    case 't':
      type = optarg;
      break;
    case 'f':
      fabric_version = optarg;
      break;
    case 'i':
      list_installed();
      curl_global_cleanup();
      return 0;
    case 'h':
      print_help(argv[0]);
      curl_global_cleanup();
      return 0;
    case '?':
      if (optopt)
        fprintf(stderr, "Error: Unknown option '%-c'\n", optopt);
      else
        fprintf(stderr, "Error: Unknown option '%s'\n", argv[optind - 1]);
      fprintf(stderr, "Run with -h for usage.\n");
      curl_global_cleanup();
      return 1;
    }
  }
  if (!minecraft_path) {
    asprintf(&minecraft_path, "%s/.minecraft", getenv("HOME"));
  }

  mkdir(minecraft_path, 0755);
  chdir(minecraft_path);

  if (optind < argc) {
    fprintf(stderr, "Error: Unexpected argument '%s'\n", argv[optind]);
    fprintf(stderr, "Run with -h for usage.\n");
    curl_global_cleanup();
    return 1;
  }

  

  if (instance) {
    launchmc(0, instance);
    curl_global_cleanup();
    return 0;
  }
  if (dry_arg) {
    launchmc(1, dry_arg);
    curl_global_cleanup();
    return 0;
  }
  if (list == 1) {
    list_available_versions(type);
    curl_global_cleanup();
    return 0;
  }

  

  if (download == 1) {
    if (version) {

      if (!type) {
        fprintf(stderr, "Please specify a version type!\n"
                        "(Can be \"release\", \"snapshot\" or \"fabric\")");
        goto done;
      }

      if (streq(type, "release") || streq(type, "snapshot")) {
        download_version(version);
        goto done;

      } else if (streq(type, "fabric")) {

        int alloced = 0;
        if (!fabric_version) {
          fabric_version = get_latest_fabric_loader(version);
          printf("No loader version specified, choosing latest Fabric loader version.");
          alloced = 1;

        }

        download_fabric_manifest(version, fabric_version);
        

        printf("\nDownloading child version...\n");
        download_version(version);

        printf("\nDownloaded Fabric loader %s for %s successfully.\n"
        "Launch with \"-l fabric-loader-%s-%s\".", fabric_version, version, fabric_version, version);
        if (alloced == 1) free(fabric_version);
        goto done;
      } else {
        fprintf(stderr,
                "Invalid version type!\n"
                "Available types are \"release\", \"snapshot\" or \"fabric\"");
        goto done;
      }

    done:
      free(minecraft_path);
      curl_global_cleanup();
      return 0;
    }
  }

  if (fabric_version) {
    list_fabric_versions(fabric_version);
    free(minecraft_path);
    curl_global_cleanup();
    return 0;
  }

  fprintf(stderr, "Error: No mode specified, try -h\n");
  free(minecraft_path);
  curl_global_cleanup();
  return 1;
}
