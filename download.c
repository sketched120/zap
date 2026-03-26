// Downloader

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <curl/curl.h>

void mkdirs(const char *path) {
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s", path);
    
    // Find the last slash to separate the folder from the filename
    char *last_slash = strrchr(tmp, '/');
    if (!last_slash) return; // No slashes, nothing to create

    for (char *p = tmp + 1; p < last_slash; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    // Create the final parent directory
    *last_slash = '\0';
    mkdir(tmp, 0755);
}

static size_t write_cb(char *ptr, size_t size, size_t nmemb, void *ud) {
    return fwrite(ptr, size, nmemb, (FILE *)ud);
}

void download_files(char **urls, char **dests, int n) {
    CURLM *multi = curl_multi_init();

    FILE *fps[n];  // track open file handles

    for (int i = 0; i < n; i++) {
        if (access(dests[i], F_OK) == 0) {
            fps[i] = NULL;  // already exists, skip
            continue;
        }
        
        mkdirs(dests[i]);
        fps[i] = fopen(dests[i], "wb");

        CURL *easy = curl_easy_init();
        curl_easy_setopt(easy, CURLOPT_URL,            urls[i]);
        curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION,  write_cb);
        curl_easy_setopt(easy, CURLOPT_WRITEDATA,      fps[i]);
        curl_easy_setopt(easy, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(easy, CURLOPT_PRIVATE,        fps[i]); // stash fp
        curl_multi_add_handle(multi, easy);
    }

    int still_running = 1;
    while (still_running) {
        curl_multi_perform(multi, &still_running);

        CURLMsg *msg;
        int left;
        while ((msg = curl_multi_info_read(multi, &left))) {
            if (msg->msg != CURLMSG_DONE) continue;

            CURL *easy = msg->easy_handle;

            FILE *fp;
            curl_easy_getinfo(easy, CURLINFO_PRIVATE, &fp);
            fclose(fp);

            curl_multi_remove_handle(multi, easy);
            curl_easy_cleanup(easy);
        }

        if (still_running)
            curl_multi_poll(multi, NULL, 0, 1000, NULL);
    }

    curl_multi_cleanup(multi);
}

void download_file(const char *url, const char *dest) {
    mkdirs(dest);
    // if (file_exists(dest)) return;

    FILE *fp = fopen(dest, "wb");
    if (!fp) return;

    CURL *easy = curl_easy_init();
    curl_easy_setopt(easy, CURLOPT_URL,            url);
    curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION,  write_cb);
    curl_easy_setopt(easy, CURLOPT_WRITEDATA,      fp);
    curl_easy_setopt(easy, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_perform(easy);  // blocking, single file
    curl_easy_cleanup(easy);
    fclose(fp);
}
