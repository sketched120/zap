// Downloader

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <curl/curl.h>

#include "include/download.h"
#include "include/utils.h"


static size_t write_cb(char *ptr, size_t size, size_t nmemb, void *ud) {
    return fwrite(ptr, size, nmemb, (FILE *)ud);
}

static void print_progress(int current, int total) {
    int bar_width = 40; 
    float progress = (float)current / total;
    int filled_width = progress * bar_width;

    printf("\r[");
    
    for (int i = 0; i < bar_width; i++) {
        if (i < filled_width) printf("█");
        else printf(" ");                  
    }
    
 
    printf("] %d%% \e[K", (int)(progress * 100));
    fflush(stdout); 
}

void download_files(char **urls, char **dests, int n) {
    CURLM *m = curl_multi_init();
    int max = 16; 
    int next = 0;
    int run = 0;
    int finished = 0;

    printf("\e[?25l");
    while (next < n || run > 0) {
        while (next < n && run < max) {
            if (access(dests[next], F_OK) == 0) {
                next++;
                finished++;
                print_progress(finished, n);
                continue;
            }
            
            mkdirs(dests[next]);
            FILE *f = fopen(dests[next], "wb");
            if (!f) { next++; continue; }

            CURL *e = curl_easy_init();
            //printlog("INFO", __func__,"Downloading: %s", urls[next]);
            curl_easy_setopt(e, CURLOPT_URL,            urls[next]);
            curl_easy_setopt(e, CURLOPT_WRITEFUNCTION,  write_cb);
            curl_easy_setopt(e, CURLOPT_WRITEDATA,      f);
            curl_easy_setopt(e, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(e, CURLOPT_PRIVATE,        f); 
            
            curl_multi_add_handle(m, e);
            next++;
            run++; 
        }

        curl_multi_perform(m, &run);

        CURLMsg *msg;
        int q;
        while ((msg = curl_multi_info_read(m, &q))) {
            if (msg->msg != CURLMSG_DONE) continue;

            finished++;
            print_progress(finished, n);
            CURL *e = msg->easy_handle;
            FILE *f;
            curl_easy_getinfo(e, CURLINFO_PRIVATE, &f);
            if (f) fclose(f);

            

            curl_multi_remove_handle(m, e);
            curl_easy_cleanup(e);
        }

        if (run) curl_multi_poll(m, NULL, 0, 1000, NULL);
    }
    curl_multi_cleanup(m);
    printf("\e[?25h\n");
}

int download_file(const char *url, char *dest) {
    mkdirs(dest);
    if (file_exists(dest) == true) return 0;

    FILE *fp = fopen(dest, "wb");
    if (!fp) return 1;

    CURL *easy = curl_easy_init();
    curl_easy_setopt(easy, CURLOPT_URL,            url);
    curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION,  write_cb);
    curl_easy_setopt(easy, CURLOPT_WRITEDATA,      fp);
    curl_easy_setopt(easy, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_perform(easy);  
    curl_easy_cleanup(easy);
    fclose(fp);

    return 0;
}
