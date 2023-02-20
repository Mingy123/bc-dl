#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>
#include <unistd.h>

//#define RED   "\x1B[31m"
//#define GRN   "\x1B[32m"
//#define YEL   "\x1B[33m"
//#define CYN   "\x1B[36m"
//#define WHT   "\x1B[37m"
//#define RESET "\x1B[0m"

char* LEGALFILENAME = "1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM `~?!@#$%^&()-_=+[]{};',.\"";
struct response {
    char* item;
    size_t len;
};

size_t wfunc (char* new_item, size_t size, size_t n, struct response *prev) {
    size_t got_len = size*n;
    size_t new_len = prev->len + got_len;
    
    // change the size of char* item, then append it to the end
    prev->item = realloc (prev->item, new_len+1);
    memcpy (prev->item+prev->len, new_item, got_len);
    prev->item[new_len] = '\0';
    prev->len = new_len;

    return got_len;
}

struct response get_site (char* site) {
    CURL *curl = curl_easy_init();
    if (!curl) {
        printf("Error creating handle\n");
        exit(EXIT_FAILURE);
    }

    struct response item;
    // init the item
    item.len = 0;
    item.item = malloc(1);
    item.item[0] = '\0';

    curl_easy_setopt(curl, CURLOPT_URL, site);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, wfunc);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &item);

    CURLcode result = curl_easy_perform(curl);
    if (result != CURLE_OK) {
        printf("Error getting %s: %s\n", site, curl_easy_strerror(result));
        exit(EXIT_FAILURE);
    }

    curl_easy_cleanup(curl);
    return item;
}

// return true if valid
int fileChar (char chr) {
    char* good = LEGALFILENAME;
    while (*good != '\0') {
        if (*good == chr) return 1;
        good++;
    } return 0;
}
int intLen (int n) {
    if (n == 0) return 1;
    int ans = 0;
    while (n != 0) {
        ans++;
        n /= 10;
    } return ans;
}

void write_to_file (char* filename, char* site) {
    FILE *outfile = fopen(filename, "wb");
    struct response output = get_site(site);
    fwrite(output.item, output.len, 1, outfile);
    fclose(outfile);
    free(output.item);
    printf("\x1B[32m(%d)\n\x1B[0m", output.len);
}

char* fixName (char* og, int zeros, char* ext) {
    char* ans = malloc(strlen(og) + zeros + strlen(ext) + 1);
    char *read = og, *write = ans;
    for (int i = 0; i < zeros; i++) {
        *(write++) = '0';
    }

    while (*read != '\0') {
        if (*read == '&') {
            read += 5;
            if (*read == '#') *write = atoi(read+1);
            else *write = '&';
            write++;
            read = strstr(read, ";")+1;
        } else if (fileChar(*read)) *(write++) = *(read++);
        else read++;
    }

    while (*ext != '\0') *(write++) = *(ext++);
    *write = '\0';
    ans = realloc(ans, strlen(ans) + 1);
    return ans;
}


void parse_playlist (char* site) {
    struct response output = get_site(site);

    char* title1 = strstr(output.item, "<title>") + 7;
    char* title2 = title1;
    while (*title2 != '|' && *title2 != '<') title2++;
    char *title = malloc(title2-title1); memcpy(title, title1, title2-title1);
    title[title2-title1-1] = '\0';

    char *names = strstr(output.item, "name=\"description\"");
    if ((names = strstr(names, "\n1.")) == NULL) names = title;
    else names++;
    char *url = output.item, *brEnd, *nextLine = names;

    if (mkdir(title, 0755) == -1) {
        printf("Error creating directory: %s. If it previously existed, remove it.\n", title);
        exit(EXIT_FAILURE);
    } if (chdir(title) == -1) {
        printf("Error entering directory: %s\n", title);
        exit(EXIT_FAILURE);
    }

    int fileN = 1, fileC = 0;
    char* getC = names;
    while (atoi(getC) > fileC) {
        fileC = atoi(getC);
        while (*getC != '\n') getC++;
        getC++;
    }

    printf("\nGetting playlist: %s\n", title);
    while ((url = strstr(url, "t4.bcbits.com/stream/")) != NULL ) {
        brEnd = url;
        assert(brEnd == url && names == nextLine);
        while (*brEnd != '}') brEnd++;
        *brEnd = '\0';

        while (*nextLine != '\n' && *nextLine != '\0') nextLine++;
        *nextLine = '\0';
        char* newname = fixName(names, intLen(fileC)-intLen(fileN), ".mp3");
        printf("%s ", newname);
        fflush(stdout);
        write_to_file (newname, url);
        free(newname);

        url = ++brEnd;
        names = ++nextLine;
        fileN++;
    } free(title);

    chdir("..");

    free(output.item);
}


void parse_mainpage (char* site) {
    struct response output = get_site(site);

    char* title1 = strstr(output.item, "<title>") + 7;
    char* title2 = title1;
    while (*title2 != '<') title2++;
    char *title = malloc(title2-title1 + 1);
    memcpy(title, title1, title2-title1 + 1);
    title[title2-title1] = '\0';

    if (mkdir(title, 0755) == -1) {
        printf("Error creating directory: %s. If it previously existed, remove it.\n", title);
        exit(EXIT_FAILURE);
    } if (chdir(title) == -1) {
        printf("Error entering directory: %s\n", title);
        exit(EXIT_FAILURE);
    }

    char *start = strstr(output.item, "<ol id=\"music-grid\""), *end = strstr(start, "</ol>");
    char *url = start, *urlEnd, *link;
    while ((url = strstr(url, "<a href=\"") + 9) < end) {
        urlEnd = strstr(url, "\"");
        *urlEnd = '\0';

        link = malloc(strlen(site) + strlen(url) + 1);
        strcpy(link, site);
        strcat(link, url);

        parse_playlist(link);
        url = urlEnd + 1;
        free(link);
    }
}


int main(int argc, char **argv) {
    if (argc != 3) {
        printf("Usage: %s\n-a --album 'site' : download every playlist from the main thing\n-p --playlist 'site' : download every file from playlist\n", argv[0]);
    }

    else if (strcmp(argv[1], "-a") == 0 || strcmp(argv[1], "--album") == 0) {
        parse_mainpage(argv[2]);
    } else if (strcmp(argv[1], "-p") == 0 || strcmp(argv[1], "--playlist") == 0) {
        parse_playlist(argv[2]);
    }

    else {
        printf("Usage: %s\n-a --album 'site' : download every playlist from the main thing\n-p --playlist 'site' : download every file from playlist\n", argv[0]);
    }
}
