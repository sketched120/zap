#ifndef NATIVES_H
#define NATIVES_H

#include <cjson/cJSON.h>

void extract_natives(char *jar, char *dest);
void extract_wrapper(cJSON *json);

#endif