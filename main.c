
#define _GNU_SOURCE

#include <cjson/cJSON.h>
#include <curl/curl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

#include "include/launch.h"
#include "include/fabric.h"
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
