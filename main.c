#include <stdio.h>
#include <stdlib.h>
//#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <curl/curl.h>

// #include "include/auth.h"
//#include "download.h"
#include "utils.h"
#include "jvm_args.h"
#include "natives.h"
#include "version.h"
#include "fabric.h"

static void print_help(const char *prog)
{
    printf("usage: %s [options]\n\n", prog);
    printf("options:\n");
    printf("  -l, --launch <instance>         launch a specific instance\n");
    printf("  -L, --list <type>               list available instances ('snapshot' or 'release')\n");
    printf("  -F, --fabric <version>          list available fabric loaders for a specified version\n");
    printf("  -D, --download <version>        download the given version\n");
    printf("  -d, --dry <instance>            dry run (print java cmdline)\n");
    printf("  -i, --installed                 list installed instances\n");
    printf("  -h, --help                      print this help and exit\n");
}

static void free_launch_resources(char *classpath, char **jvm_args,
                                  char **game_args, char *asset_index,
                                  cJSON *json, char *jsonbuf)
{
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
    free(jsonbuf);
}

static void launchmc(int dry, char *version)
{
    char jsonpath[256];
    snprintf(jsonpath, sizeof(jsonpath), MINECRAFT_PATH "/versions/%s/%s.json",
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

    cJSON *main_class   = cJSON_GetObjectItem(json, "mainClass");
    cJSON *libraries    = cJSON_GetObjectItem(json, "libraries");
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
    char *classpath   = build_classpath(json);

    char *accounts_path = MINECRAFT_PATH "/accounts.json";
    /* if (!file_exists(accounts_path)) {
        fprintf(stderr, "error: no accounts.json found, create one from PrismLauncher\n");
        free(classpath);
        free(asset_index);
        cJSON_Delete(json);
        free(jsonbuf);
        return;
    } */

    // Account account_info = get_account_details(accounts_path);
    char natives_dir[256];
    snprintf(natives_dir, sizeof(natives_dir), MINECRAFT_PATH"/natives/%s", version);
    LaunchContext ctx = {
        .version      = version,
         .natives_dir  = natives_dir,
        .classpath    = classpath,
        .assets_dir   = MINECRAFT_PATH "/assets/",
        .asset_index  = asset_index,
        .game_dir     = MINECRAFT_PATH,
        // .username     = account_info.username,
        // .uuid         = account_info.uuid,
        // .access_token = account_info.ygg_token,
        .username = "sketched_test",
        .uuid = "00000000-0000-0000-0000-000000000000"
    };

    char **jvm_args  = build_jvm_args(json, &ctx);
    char **game_args = build_game_args(json, &ctx);

    //system("rm -rf /tmp/tnt-mc-natives");
    extract_wrapper(json);

    int jvm_count = 0, game_count = 0;
    while (jvm_args[jvm_count])   jvm_count++;
    while (game_args[game_count]) game_count++;

    int total        = 2 + jvm_count + 1 + game_count + 1;
    char **java_args = malloc(sizeof(char *) * total);
    if (!java_args) {
        fprintf(stderr, "error: malloc failed\n");
        free_launch_resources(classpath, jvm_args, game_args, asset_index, json, jsonbuf);
        return;
    }

    char javacmd[128];
    if (java_version_required < 9)
        snprintf(javacmd, sizeof(javacmd),
                 "/usr/lib/jvm/java-%d-openjdk/jre/bin/java", java_version_required);
    else
        snprintf(javacmd, sizeof(javacmd),
                 "/usr/lib/jvm/java-%d-openjdk/bin/java", java_version_required);

    int idx = 0;
    java_args[idx++] = javacmd;
    java_args[idx++] = "-Xmx2G";
    for (int i = 0; i < jvm_count; i++)  java_args[idx++] = jvm_args[i];
    java_args[idx++] = main_class->valuestring;
    for (int i = 0; i < game_count; i++) java_args[idx++] = game_args[i];
    java_args[idx]   = NULL;

    if (dry) {
        printf("cmdline:\n");
        for (int i = 0; java_args[i]; i++)
            printf("%s ", java_args[i]);
        printf("\n");
        free(java_args);
        free_launch_resources(classpath, jvm_args, game_args, asset_index, json, jsonbuf);
        return;
    }

    execvp(javacmd, java_args);

    perror("execvp failed");
    free(java_args);
    free_launch_resources(classpath, jvm_args, game_args, asset_index, json, jsonbuf);
}

int main(int argc, char *argv[])
{
    curl_global_init(CURL_GLOBAL_ALL);


    static struct option long_opts[] = {
        { "launch",    required_argument, 0, 'l' },
        { "dry",       required_argument, 0, 'd' },
        { "list",      required_argument, 0, 'L' },
        { "download",  required_argument, 0, 'D' },
        { "fabric",    required_argument, 0, 'F' },
        { "help",      no_argument,       0, 'h' },
        { "installed", no_argument,       0, 'i' },
        { 0, 0, 0, 0 }
    };

    char *instance         = NULL;
    char *dry_arg          = NULL;
    char *reqtype          = NULL;
    char *reqversion       = NULL;
    char *reqfabricversion = NULL;
    int opt;

    while ((opt = getopt_long(argc, argv, "l:d:hL:D:F:i", long_opts, NULL)) != -1) {
        switch (opt) {
        case 'l': instance         = optarg; break;
        case 'd': dry_arg          = optarg; break;
        case 'L': reqtype          = optarg; break;
        case 'D': reqversion       = optarg; break;
        case 'F': reqfabricversion = optarg; break;
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
                fprintf(stderr, "error: unknown option '-%c'\n", optopt);
            else
                fprintf(stderr, "error: unknown option '%s'\n", argv[optind - 1]);
            fprintf(stderr, "run with -h for usage\n");
            curl_global_cleanup();
            return 1;
        }
    }

    if (optind < argc) {
        fprintf(stderr, "error: unexpected argument '%s'\n", argv[optind]);
        fprintf(stderr, "run with -h for usage\n");
        curl_global_cleanup();
        return 1;
    }

    if (instance)         { launchmc(0, instance);                     curl_global_cleanup(); return 0; }
    if (dry_arg)          { launchmc(1, dry_arg);                      curl_global_cleanup(); return 0; }
    if (reqtype)          { list_available_versions(reqtype);                    curl_global_cleanup(); return 0; }
    if (reqversion)       { download_version(reqversion);              curl_global_cleanup(); return 0; }
    if (reqfabricversion) { list_fabric_versions(reqfabricversion);    curl_global_cleanup(); return 0; }

    fprintf(stderr, "error: no mode specified, try -h\n");
    curl_global_cleanup();
    return 1;
}
