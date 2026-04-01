// auth.c

#include <cjson/cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include "utils.h"
#include "auth.h"

Account *get_account_details(cJSON *auth_json) {
    
    Account *acc = malloc(sizeof(Account));
    cJSON *accounts = cJSON_GetObjectItem(auth_json, "accounts");
    cJSON *account = cJSON_GetArrayItem(accounts, 0);
    
    nullchkr(account, NULL);
    cJSON *profile = cJSON_GetObjectItem(account, "profile");

    acc->username = cJSON_GetObjectItem(profile, "name")->valuestring;
    acc->uuid = cJSON_GetObjectItem(profile, "id")->valuestring;

    cJSON *ygg = cJSON_GetObjectItem(account, "ygg");
    
    acc->ygg_token = cJSON_GetObjectItem(ygg, "token")->valuestring;

    return acc;

}