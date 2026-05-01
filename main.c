
#define _GNU_SOURCE

#include <cjson/cJSON.h>
#include <curl/curl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "include/launch.h"
#include "include/fast.h"
#include "include/fabric.h"
#include "include/utils.h"
#include "include/version.h"

char *minecraft_path = NULL;

static void print_help(const char *prog) {
  printlog("INFO", "launcher", "usage: %s [options]\n"
         "options:\n"
         "  -l -v <version>          launch a version\n"
         "  -D <version>             dry run (print java cmdline)\n"
         "  -L                       list available versions\n"
         "  -d -v <version> -t <type>  download a version\n"
         "  -t <type>                release, snapshot, fabric\n"
         "  -f <version>             fabric loader version (default: latest)\n"
         "  -p <path>                minecraft path (default: ~/.minecraft)\n"
         "  -n <instance>            use an instance directory\n"
         "  -i                       list installed versions\n"
         "  -h                       print this help and exit", prog);
}

int main(int argc, char *argv[]) {
  curl_global_init(CURL_GLOBAL_ALL);

  char *version = NULL;
  char *dry_arg = NULL;
  int launch = 0, download = 0, list = 0, listi = 0, zmm = 0;
  char *type = NULL;
  char *instance = NULL;
  char *fabric_version = NULL;
  int fabric_alloced = 0;
  float mem = 2;
  int fast = 0;
  int opt;
  int exit_code = 0;

  opterr = 0;

  while ((opt = getopt(argc, argv, "lD:Ldv:t:Ff:p:in:m:zh")) != -1) {
    switch (opt) {
    case 'l': launch = 1; break;
    case 'D': dry_arg = optarg; break;
    case 'L': list = 1; break;
    case 'v': version = optarg; break;
    case 'd': download = 1; break;
    case 'p': minecraft_path = strdup(optarg); break;
    case 't': type = optarg; break;
    case 'F': fast = 1; break;
    case 'f': fabric_version = optarg; break;
    case 'i': listi = 1; break;
    case 'n': instance = optarg; break;
    case 'z': zmm = 1; break;
    case 'm': mem = strtof(optarg, NULL); break;
    case 'h':
      print_help(argv[0]);
      goto cleanup;
    case '?':
      if (optopt)
        printlog("ERROR", "launcher", "Unknown option '%-c'", optopt);
      else
        printlog("ERROR", "launcher","Unknown option '%s'", argv[optind - 1]);
      exit_code = 1;
      goto cleanup;
    }
  }

  if (!minecraft_path) {
    asprintf(&minecraft_path, "%s/.minecraft", getenv("HOME"));
  }
 
  mkdir(minecraft_path, 0755);
  chdir(minecraft_path);

  if (fast == 1) {
    fastlaunch();
    return 0;
  }

  printlog("INFO", "launcher", "zap 0.0.1" );

  if (instance) {
    char *inst_path = NULL;
    asprintf(&inst_path, "%s/instances/%s", minecraft_path, instance);
    mkdirs(inst_path);
    mkdir(inst_path, 0755);
    chdir(inst_path);
    free(inst_path);
  }

  if (zmm == 1) {
    char path[BUF_MID];
    snprintf(path, sizeof(path), "%s/zmm", minecraft_path);
    execl(path, path, NULL);
    perror("execlp");
    exit_code = 1;
    goto cleanup;
  } 

  if (listi == 1) {
    list_installed();
    goto cleanup;
  }

  if (optind < argc) {
    printlog("ERROR","launcher","Unexpected argument '%s'", argv[optind]);
    exit_code = 1;
    goto cleanup;
  }

  if (launch == 1) {
    if (version) {
      fastcreate(argc, argv);
      launchmc(0, mem, version);
      goto cleanup;
    } else {
      printlog("ERROR", "launcher", "Specify a version to launch.");
      exit_code = 1;
      goto cleanup;
    }
  }

  if (dry_arg) {
    launchmc(1, mem, dry_arg);
    goto cleanup;
  }

  if (list == 1) {
    list_available_versions(type);
    goto cleanup;
  }

  if (download == 1) {
    if (version) {
      if (!type) {
        printlog("ERROR", "launcher","Specify a type!");
        exit_code = 1;
        goto cleanup;
      }
      if (streq(type, "release") || streq(type, "snapshot")) {
        download_version(version);
      } else if (streq(type, "fabric")) {
        if (!fabric_version) {
          fabric_version = get_latest_loader(version);
          fabric_alloced = 1;
          if (!fabric_version) {
            exit_code = 1;
            goto cleanup;
          }
        }
        download_fabric_manifest(version, fabric_version);
        if (download_version(version) != 0) {
          printlog("ERROR", __func__, "Download failed!");
          exit_code = 1;
        }
      } else {
        printlog("ERROR", "launcher", "Invalid type!");
        exit_code = 1;
      }
      goto cleanup;
    }
  }

  if (fabric_version && !download) {
    list_fabric_versions(fabric_version);
    goto cleanup;
  }

  printlog("ERROR", "launcher", "No mode specified, try -h");
  exit_code = 1;

cleanup:
  if (fabric_alloced && fabric_version) free(fabric_version);
  if (minecraft_path) free(minecraft_path);
  curl_global_cleanup();
  return exit_code;
}
