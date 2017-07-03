/*
MIT License

Copyright (c) 2017 Michael Armbruster

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#define _VERSION "0.1.2"
#define BUFFER_LEN 1024
#define PCRE2_CODE_UNIT_WIDTH 8

#if defined(WIN32) || defined(__WIN32__) || defined(__TOS_WIN__) || defined(__WINDOWS__)
    #ifndef _WIN32
        #define _WIN32 1
    #endif
#endif

#ifdef _WIN32
    #include <io.h>
    #include <windows.h>
    #ifndef access
        #define access _access
    #endif
#else
    #include <pthread.h>
    #include <pwd.h>
    #include <unistd.h>
#endif

#include <fcntl.h>
#include <pcre2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <curl/curl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "utarray.h"
#include "uthash.h"
#include "utstring.h"

struct memstruct
{
    char *memory;
    size_t size;
};

struct threads
{
    char *key;
    int running;
    #ifdef _WIN32
        HANDLE thread;
    #else
        pthread_t thread;
    #endif
    UT_hash_handle hh;
};

struct argument
{
    char *streamurl;
    UT_string *outdir;
    struct threads *thread;
};

inline char separator()
{
    #ifdef _WIN32
        return '\\';
    #else
        return '/';
    #endif
}

int dir_exists(const char* absolutePath)
{
    if (access(absolutePath, 0) == 0)
    {
        struct stat status;
        if (stat(absolutePath, &status) == 0)
        {
            return (status.st_mode & S_IFDIR) != 0;
        }
    }

    return 0;
}

const char* getconfdir()
{
    const char* confdir = NULL;

    #ifdef _WIN32
        if ((confdir = getenv("APPDATA")) == NULL)
        {
            confdir = "C:\\";
        }
    #else
        if ((confdir = getenv("HOME")) == NULL)
        {
            confdir = getpwuid(getuid())->pw_dir;
        }
    #endif

    return confdir;
}

static size_t write_cb(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    struct memstruct *mem = (struct memstruct *)userp;

    mem->memory = realloc(mem->memory, mem->size + realsize + 1);
    if(mem->memory == NULL) {
        /* out of memory! */
        printf("not enough memory (realloc returned NULL)\n");
        return 0;
    }

    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

#ifdef _WIN32
DWORD WINAPI recordstream(LPVOID data)
#else
void *recordstream(void *data)
#endif
{
    UT_string *outfile;
    UT_string *command;
    FILE *process = NULL;
    time_t t = time(NULL);
    struct argument *args = (struct argument*)data;
    struct tm tm = *localtime(&t);

    utstring_new(outfile);
    utstring_new(command);

    utstring_concat(outfile, args->outdir);
    utstring_printf(outfile, "%s_%04d-%02d-%02d_%02d-%02d-%02d.mp4", args->thread->key, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    utstring_printf(command, "ffmpeg -i %s -c:a copy -c:v copy %s", args->streamurl, utstring_body(outfile));
    #ifdef _WIN32
        utstring_printf(command, " > NUL 2>&1");
    #else
        utstring_printf(command, " > /dev/null 2>&1");
    #endif

    printf("Recording for model \"%s\" starting via ffmpeg\nRecorded file will be %s_%04d-%02d-%02d_%02d-%02d-%02d.mp4 in the output folder\n\n", args->thread->key, args->thread->key, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

    process = popen(utstring_body(command), "r");
    if (process != NULL)
    {
        pclose(process);
    }
    else
    {
        fprintf(stderr, "Error: We cannot execute ffmpeg!\n");
    }

    utstring_free(outfile);
    utstring_free(command);
    printf("Recording for model \"%s\" finished\n", args->thread->key);
    args->thread->running = 0;
    #ifdef _WIN32
        return 0;
    #else
        pthread_exit(NULL);
    #endif
}

int main(int argc, char *argv[])
{
    UT_string *outdir;
    UT_string *confdir;
    UT_string *modelfile_str;
    UT_string *model;
    UT_string *modelurl;
    UT_string *streamurl;
    UT_array *models;
    FILE *modelfile;
    pcre2_code *regex;
    pcre2_match_data *match_data;
    PCRE2_SIZE erroroffset, *ovector;
    CURL *curl;
    CURLcode res;
    struct memstruct chunk;
    struct threads *recording = NULL;
    struct threads *thread;
    struct argument *threaddata;
    int fd, errorcode, regexcount;
    char tmp[BUFFER_LEN];
    char *tmpcopy;
    char **p;
    char separator_str[2];
    separator_str[0] = separator();
    separator_str[1] = '\0';

    curl_global_init(CURL_GLOBAL_ALL);
    utstring_new(outdir);
    utstring_new(confdir);
    utstring_new(modelfile_str);
    utstring_new(model);
    utstring_new(modelurl);
    utstring_new(streamurl);
    utarray_new(models, &ut_str_icd);

    printf("CaptureBateC v%s\n", _VERSION);
    printf("Copyright 2017 Michael Armbruster - Licensed under MIT License\n");
    printf("This application uses parts of the uthash library by Troy. D Hanson\n\n");

    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <outputfolder>\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (!dir_exists(argv[1]))
    {
        fprintf(stderr, "Error: Output directory does not exist!\n");
        return EXIT_FAILURE;
    }
    utstring_renew(outdir);
    utstring_printf(outdir, "%s", argv[1]);
    if (utstring_find(outdir, utstring_len(outdir) - 1, separator_str, 1) == -1)
    {
        utstring_printf(outdir, "%s", separator_str);
    }

    regex = pcre2_compile((const unsigned char*)"http.*/playlist\\.m3u8", PCRE2_ZERO_TERMINATED, 0, &errorcode, &erroroffset, NULL);
    if (regex == NULL)
    {
        fprintf(stderr, "Could not compile regular expression!\n");
        return EXIT_FAILURE;
    }

    utstring_printf(confdir, "%s", getconfdir());
    if (utstring_find(confdir, utstring_len(confdir) - 1, separator_str, 1) == -1)
    {
        utstring_printf(confdir, "%s", separator_str);
    }

    #ifdef _WIN32
        utstring_printf(confdir, "%s%s", "CaptureBate", separator_str);
    #else
        utstring_printf(confdir, "%s%s", ".capturebate", separator_str);
    #endif

    if (!dir_exists(utstring_body(confdir)))
    {
        #ifdef _WIN32
            _mkdir(utstring_body(confdir));
        #else
            mkdir(utstring_body(confdir), 0700);
        #endif

        if (!dir_exists(utstring_body(confdir)))
        {
            fprintf(stderr, "Error: Could not create config dir!\n");
            return EXIT_FAILURE;
        }
    }
    utstring_printf(modelfile_str, "%s%s", utstring_body(confdir), "models.txt");

    fd = open(utstring_body(modelfile_str), O_RDONLY);
    if (fd < 0)
    {
        fprintf(stderr, "Error: Could not find model file at\n%s\n\nPlease create this file and put one model per line\n", utstring_body(modelfile_str));
        return EXIT_FAILURE;
    }
    close(fd);

    while (1)
    {
        modelfile = fopen(utstring_body(modelfile_str), "rt");
        if (modelfile == NULL)
        {
            fprintf(stderr, "Error while opening model.txt for reading, trying again in 10 seconds!\n");
            #ifdef _WIN32
                Sleep(10000);
            #else
                sleep(10);
            #endif
            continue;
        }
        utarray_clear(models);
        while(!feof(modelfile))
        {
            if (fgets(tmp, BUFFER_LEN, modelfile) != NULL)
            {
                utstring_renew(model);
                utstring_bincpy(model, tmp, strlen(tmp) - 1);
                utarray_push_back(models, &utstring_body(model));
            }
        }
        fclose(modelfile);

        for (p = (char**)utarray_front(models); p != NULL; p = (char**)utarray_next(models, p))
        {
            curl = curl_easy_init();
            if (curl)
            {
                chunk.memory = malloc(1);
                chunk.size = 0;

                utstring_renew(modelurl);
                utstring_printf(modelurl, "https://en.chaturbate.com/%s/", *p);

                curl_easy_setopt(curl, CURLOPT_URL, utstring_body(modelurl));
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
                // coverity[bad_sizeof]
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)(&chunk));
                curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_11_5) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/56.0.2924.87 Safari/537.36");
                res = curl_easy_perform(curl);

                if(res != CURLE_OK)
                {
                    fprintf(stderr, "curl_easy_perform() failed: %s\n",
                    curl_easy_strerror(res));
                }
                else
                {
                    match_data = pcre2_match_data_create(20, NULL);
                    regexcount = pcre2_match(regex, (unsigned char*)chunk.memory, -1, 0, PCRE2_NOTEMPTY, match_data, NULL);

                    if (regexcount <= 0)
                    {
                        printf("Model \"%s\" is offline!\n", *p);
                    }
                    else
                    {
                        ovector = pcre2_get_ovector_pointer(match_data);
                        utstring_renew(streamurl);
                        utstring_printf(streamurl, "%.*s", (int)(ovector[1] - ovector[0]), (char*)(chunk.memory + ovector[0]));
                        tmpcopy = (char*)malloc((utstring_len(streamurl) + 1) * sizeof(char));
                        strncpy(tmpcopy, utstring_body(streamurl), utstring_len(streamurl));
                        tmpcopy[utstring_len(streamurl)] = '\0';
                        printf("Assuming stream for model \"%s\" is running\n", *p);
                        HASH_FIND_STR(recording, *p, thread);
                        if (thread == NULL || !thread->running)
                        {
                            if (thread != NULL)
                            {
                                HASH_DEL(recording, thread);
                            }

                            thread = (struct threads*)malloc(sizeof(struct threads));
                            thread->key = (char*)malloc((strlen(*p) + 1) * sizeof(char));
                            thread->running = 1;
                            strncpy(thread->key, *p, strlen(*p));
                            thread->key[strlen(*p)] = '\0';
                            threaddata = (struct argument*)malloc(sizeof(struct argument));
                            threaddata->thread = thread;
                            threaddata->streamurl = tmpcopy;
                            threaddata->outdir = outdir;
                            #ifdef _WIN32
                                thread->thread = CreateThread(NULL, 0, recordstream, (LPVOID)threaddata, 0);
                            #else
                                pthread_create(&(thread->thread), NULL, recordstream, (void*)threaddata);
                                pthread_detach(thread->thread);
                            #endif

                            HASH_ADD_KEYPTR(hh, recording, thread->key, strlen(thread->key), thread);
                        }
                        else
                        {
                            printf("We are already recording model \"%s\"\n", *p);
                        }
                    }

                    printf("\n");
                    pcre2_match_data_free(match_data);
                }

                free(chunk.memory);
                curl_easy_cleanup(curl);
            }
            else
            {
                fprintf(stderr, "Error: Could not initialize cURL!\n");
                continue;
            }
        }

        printf("Waiting 31 seconds before we check the streams again\n");
        #ifdef _WIN32
            Sleep(31000);
        #else
            sleep(31);
        #endif
    }

    curl_global_cleanup();
    pcre2_code_free(regex);
    utstring_free(outdir);
    utstring_free(streamurl);
    utstring_free(modelfile_str);
    utstring_free(confdir);
    utstring_free(model);
    utstring_free(modelurl);
    utarray_free(models);
    return EXIT_SUCCESS;
}
