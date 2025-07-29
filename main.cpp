#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <gumbo.h>
#include <curl/curl.h>
#include "json.hpp"
#include <message.hpp>
#include <mime.hpp>
#include <smtp.hpp>
#include <crow_all.h>
#include <optional>
#include <algorithm>
#include <ctime>

std::string formatToTimestampIST(time_t ts) {
    
    // IST is UTC+5 hours 30 minutes → 19800 seconds
    ts += 19800;
    
    return std::ctime(&ts); // includes newline and fixed format
}

using json = nlohmann::json;
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t totalSize = size * nmemb;
    std::string* buffer = static_cast<std::string*>(userp);
    buffer->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

std::optional<std::string> fetch_html(const std::string& url) {
    std::cout << "[DEBUG] fetch_html() - Attempting to fetch: " << url << "\n";

    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "❌ Failed to initialize cURL\n";
        return std::nullopt;
    }

    std::string html;
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Accept: text/html,application/xhtml+xml");
    headers = curl_slist_append(headers, "Accept-Language: en-US,en;q=0.9");
    headers = curl_slist_append(headers, "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64)");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &html);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 10.0; Win64; x64)");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);  // For detailed trace in Render logs

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        std::cerr << "❌ curl_easy_perform() failed: " << curl_easy_strerror(res) << "\n";
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return std::nullopt;
    }

    long response_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    std::cout << "[DEBUG] HTTP response code: " << response_code << "\n";

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return html;
}

bool sendMail(const std::string& recipient, const std::string& sub, const std::string& body) {
    try {
        std::string senderEmail = std::getenv("SMTP_USER") ? std::getenv("SMTP_USER") : "";
        std::string senderPassword = std::getenv("SMTP_PASS") ? std::getenv("SMTP_PASS") : "";

        if (senderEmail.empty() || senderPassword.empty()) {
            std::cerr << "[WARN] SMTP credentials not set\n";
            return false;
        }

        mailio::message message;
        message.from(mailio::mail_address("Sender", senderEmail));
        message.add_recipient(mailio::mail_address("Recipient", recipient));
        message.subject(sub);
                
        std::string cleanBody = body;
        
        cleanBody.erase(std::remove_if(
                                cleanBody.begin(), 
                                cleanBody.end(), [](char ch) {return (ch >= 0 && ch <= 31) || ch == 127 || ch == '`';}
                                      ), 
                        cleanBody.end());

        message.content_type(mailio::mime::media_type_t::TEXT, "html");
        message.content(cleanBody);
        

        mailio::smtps gmail_smtp("smtp.gmail.com", 465);
        gmail_smtp.authenticate(senderEmail, senderPassword, mailio::smtps::auth_method_t::LOGIN);
        gmail_smtp.submit(message);

        std::cout << "✅ Email sent to " << recipient << "\n";
        return true;
    } catch (const std::exception& ex) {
        std::cerr << "❌ Email error: " << ex.what() << "\n";
        return false;
    }
}

void extract_next_data_script(const std::string& html, std::string& retVal, std::string& json_text) {
    GumboOutput* output = gumbo_parse(html.c_str());

    std::function<void(const GumboNode*)> search_script = [&](const GumboNode* node) {
        if (node->type != GUMBO_NODE_ELEMENT) return;

        const GumboElement& element = node->v.element;
        if (element.tag == GUMBO_TAG_SCRIPT) {
            GumboAttribute* id_attr = gumbo_get_attribute(&element.attributes, "id");
            if (id_attr && std::string(id_attr->value) == "__NEXT_DATA__") {
                for (unsigned int i = 0; i < element.children.length; ++i) {
                    GumboNode* child = static_cast<GumboNode*>(element.children.data[i]);
                    if (child->type == GUMBO_NODE_TEXT) {
                        json_text = child->v.text.text;
                        try {
                            auto json_obj = nlohmann::json::parse(json_text);
                            retVal = json_obj.dump(2);
                        } catch (const std::exception& e) {
                            std::cerr << "❌ JSON parsing failed: " << e.what() << "\n";
                        }
                        return;
                    }
                }
            }
        }

        for (unsigned int i = 0; i < element.children.length; ++i) {
            search_script(static_cast<GumboNode*>(element.children.data[i]));
        }
    };

    search_script(output->root);
    gumbo_destroy_output(&kGumboDefaultOptions, output);
}

std::string extractInRequiredFormat(const nlohmann::json& classes) {
    using json = nlohmann::json;
    //json result = json::array();
    
    std::map<std::string, json> quota_map;
    json train_map;
    
    auto debugClassEntry = [](const json& cls, const std::string& quota, const std::string& class_name, int price, int seats) {
        std::cout << "[DEBUG] quota: " << quota
                  << " | class: " << class_name
                  << " | price: " << price
                  << " | seats: " << seats
                  << std::endl;
    };

    std::cout << "[INFO] extracting data..." << std::endl;

    for (const auto& train : classes) {
        if (!train.contains("classes") || !train["classes"].is_array())
            continue;

        quota_map.clear();
        std::string train_num = train["train"]["number"];
        std::cout << "[INFO] extracting seat avalability for different quotas for train number : " << train_num<< std::endl;
        
        for (const auto& cls : train["classes"]) {
            //no_of_seats = 0;
            //price = 0;
            
            int no_of_seats = 0;
            int price = 0;
            
            if (!cls.contains("quota") || !cls["quota"].contains("key"))
                continue;

            const std::string quota = cls["quota"]["key"];
            if (quota != "GN" && quota != "SS" && quota != "LD")
                continue;

            const std::string class_name = cls.value("class", "UNKNOWN");
            price = cls.value("price", 0);

            
            const std::string status = cls.value("status", "");

            if (status.find("AVL") != std::string::npos) {
                size_t pos = status.find("-");
                if (pos != std::string::npos) {
                    std::string seats = status.substr(pos + 1);
                    try {
                        no_of_seats = std::stoi(seats);
                    } catch (...) {
                        no_of_seats = 0;
                    }
                }
            }
            if(no_of_seats == 0 || price == 0)
                continue;

            debugClassEntry(cls, quota, class_name, price, no_of_seats );

            json class_entry = {
                {"class_name", class_name},
                {"price", price},
                {"no_of_seats", no_of_seats}
            };

            if (quota_map.end() == quota_map.find(quota)) {
                quota_map[quota]["Quota"] = quota;
                quota_map[quota]["classes"] = json::array();
            }

            quota_map[quota]["classes"].push_back(class_entry);
        }
        
        
        if(!quota_map.empty()) {
        
            std::string train_name = train["train"]["name"];
            const int dep_time = train["dep_time"];
            const int arr_time = train["arr_time"];
            
            json train_entry = {
                {"train_name", train_name},
                {"train_number", train_num},
                {"arr_time", arr_time},
                {"dep_time", dep_time},
                
            };       
        
            
            
            for (const auto& [quota, quota_data] : quota_map) {
                
                train_entry[quota] = quota_data;
            }
            //std::cout << "[INFO] building complete json for train number : " << train_num<< std::endl;
            train_map[train_num + " " + train_name] = train_entry;
        }
        else
        {
            std::cout << "[INFO] no seats available in any quotas for train number : " << train_num<< std::endl;
        }
        
    }

    

    std::cout << "[INFO] data extracted successfully.\n" << std::endl;
   
    std::string returVal = "";
    std::ostringstream out;
    
    out << "<html><body>";
       
    for (const auto& [number_name, info] : train_map.items()) {
        
        //out << "Train: " << info.value("train_name", "") << " (" << info.value("train_number", "") << ")\r\n";
        //out << "Departure: " << info["dep_time"] << " | Arrival: " << info["arr_time"] << "\r\n";
        //out << "Quota: " << info["GN"]["Quota"] << "\r\n";
        //out << "Classes:\r\n";
        
        out << "<h3>" << info["train_name"] << " (" << info["train_number"] << ")</h3>";
        out << "<p><b>Departure:</b> " << formatToTimestampIST(info["dep_time"]) << " | <b>Arrival:</b> " << formatToTimestampIST(info["arr_time"]) << "<br>";
        out << "<b>Quota:</b> " << info["GN"]["Quota"] << "</p>";
        
        
        if(info.contains("GN"))
        {
            //out << "-----------------------------\r`\n";
            out << "<ul>";
            for (const auto& cls : info["GN"]["classes"]) {
                //out << " - " << cls["class_name"] << ": " << cls["no_of_seats"] << " seats, ₹" << cls["price"] << "\r\n";
                 out << "<li>" << cls["class_name"] << ": " << cls["no_of_seats"] << " seats, ₹" << cls["price"] << "</li>";
            }
            out << "</ul><hr>";
        }
        
        if(info.contains("LD"))
        {
            //out << "-----------------------------\r\n";
            out << "<ul>";
            for (const auto& cls : info["LD"]["classes"]) {
                out << " - " << cls["class_name"] << ": " << cls["no_of_seats"] << " seats, ₹" << cls["price"] << "\r\n";
            }
            //out << "\n"; // spacing between trains
        }
            out << "</ul><hr>";
        
               
        if(info.contains("SS"))
        {
            //out << "-----------------------------\r\n";
            out << "<ul>";
            for (const auto& cls : info["SS"]["classes"]) {
                out << " - " << cls["class_name"] << ": " << cls["no_of_seats"] << " seats, ₹" << cls["price"] << "\r\n";
            }
            out << "</ul><hr>";
        }       
        
        
    }
    
    out << "</body></html>";
        
    returVal = out.str();
    std::cout << "[INFO] data converrted into readable html.\n" << std::endl;
    
    return returVal;
}


std::string createResponse(const std::string& url, const std::string& receiverEmail, int noOfDays, bool& emailSuccess,
const std::string & src, const std::string & dst, const std::string & date) {
    auto htmlOpt = fetch_html(url);
    if (!htmlOpt) {
        std::cerr << "❌ Failed to fetch HTML from URL: " << url << "\n";
        emailSuccess = false;
        return "";
    }

    
    std::string html = *htmlOpt;

    /*
    std::string retVal;
    std::string json_text;
    extract_next_data_script(html, retVal, json_text);
    
    json data = json::parse(retVal);
    //auto classes = data["props"]["pageProps"]["trainListResponse"]["response"]["results"];
    auto classes = data["props"]["pageProps"]["trainListResponse"]["response"]["result"]["results"];
    auto train = data["props"]["pageProps"]["trainListResponse"]["response"]["result"]["train"];


    
    //return classes.dump(4);
    
    std::string output = extractInRequiredFormat(classes);//
    */

    std::string output = html;//
    
    std::cout << "[INFO] Sending email to " << receiverEmail << " for " << noOfDays << " days\n";
    emailSuccess = sendMail(receiverEmail, std::string("Train Info JSON ") +  src + " - " + dst + " on " + date, output);
    
    
    return output;

}

int main() {
    crow::SimpleApp app;

    CROW_ROUTE(app, "/health")([] {
        return "OK";
    });

    CROW_ROUTE(app, "/send")([](const crow::request& req) {
    const auto& url = req.url_params;

    // 👇 If any query param is missing, show the HTML form instead
    if (!url.get("src") || !url.get("dst") || !url.get("date") ||
        !url.get("email_id") || !url.get("no_of_days")) {

        std::ostringstream html;
        html << "<!DOCTYPE html><html><head><title>Train Availability Form</title></head><body>"
             << "<h2>Check Train Availability</h2>"
             << "<form method='get' action='/send'>"
             << "Source: <input name='src' required><br>"
             << "Destination: <input name='dst' required><br>"
             << "Date (YYYYMMDD): <input name='date' required><br>"
             << "Email: <input name='email_id' type='email' required><br>"
             << "No. of Days: <input name='no_of_days' type='number' required><br>"
             << "<button type='submit'>Submit</button>"
             << "</form></body></html>";

        return crow::response(200, html.str());
    }

    std::string src     = url.get("src");
    std::string dst     = url.get("dst");
    std::string date    = url.get("date");
    std::string email   = url.get("email_id");
    std::string days    = url.get("no_of_days");

    int noOfDays;
    try {
        noOfDays = std::stoi(days);
        if (noOfDays <= 0) return crow::response(400, "Invalid number of days");
    } catch (...) {
        return crow::response(400, "Invalid number of days");
    }

    //const std::string BASE_URL = "https://www.goibibo.com/trains/dsrp";
    //std::string httpsUrl = BASE_URL + "/" + src + "/" + dst + "/" + date + "/GN/";
    std::string httpsUrl = "https://www.ixigo.com/search/result/train/BGM/PUNE/20082025//1/0/0/0/ALL";

    bool emailSent = false;
    std::string urlResponse;

    try {
        urlResponse = createResponse(httpsUrl, email, noOfDays, emailSent, src, dst, date);
    } catch (const std::exception& ex) {
        std::cerr << "[ERROR] Exception: " << ex.what() << "\n";
        return crow::response(500, "Internal server error");
    }

    std::ostringstream body;
    body << "<pre>"
         << "From: " << src << "\nTo: " << dst
         << "\nDate: " << date
         << "\nDuration: " << noOfDays << " days"
         << "\nReceiver Email: " << email
         << "\n\nEmail status: " << (emailSent ? "✅ Sent" : "❌ Failed")
         << "\n\nExtracted JSON:\n" << urlResponse
         << "</pre>";

    return crow::response(200, body.str());
});

    int port = std::getenv("PORT") ? std::stoi(std::getenv("PORT")) : 8080;
    std::cout << "[INFO] Server starting on port " << port << "\n";
    app.port(port).multithreaded().run();
}
