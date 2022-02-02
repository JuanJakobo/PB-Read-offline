//------------------------------------------------------------------
// pocket.cpp
//
// Author:           JuanJakobo
// Date:             22.04.2021
//
//-------------------------------------------------------------------

#include "util.h"
#include "log.h"
#include "eventHandler.h"
#include "pocket.h"
#include "pocketModel.h"


#include <string>
#include <vector>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

using std::string;
using std::vector;

Pocket::Pocket()
{
    if (iv_access(CONFIG_PATH.c_str(), W_OK) == 0)
        _accessToken = Util::accessConfig(Action::IReadSecret,"AccessToken");

    if(_accessToken.empty())
        loginDialog();
}

void Pocket::loginDialog(){
    try{
        auto code = getCode();
        auto dialogResult = DialogSynchro(ICON_QUESTION, "Action",("Please type the URL below into your browser to grant this application access to Pocket. (It is normal that the page fails to load) \n https://getpocket.com/auth/authorize?request_token=" + code).c_str(), "Done", "Cancel", NULL);
        switch (dialogResult)
        {
            case 1:
                {
                    try{
                        _accessToken =  getAccessToken(code);
                        Util::accessConfig(Action::IWriteSecret,"AccessToken",_accessToken);
                    }
                    catch (const std::exception &e)
                    {
                        Log::writeErrorLog(e.what());
                        Message(ICON_INFORMATION, "Error while downloading", e.what(), 1200);
                        loginDialog();
                    }

                    break;
                }
            default:
                CloseApp();
                break;
        }
    }
    catch (const std::exception &e)
    {
        Log::writeErrorLog(e.what());
        Message(ICON_INFORMATION, "Error while downloading", e.what(), 1200);
    }
}

string Pocket::getCode(){
    nlohmann::json j = post("oauth/request","{\"consumer_key\":\"" + CONSUMER_KEY + "\", \"redirect_uri\":\"github.com\"}");
    if (j["code"].is_string())
        return j["code"];
    return "error";
}

string Pocket::getAccessToken(const string &code){
    nlohmann::json j = post("oauth/authorize","{\"consumer_key\":\"" + CONSUMER_KEY + "\", \"code\":\"" + code  + "\"}");
    if (j["access_token"].is_string())
        return j["access_token"];
    //TODO get also user
    return "error";
    //save authorize code
}

vector<PocketItem> Pocket::getItems(bool unread, bool archive, bool favorited)
{
    string postData = "{\"consumer_key\":\"" + CONSUMER_KEY + "\", \"access_token\":\"" + _accessToken  + "\", \"count\":\"10\",\"detailType\":\"complete\",";
    postData += "\"state\":";
    if(unread && archive)
        postData += "\"all\"";
    else if(!unread && archive)
        postData += "\"archive\"";
    else if(unread && !archive)
        postData += "\"unread\"";

    if(favorited)
        postData += ",\"favorite\":\"1\"";

    postData += "}";

    nlohmann::json j = post("get",postData);
    //nlohmann::json j = post("get","{\"consumer_key\":\"" + CONSUMER_KEY + "\", \"access_token\":\"" + accessToken  + "\", \"count\":\"10\",\"detailType\":\"complete\", \"contentType\":\"article\"}");
    //use since
    //favorite 1 for favorite
    //contentType? -> articles?
    //get here also the images!

    vector<PocketItem> tempItems;

    int since = j["since"];

    for (const auto &element : j["list"].items()){

//if has_video is 2 --> is a video
//is_article is 1 --> is article

        PocketItem temp;
        //also needs to get the rest
        if(!element.value()["is_article"].is_null()){
            string is_article = element.value()["is_article"];
            if(is_article == "1"){
                if(element.value()["item_id"].is_string()){
                    temp.id = element.value()["item_id"];
                }
                if(element.value()["resolved_title"].is_string())
                    temp.title = element.value()["resolved_title"];
                if(element.value()["given_url"].is_string())
                    temp.url = element.value()["given_url"];
                if(!element.value()["favorite"].is_null())
                {
                    if(element.value()["favorite"] == "0")
                        temp.starred = false;
                    else
                        temp.starred = true;
                }
                if(!element.value()["status"].is_null())
                {
                    if(element.value()["status"] == "1")
                        temp.status = "archived";
                    else if(element.value()["status"] == "2")
                        temp.status = "shall be deleted";
                    else
                        temp.status = "unread";
                }
                if(element.value()["excerpt"].is_string())
                    temp.excerpt = element.value()["excerpt"];
                //word_count --> only for article
                //time_added
                //lang
                if(element.value()["time_to_read"].is_number()){
                    temp.reading_time = element.value()["time_to_read"];
                }

                //has_images --> contains array images --> item_id, image_id, src, widht, height
                //if(element.value()["has_images"] > 0)

                tempItems.push_back(temp);
            }
        }
    }

    return tempItems;

}

nlohmann::json Pocket::post(const string &apiEndpoint, const string &data)
{
    if(!Util::connectToNetwork())
        Log::writeInfoLog("no internet");

    string url = POCKET_URL + apiEndpoint;

    string readBuffer;
    CURLcode res;
    CURL *curl = curl_easy_init();

    if (curl)
    {

        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json; charset=UTF-8");
        headers = curl_slist_append(headers, "X-Accept: application/json");
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Util::writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());

        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        if (res == CURLE_OK)
        {
            long response_code;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

            switch (response_code)
            {
            case 200:
                return nlohmann::json::parse(readBuffer);

           //TODO get http status and x-error-code 
           //handle invalid access token !
            default:
                throw std::runtime_error("HTML Error Code" + std::to_string(response_code));
            }
        }
        else
        {
            Log::writeErrorLog("pocket API: " + url + " RES Error Code: " + std::to_string(res));
            throw std::runtime_error("Curl RES Error Code " + std::to_string(res));
        }
    }
    return {};
}
