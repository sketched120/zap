#ifndef AUTH_H
#define AUTH_H

#include <cjson/cJSON.h>

typedef struct {
    char *username;
    char *uuid;
    char *ygg_token;
} Account;

Account *get_account_details(cJSON *auth_json);

#endif