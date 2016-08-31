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

struct MemoryStruct {
  char *memory;
  size_t size;
};

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

const char *url = "https://graph.facebook.com/v2.6/100473740084971/posts/?fields=message,link,created_time,type,name,id,comments.limit(0).summary(true),shares,reactions.limit(0).summary(true)&limit=100&access_token=154244191671387|20d0bb5dec5b62de17782cf1852a4056";

int main(void)
{
    CURL *curl_handle;
    CURLcode res;
    struct MemoryStruct *mem;
    mem = (struct MemoryStruct *)calloc(1, sizeof(struct MemoryStruct));

    /* Initialize a libcurl handle. */ 
    curl_global_init(CURL_GLOBAL_ALL | CURL_GLOBAL_SSL);
    curl_handle = curl_easy_init();
    curl_easy_setopt(curl_handle, CURLOPT_URL, url);
    curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, getDataFromWeb);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)mem);
 
    /* Perform the request and any follow-up parsing. */ 
    res = curl_easy_perform(curl_handle);
    if(res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
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
    free(mem);
  
    fprintf(stderr, "\n----- end -----!\n");
  
    return EXIT_SUCCESS;
}
