#include <curl/curl.h>
#include <string>
#include <iostream>
#include <vector>

std::size_t callback(const char* in, std::size_t size, std::size_t num, std::string* out) {
    const std::size_t totalBytes(size * num);
    out->append(in, totalBytes);
    return totalBytes;
}
std::string* request(const std::string& url) {
    CURL* curl = curl_easy_init();

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    long httpCode(0);
    std::string* httpData(new std::string());

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, httpData);

    curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);

    if (httpCode == 200) {
        return httpData;
    } else {
        std::cout << "request is not success" << std::endl;
    }
    return nullptr;
}

size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::vector<char>* data) {
    size_t totalSize = size * nmemb;
    data->insert(data->end(), (char*)contents, (char*)contents + totalSize);
    return totalSize;
}

// Function to download content from a URL (for binary data like images)
std::vector<char> download(const std::string& url) {
    CURL* curl;
    CURLcode res;
    std::vector<char> readBuffer;

    curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        res = curl_easy_perform(curl);  // Perform the request
        if(res != CURLE_OK) {
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        }
        curl_easy_cleanup(curl);  // Cleanup
    }
    return readBuffer;
}
