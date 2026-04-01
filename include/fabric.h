#ifndef FABRIC_H
#define FABRIC_H

#include <cjson/cJSON.h>

void launch_loader_handler(cJSON *child_json);
void download_fabric_libraries(cJSON *json);
void list_fabric_versions(char *req_mc_version);
char *get_latest_fabric_loader(char *req_mc_version);
void download_fabric_manifest(char *req_mc_version, char *req_loader_version);


#endif