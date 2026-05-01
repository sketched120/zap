#ifndef AUTH_H
#define AUTH_H

#include <cjson/cJSON.h>

#define XERR_NO_XBOX_ACCOUNT  2148916233
#define XERR_COUNTRY_BANNED   2148916235
#define XERR_ADULT_VERIFY_1   2148916236
#define XERR_ADULT_VERIFY_2   2148916237
#define XERR_CHILD_ACCOUNT    2148916238

/*typedef struct {
    char *username;
    char *uuid;
    char *ygg_token;
} Account;*/

typedef struct {
    /* msa */
    char *access_token;
    char *refresh_token;
    long long ms_expiry;

    /* xbox */
    char *xbl_token;
    char *uhs;
    char *xsts_token;

    /* minecraft */
    char *mc_token;
    long long mc_expiry;
    char *uuid;
    char *name;
} Account;

void auth_flow(void);
Account *get_account_details(cJSON *auth_json);

#endif
