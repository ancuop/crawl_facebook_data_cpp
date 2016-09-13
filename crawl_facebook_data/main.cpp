#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <json/json.h>
//#include <expat.h>
#include <curl/curl.h>
#include <malloc.h>
#include <iostream>
#include <json/value.h> 
#include "csvparser.h"
#include <ctime>
#include <queue>
#include <pthread.h>

// TODO 
// should reference later: 
//    https://curl.haxx.se/libcurl/c/multi-uv.html
// may use this queue to store data 
//    https://github.com/cameron314/concurrentqueue
#define WRITE_FILE

struct PageInfo {
    std::string page_id;
    int is_empty = 0;
};
struct MemoryStruct {
  char *memory;
  size_t size;
};

const char *file_dir = "./facebook_vn_brands.csv"; 

std::string BASE = "https://graph.facebook.com/v2.6/";
std::string POST = "/posts";
std::string FIELDS = "/?fields=message,link,created_time,type,name,id,"
                                "comments.limit(0).summary(true),shares,reactions"
                                ".limit(0).summary(true)";
std::string LIMIT = "&limit=";
std::string APP_ID = "???";                         // should not share with any one
std::string APP_SECRET = "???";    // should not share with any one
std::string ACCESS_TOKEN = "&access_token=" + APP_ID + "|" + APP_SECRET;

int max_status = 100;   // number of status of a facebook page need to crawl
int max_times = 20;
int max_thread = 10;

static pthread_mutex_t  *lock = NULL;
#define lock_queue()       pthread_mutex_lock(lock)
#define unlock_queue()      pthread_mutex_unlock(lock)


static size_t getDataFromWeb(void *contents, size_t length, size_t nmemb,
                                  void *userp)
{
    size_t real_size = length * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;
    mem->memory = (char *)realloc(mem->memory, mem->size + real_size + 1);
    if (mem->memory == NULL) {
        /* Out of memory. */ 
        fprintf(stderr, "Not enough memory (realloc returned NULL).\n");
        return 0;
    }
    memcpy(&(mem->memory[mem->size]), contents, real_size);
    mem->size += real_size;
    mem->memory[mem->size] = 0;
    
    return real_size;
}

#ifdef WRITE_FILE
    static size_t write_to_file(void *ptr, size_t size, size_t nmemb, void *stream)
    {
        fprintf (stderr, "size: %zu, nmemb: %zu\n", size, nmemb);
        size_t written = fwrite(ptr, size, nmemb, (FILE *)stream);
        return written;
    }
#endif


std::queue<struct PageInfo> queue_page;
    
struct PageInfo get_page_info (void)
{
    struct PageInfo pageinfo;
    
    lock_queue();
    if (queue_page.size()) {
        pageinfo = queue_page.front();
        queue_page.pop();
    } else {
        pageinfo.is_empty = 1;
    }
    unlock_queue();
    
    return pageinfo;
}

static void *thread_read_fb(void *url)
{
    struct PageInfo page_info;
    
    while (1) 
    {
        page_info = get_page_info();
        if (page_info.is_empty == 1) {
            /* Queue is empty */
            return NULL;
        }

        std::string URL;
        URL = BASE +  page_info.page_id + POST + FIELDS + 
                                LIMIT + std::to_string(max_status) + ACCESS_TOKEN;

#ifdef WRITE_FILE
        std::string filename;
        std::string TXT = ".txt";
        std::string DIR = "dataTempMulti2/";
        filename = DIR + page_info.page_id + TXT;
        FILE *pagefile;

        pagefile = fopen(filename.c_str(), "wb");
        if (!pagefile) {
            std::cout << "ERR can not open file: " << filename << std::endl; 
            continue;
        }
#endif      

        CURL *curl_handle;
        CURLcode res;

        struct MemoryStruct *mem;
        mem = (struct MemoryStruct *)calloc(1, sizeof(struct MemoryStruct));

        /* Initialize a libcurl handle. */ 

        curl_handle = curl_easy_init();
        curl_easy_setopt(curl_handle, CURLOPT_URL, URL.c_str());
        curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, getDataFromWeb);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)mem);

        /* Perform the request and any follow-up parsing. */ 
        res = curl_easy_perform(curl_handle);
        if(res != CURLE_OK) {
            std::cout << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        } else {
            Json::Value jsonData;
            Json::Reader jsonReader;

            if (jsonReader.parse(mem->memory, jsonData))
            {
                Json::Value defaultValue;
                /* May remove encoding if not need */
                std::string encoding = jsonData.get("encoding", "UTF-8" ).asString();
                Json::Value data = jsonData.get("data", defaultValue);
                std::cout << "data size: " << data.size() << std::endl;
                for (int i=0; i<data.size(); i++) {
//                    std::cout << "------------\n";
//                    std::cout << "link: " << data[i]["link"] << std::endl;
//                    std::cout << "type: " << data[i]["type"] << std::endl;
//                    std::cout << "id: " << data[i]["id"] << std::endl;
//                    std::cout << "message: " << data[i]["message"] << std::endl;
//                    std::cout << "------------\n";
                    
                    size_t written = fwrite(data.toStyledString().c_str(), 1, mem->size, pagefile);
                }
            }
            else
            {
                std::cout << "Could not parse HTTP data as JSON" << std::endl;
            }

        }

#ifdef WRITE_FILE
        fclose(pagefile);
#endif
        free(mem->memory);
        free(mem);
        
        /* Clean up. */ 
        curl_easy_cleanup(curl_handle);
    }
    
}

/* The CSV file has 3 column: 1: page_id, 2: Name, 3: secret */
int main(void)
{
    int err;

    curl_global_init(CURL_GLOBAL_ALL | CURL_GLOBAL_SSL);
    /* Parse CSV file to get url */
    /* \t: each column separate by tab */
    CsvParser *csvparser = CsvParser_new(file_dir, "\t", 0);
    CsvRow *row;
    
    /* Put all Page Id to queue */
    while (row = CsvParser_getRow(csvparser)) 
    {
        /* Each read is a row of csv file */
        const char **rowFields = CsvParser_getFields(row);
        struct PageInfo page_info;
        page_info.page_id = rowFields[0];
        /* Push page info to queue */
        queue_page.push(page_info);
        
        /* Destroy row */
        CsvParser_destroy_row(row);
        
        if (--max_times == 0) {
            break;
        }
        
    }
    /* Destroy parser */
    CsvParser_destroy(csvparser);
 
    /* Create multi thread */
    lock = (pthread_mutex_t *) calloc(1, sizeof (pthread_mutex_t));
    if (pthread_mutex_init(lock, NULL)) {
        perror("Unable to initialize mutex for server");
        return EXIT_FAILURE;
    }
            
    pthread_t thread_id[max_thread];
    for (int i=0; i<max_thread; i++) {
        err = pthread_create(&thread_id[i], NULL, thread_read_fb, NULL);
        if (err != 0) {
            std::cout << "Could not run thread: " << i << std::endl;
        } else {
            std::cout << "Run thread: " << i << std::endl;
        }
    }
    
    for (int i=0; i<max_thread; i++) {
        err = pthread_join(thread_id[i], NULL);
        std::cout << "Thread terminated: " << i << std::endl;
    }


    /* Destroy mutex lock */
    pthread_mutex_destroy(lock);
    free(lock);
    
    curl_global_cleanup();
    
    fprintf(stderr, "\n----- end -----!\n");
    
    return EXIT_SUCCESS;
}
