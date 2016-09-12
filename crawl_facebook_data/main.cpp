#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <json/json.h>
#include <expat.h>
#include <curl/curl.h>
#include <malloc.h>
#include <iostream>
#include <json/value.h> 
#include "csvparser.h"

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

/* The CSV file has 3 column: 1: page_id, 2: Name, 3: secret */
int main(void)
{
    /* Parse CSV file to get url */
    /* \t: each column separate by time */
    CsvParser *csvparser = CsvParser_new(file_dir, "\t", 0);
    CsvRow *row;
    
    while (row = CsvParser_getRow(csvparser)) {
        /* Each read is a row of csv file */
        const char **rowFields = CsvParser_getFields(row);
        
        std::string URL;
        URL = BASE + rowFields[0] + POST + FIELDS + 
                                LIMIT + std::to_string(max_status) + ACCESS_TOKEN;
#ifdef DEBUG_PRINT
        fprintf(stderr, "===== NEW LINE =====\n");
        for (int i=0; i<CsvParser_getNumFields(row); i++) {
            fprintf(stderr, "FIELD i=%d: %s\n", i, rowFields[i]);
        }
        std::cout << "url: " << URL << std::endl;
#endif
        
        CURL *curl_handle;
        CURLcode res;

        struct MemoryStruct *mem;
        mem = (struct MemoryStruct *)calloc(1, sizeof(struct MemoryStruct));

        /* Initialize a libcurl handle. */ 
        curl_global_init(CURL_GLOBAL_ALL | CURL_GLOBAL_SSL);
        curl_handle = curl_easy_init();
        curl_easy_setopt(curl_handle, CURLOPT_URL, URL.c_str());
        curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, getDataFromWeb);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)mem);

        /* Perform the request and any follow-up parsing. */ 
        res = curl_easy_perform(curl_handle);
        if(res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        } else {
            std::cout << "OK" << std::endl;
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
                    std::cout << "------------\n";
                    std::cout << "link: " << data[i]["link"] << std::endl;
                    std::cout << "type: " << data[i]["type"] << std::endl;
                    std::cout << "id: " << data[i]["id"] << std::endl;
                    std::cout << "message: " << data[i]["message"] << std::endl;
                    std::cout << "------------\n";
                }
                
                std::cout << std::endl;
            }
            else
            {
                std::cout << "Could not parse HTTP data as JSON" << std::endl;
            }
        }

        /* Clean up. */ 
        curl_easy_cleanup(curl_handle);
        curl_global_cleanup();

        free(mem->memory);
        free(mem);
        
        /* Destroy row */
        CsvParser_destroy_row(row);
    }
    
    /* Destroy parser */
    CsvParser_destroy(csvparser);

    fprintf(stderr, "\n----- end -----!\n");
  
    return EXIT_SUCCESS;
}
