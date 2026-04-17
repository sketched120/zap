// auth.c

#include <cjson/cJSON.h>
#include <curl/curl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "include/utils.h"

#include "include/auth.h"

/* we are reusing the official minecraft launcher's client id here */
#define CLIENT_ID "00000000402B5328"

typedef struct {
  char *memory;
  size_t size;
} Buffer;

void auth_flow();

Account *get_account_details(cJSON *creds) {

  auth_flow();
  Account *acc = malloc(sizeof(Account));

  acc->name = cJSON_GetObjectItem(creds, "name")->valuestring;
  acc->uuid = cJSON_GetObjectItem(creds, "uuid")->valuestring;

  acc->mc_token = cJSON_GetObjectItem(creds, "ygg_token")->valuestring;

  return acc;
}
static size_t write_cb(char *contents, size_t size, size_t nmemb, void *userp) {
  size_t realsize = size * nmemb;
  Buffer *mem = (Buffer *)userp;

  char *ptr = realloc(mem->memory, mem->size + realsize + 1);
  if (!ptr) {
    /* out of memory! */
    printlog("ERROR", __func__, "not enough memory (realloc returned NULL)\n");
    return 0;
  }

  mem->memory = ptr;
  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;

  return realsize;
}

/* -- Helper functions -- */
static void reset_buffer(Buffer *buf) {
  free(buf->memory);
  buf->memory = malloc(1);
  buf->size = 0;
}
static void free_shit(Account *account) {
  if (!account)
    return;
  free(account->access_token);
  free(account->refresh_token);
  free(account->xbl_token);
  free(account->xsts_token);
  free(account->mc_token);
  free(account->uhs);
  free(account->uuid);
  free(account->name);

  memset(account, 0, sizeof(Account));
}

static int auth_post(char *url, char *field, Buffer *resp, long *http_code) {
  CURL *curl;
  CURLcode result;

  struct curl_slist *headers = NULL;
  headers = curl_slist_append(headers, "Content-Type: application/json");
  headers = curl_slist_append(headers, "Accept: application/json");

  /* get a curl handle */
  curl = curl_easy_init();
  if (curl) {
    /* First set the URL that is about to receive our POST. This URL can
       be an https:// URL if that is what should receive the data. */
    curl_easy_setopt(curl, CURLOPT_URL, url);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, resp);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    /* Now specify the POST data */
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, field);

    /* Perform the request, result gets the return code */
    result = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, http_code);
    /* Check for errors */
    if (result != CURLE_OK)
      fprintf(stderr, "curl_easy_perform() failed: %s\n",
              curl_easy_strerror(result));

    /* always cleanup */
    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);
  }

  return (int)result;
}
static int auth_form_post(char *url, char *field, Buffer *resp,
                          long *http_code) {
  CURL *curl;
  CURLcode result;

  struct curl_slist *headers = NULL;
  headers = curl_slist_append(
      headers, "Content-Type: application/x-www-form-urlencoded");

  /* get a curl handle */
  curl = curl_easy_init();
  if (curl) {
    /* First set the URL that is about to receive our POST. This URL can
       be an https:// URL if that is what should receive the data. */
    curl_easy_setopt(curl, CURLOPT_URL, url);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, resp);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    /* Now specify the POST data */
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, field);

    /* Perform the request, result gets the return code */
    result = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, http_code);
    /* Check for errors */
    if (result != CURLE_OK)
      fprintf(stderr, "curl_easy_perform() failed: %s\n",
              curl_easy_strerror(result));

    /* always cleanup */
    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);
  }

  return (int)result;
}

static int auth_get(char *url, char *token, Buffer *resp, long *http_code) {
  CURL *curl;

  struct curl_slist *headers = NULL;

  curl = curl_easy_init();
  if (curl) {
    curl_easy_setopt(curl, CURLOPT_URL, url);

    /* Use HTTP/3 but fallback to earlier HTTP if necessary */
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_3);
    if (token) {
      char auth[4096];
      snprintf(auth, sizeof(auth), "Authorization: Bearer %s", token);
      headers = curl_slist_append(headers, auth);
      curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, resp);

    /* Perform the request, result gets the return code */
    CURLcode result = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, http_code);
    /* Check for errors */
    if (result != CURLE_OK)
      fprintf(stderr, "curl_easy_perform() failed: %s\n",
              curl_easy_strerror(result));

    /* always cleanup */
    if (headers)
      curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
  }

  return 0;
}

/* -- global response buffer for all functions -- */
static Buffer resp;

static long http_code;

/* -- Microsoft auth -- */
static int msa_auth(Account *account) {
  char *redirect_url = "https://login.live.com/oauth20_desktop.srf";
  char code_url[512];

  snprintf(code_url, sizeof(code_url),
           "https://login.live.com/"
           "oauth20_authorize.srf?prompt=select_account&client_id=%s"
           "&response_type=code&scope=service%%3A%%3Auser.auth.xboxlive.com%%"
           "3A%%3AMBI_SSL&lw=1&fl=dob,easi2&xsup=1&nopa=2&redirect_uri=%s",
           CLIENT_ID, redirect_url);

  pid_t pid = fork();
  if (pid == 0) {
    execlp("xdg-open", "xdg-open", code_url, NULL);
    exit(0);
  }
  char msa_code[1024];
  printlog("INFO", __func__,
           "Please enter the access code from"
           " your redirect URL: ");
  fgets(msa_code, sizeof(msa_code), stdin);
  msa_code[strcspn(msa_code, "\n")] = '\0';

  char *mstoken_url = "https://login.live.com/oauth20_token.srf";
  char mstoken_body[2048];
  snprintf(mstoken_body, sizeof(mstoken_body),
           "client_id=%s&code=%s&redirect_uri=%s&grant_type=authorization_code&"
           "scope=service::user.auth.xboxlive.com::MBI_SSL",
           CLIENT_ID, msa_code, redirect_url);

  memset(msa_code, 0, sizeof(msa_code));
  resp.memory = malloc(1);
  resp.size = 0;

  int post_st = auth_form_post(mstoken_url, mstoken_body, &resp, &http_code);
  if (post_st) {
    puts("An error occured when GET"); /*debug*/

    free(resp.memory);
    return 1;
  }

  cJSON *tok_resp = cJSON_Parse(resp.memory);
  if (!tok_resp) {
    fprintf(stderr, "cJSON parse failed: %s\n", resp.memory);

    free(resp.memory);
    return 1;
  }
  cJSON *token = cJSON_GetObjectItem(tok_resp, "access_token");
  if (!token) {
    fprintf(stderr, "no access_token in response: %s\n", resp.memory);

    free(resp.memory);
    return 1;
  }

  cJSON *refresh_tok = cJSON_GetObjectItem(tok_resp, "refresh_token");
  if (!refresh_tok) {
    fprintf(stderr, "no refresh_token in response: %s\n", resp.memory);
    free(resp.memory);
    return 1;
  }

  cJSON *expiry_obj = cJSON_GetObjectItem(tok_resp, "expires_in");

  account->access_token = strdup(token->valuestring);        /* free this */
  account->refresh_token = strdup(refresh_tok->valuestring); /* and this  */
  account->ms_expiry = time(NULL) + (long)expiry_obj->valueint;

  cJSON_Delete(tok_resp);

  reset_buffer(&resp);
  return 0;
}

static int msa_refresh(Account *account) {
  char *refresh_url = "https://login.live.com/oauth20_token.srf";
  char refresh_body[2048];

  snprintf(
      refresh_body, sizeof(refresh_body),
      "client_id=%s"
      "&grant_type=refresh_token"
      "&refresh_token=%s"
      "&scope=service::user.auth.xboxlive.com::MBI_SSL"
      "&redirect_uri=https%%3A%%2F%%2Flogin.live.com%%2Foauth20_desktop.srf",
      CLIENT_ID, account->refresh_token);

  int post_st = auth_form_post(refresh_url, refresh_body, &resp, &http_code);
  if (post_st) {
    puts("An error occured when POST"); /*debug*/
    free(resp.memory);
    return 1;
  }
  cJSON *refresh_json = cJSON_Parse(resp.memory);
  if (!refresh_json) {
    printlog("ERROR", __func__, "cJSON Parse error: %s", resp.memory);
    free(resp.memory);
    return 1;
  }
  cJSON *refreshed_token_obj =
      cJSON_GetObjectItem(refresh_json, "access_token");
  if (!refreshed_token_obj) {
    printlog("ERROR", __func__, "No access_token in response: %s", resp.memory);
    free(resp.memory);
    return 1;
  }
  cJSON *refreshed_refresh_token_obj =
      cJSON_GetObjectItem(refresh_json, "refresh_token");
  if (!refreshed_refresh_token_obj) {
    printlog("ERROR", __func__, "No refresh_token in response: %s",
             resp.memory);
    free(resp.memory);
    return 1;
  }
  cJSON *refreshed_expiry = cJSON_GetObjectItem(refresh_json, "expires_in");

  account->access_token = strdup(refreshed_token_obj->valuestring);
  account->refresh_token = strdup(refreshed_refresh_token_obj->valuestring);
  account->ms_expiry =
      time(NULL) + (long)(cJSON_GetNumberValue(refreshed_expiry));

  cJSON_Delete(refresh_json);
  reset_buffer(&resp);

  return 0;
}
static int xbl_flow(Account *account) {
  /* -- XBL FLOW --*/
  char *xbl_url = "https://user.auth.xboxlive.com/user/authenticate";
  char xbl_body[4096];
  snprintf(xbl_body, sizeof(xbl_body),
           "{ \"Properties\": { \"AuthMethod\": \"RPS\", \"SiteName\": "
           "\"user.auth.xboxlive.com\", \"RpsTicket\": \"%s\" },"
           "\"RelyingParty\": \"http://auth.xboxlive.com\", \"TokenType\": "
           "\"JWT\" }",
           account->access_token);

  int post_st = auth_post(xbl_url, xbl_body, &resp, &http_code);
  if (post_st) {
    printlog("ERROR", __func__, "An error occured when POST at %d",
             __LINE__); /*debug*/

    free(resp.memory);
    return 1;
  }
  if (http_code == 400) {
    reset_buffer(&resp);
    snprintf(xbl_body, sizeof(xbl_body),
             "{ \"Properties\": { \"AuthMethod\": \"RPS\", \"SiteName\": "
             "\"user.auth.xboxlive.com\", \"RpsTicket\": \"d=%s\" },"
             "\"RelyingParty\": \"http://auth.xboxlive.com\", \"TokenType\": "
             "\"JWT\" }",
             account->access_token);
    post_st = auth_post(xbl_url, xbl_body, &resp, &http_code);
    if (post_st) {
      printlog("ERROR", __func__, "An error occured when POST at %d",
               __LINE__); /*debug*/

      free(resp.memory);
      return 1;
    }
  }

  cJSON *xbl_json = cJSON_Parse(resp.memory);
  if (!xbl_json) {
    fprintf(stderr, "cJSON parse failed: %s\n", resp.memory);

    free(resp.memory);
    return 1;
  }
  cJSON *xbl_tok = cJSON_GetObjectItem(xbl_json, "Token");
  cJSON *xui = cJSON_GetObjectItem(
      cJSON_GetObjectItem(xbl_json, "DisplayClaims"), "xui");
  cJSON *uhs = cJSON_GetObjectItem(cJSON_GetArrayItem(xui, 0), "uhs");

  account->xbl_token = strdup(xbl_tok->valuestring);
  account->uhs =
      strdup(uhs->valuestring); /* this is same across xbl/xsts flow */

  cJSON_Delete(xbl_json);

  reset_buffer(&resp);
  return 0;
}
/* -- XSTS FLOW --*/
int xsts_flow(Account *account) {

  char *xsts_url = "https://xsts.auth.xboxlive.com/xsts/authorize";
  char xsts_body[2048];

  snprintf(xsts_body, sizeof(xsts_body),
           "{ \"Properties\": { \"SandboxId\": \"RETAIL\", \"UserTokens\": "
           "[\"%s\"] },"
           " \"RelyingParty\": \"rp://api.minecraftservices.com/\", "
           "\"TokenType\": \"JWT\" }",
           account->xbl_token);

  int post_st = auth_post(xsts_url, xsts_body, &resp, &http_code);

  if (post_st) {
    printlog("ERROR", __func__, "An error occured when POST at %d",
             __LINE__); /*debug*/

    free(resp.memory);
    return 1;
  }

  cJSON *xsts_json = cJSON_Parse(resp.memory);
  if (!xsts_json) {
    fprintf(stderr, "cJSON parse failed: %s\n", resp.memory);
    free(resp.memory);
    return 1;
  }
  if (http_code == 401) {
    cJSON *xerr_obj = cJSON_GetObjectItem(xsts_json, "XErr");

    long XErr = (long)xerr_obj->valuedouble;
    switch (XErr) {
    case XERR_NO_XBOX_ACCOUNT:
      printlog("ERROR", __func__,
               "The specified Microsoft account does not have a valid "
               "Xbox profile.");
      goto abort;
    case XERR_COUNTRY_BANNED:
      printlog("ERROR", __func__,
               "The specified Microsoft account is from a country where Xbox "
               "Live is not available/banned.");
      goto abort;
    case XERR_CHILD_ACCOUNT:
      printlog("ERROR", __func__,
               "The account is a child (under 18) and cannot proceed unless "
               "the account is added to a Family by an adult.");
      goto abort;
    case XERR_ADULT_VERIFY_1:
      printlog("ERROR", __func__,
               " The account needs adult verification on the Xbox page.");
      goto abort;
    case XERR_ADULT_VERIFY_2:
      printlog("ERROR", __func__,
               " The account needs adult verification on the Xbox page.");
      goto abort;
    default:
      printlog("ERROR", __func__, " Recieved unknown XErr: %ld", XErr);
      goto abort;
    }

  abort:
    cJSON_Delete(xsts_json);
    free(resp.memory);
    return 1;
  }

  cJSON *xsts_tok = cJSON_GetObjectItem(xsts_json, "Token");
  account->xsts_token = strdup(xsts_tok->valuestring);

  cJSON_Delete(xsts_json);

  reset_buffer(&resp);
  return 0;
}
/* -- MINECRAFTSERVICES FLOW -- */
static int mcsvc_flow(Account *account) {
  char *mcsvc_url =
      "https://api.minecraftservices.com/authentication/login_with_xbox";
  char mcsvc_body[4096];

  snprintf(mcsvc_body, sizeof(mcsvc_body),
           "{ \"identityToken\": \"XBL3.0 x=%s;%s\" }", account->uhs,
           account->xsts_token);

  int post_st = auth_post(mcsvc_url, mcsvc_body, &resp, &http_code);
  if (post_st) {
    printlog("ERROR", __func__, "An error occured when POST at %d",
             __LINE__); /*debug*/
    free(resp.memory);
    return 1;
  }

  cJSON *mcsvc_json = cJSON_Parse(resp.memory);
  if (!mcsvc_json) {
    fprintf(stderr, "cJSON parse failed: %s\n", resp.memory);
    free(resp.memory);
    return 1;
  }

  cJSON *mcsvc_tokenobj = cJSON_GetObjectItem(mcsvc_json, "access_token");
  if (!mcsvc_tokenobj) {
    fprintf(stderr, "no access_token in response: %s\n", resp.memory);
    free(resp.memory);
    return 1;
  }
  cJSON *mc_expiryobj = cJSON_GetObjectItem(mcsvc_json, "expires_in");
  if (mc_expiryobj) {
    account->mc_expiry = time(NULL) + (long)cJSON_GetNumberValue(mc_expiryobj);
  }
  account->mc_token = strdup(mcsvc_tokenobj->valuestring);

  cJSON_Delete(mcsvc_json);

  reset_buffer(&resp);
  return 0;
}
/* -- MINECRAFT PROFILE FLOW -- */
static int profile_flow(Account *account) {
  char *profile_url = "https://api.minecraftservices.com/minecraft/profile";

  int get_st = auth_get(profile_url, account->mc_token, &resp, &http_code);
  if (get_st) {
    printlog("ERROR", __func__, "An error occured when GET at %d",
             __LINE__); /*debug*/
    free(resp.memory);
    return 1;
  }

  cJSON *profile_json = cJSON_Parse(resp.memory);
  if (cJSON_GetObjectItem(profile_json, "error")) {
    printlog("ERROR", __func__,
             "A valid Minecraft profile was not found for "
             "this account! Make sure you own Minecraft: Java Edition before "
             "retrying, or if you "
             "haven't, create a Minecraft profile on https://minecraft.net.");
    return 1;
  }

  if (!profile_json) {
    fprintf(stderr, "cJSON parse failed: %s\n", resp.memory);
    free(resp.memory);
    return 1;
  }
  cJSON *uuid_obj = cJSON_GetObjectItem(profile_json, "id");
  if (!uuid_obj) {
    fprintf(stderr, "no id in response: %s\n", resp.memory);
    free(resp.memory);
    return 1;
  }
  cJSON *name_obj = cJSON_GetObjectItem(profile_json, "name");
  if (!name_obj) {
    fprintf(stderr, "no name in response: %s\n", resp.memory);
    free(resp.memory);
    return 1;
  }

  account->uuid = strdup(uuid_obj->valuestring);
  account->name = strdup(name_obj->valuestring);

  cJSON_Delete(profile_json);

  reset_buffer(&resp);
  return 0;
}

static int save_creds(const Account *account, char *path) {
  cJSON *creds = cJSON_CreateObject();

  if (!cJSON_AddStringToObject(creds, "refresh_token",
                               account->refresh_token)) {
    printlog("ERROR", __func__, "Failed to add refresh_token to JSON");
    goto fail;
  }
  if (!cJSON_AddNumberToObject(creds, "expires_in",
                               (double)account->ms_expiry)) {
    printlog("ERROR", __func__, "Failed to add expires_In to JSON");
    goto fail;
  }
  if (!cJSON_AddStringToObject(creds, "ygg_token", account->mc_token)) {
    printlog("ERROR", __func__, "Failed to add ygg_token to JSON");
    goto fail;
  }
  if (!cJSON_AddNumberToObject(creds, "mc_expiry",
                               (double)account->mc_expiry)) {
    printlog("ERROR", __func__, "Failed to add mc_expiry to JSON");
    goto fail;
  }
  if (!cJSON_AddStringToObject(creds, "uuid", account->uuid)) {
    printlog("ERROR", __func__, "Failed to add uuid to JSON");
    goto fail;
  }
  if (!cJSON_AddStringToObject(creds, "name", account->name)) {
    printlog("ERROR", __func__, "Failed to add name to JSON");
    goto fail;
  }

  mkdirs(path);
  char tmp[256];
  snprintf(tmp, sizeof(tmp), "%s.tmp", path);
  
  FILE *f = fopen(tmp, "w");
  if (!f) {
    printlog("ERROR", __func__,
             "Failed to open file for saving credentials: %s", strerror(errno));
    goto fail;
  }

  char *out = cJSON_Print(creds);
  fputs(out, f);
  fflush(f);

  int fd = fileno(f);
  fsync(fd);
  fclose(f);


  
  if (rename(tmp, path) ) { 
    perror("Rename failed");
  }

  free(out);
  cJSON_Delete(creds);
  return 0;

fail:
  cJSON_Delete(creds);
  return 1;
}

void auth_flow() {

  char path[512];
  snprintf(path, sizeof(path), "%s/zap/creds.json.tmp", minecraft_path);
  Account account = {0};

  if (!file_exists(path)) {

    if (msa_auth(&account))
      goto fail;

    if (xbl_flow(&account))
      goto fail;

    if (xsts_flow(&account))
      goto fail;

    if (mcsvc_flow(&account))
      goto fail;

    if (profile_flow(&account))
      goto fail;
  } else {
    char *cred_buf = read_file(path);
    cJSON *creds = cJSON_Parse(cred_buf);
    if (!creds) {
      printlog("ERROR", __func__,
               "Failed to parse credentials, the file "
               "may be corrupted. Deleting!");
      unlink(path);
      goto fail;
    }
    cJSON *ms_expiry = cJSON_GetObjectItem(creds, "expires_in");
    cJSON *mc_expiry = cJSON_GetObjectItem(creds, "mc_expiry");

    if (time(NULL) + 7200 > ms_expiry->valuedouble ||
        time(NULL) + 7200 > mc_expiry->valuedouble) {
      cJSON *refresh_token_obj = cJSON_GetObjectItem(creds, "refresh_token");
      account.refresh_token = strdup(refresh_token_obj->valuestring);
      if (msa_refresh(&account))
        goto fail;
      if (xbl_flow(&account))
        goto fail;
      if (xsts_flow(&account))
        goto fail;
      if (mcsvc_flow(&account))
        goto fail;
      if (profile_flow(&account))
        goto fail;
    } else
      return;
  }
save:
  if (save_creds(&account, path))
    goto fail;
  free_shit(&account);
  return;

fail:
  printlog("ERROR", __func__, "Error occured during authentication.");
  printlog("INFO", __func__, "Aborting!");
  exit(1);
}