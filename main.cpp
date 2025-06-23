#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <gumbo.h>
#include <curl/curl.h>
#include "json.hpp"
#include <message.hpp>
#include <smtp.hpp>
#include <crow_all.h>


// Callback to write received data into a std::string
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t totalSize = size * nmemb;
    std::string* buffer = static_cast<std::string*>(userp);
    buffer->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

std::string fetch_html(const std::string& url) {
    CURL* curl = curl_easy_init();
    std::string html;

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); // follow redirects
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &html);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "htmljson-parser/1.0");

        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            std::cerr << "❌ curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        }

        curl_easy_cleanup(curl);
    }

    return html;
}

void sendMail(const std::string &receipient, const std::string &sub, const std::string &body)
{
    
    try {
        // Compose the message
        mailio::message message;
        std::string senderEmail = std::getenv("SMTP_USER");
        std::string senderPassword = std::getenv("SMTP_PASS");
        
        message.from(mailio::mail_address("Sender", senderEmail));
        message.add_recipient(mailio::mail_address("Recipient", receipient));
        message.subject(sub);
        message.content(body);

        // Configure SMTP with Gmail (use port 465 for SSL)
        mailio::smtps gmail_smtp("smtp.gmail.com", 465);
        gmail_smtp.authenticate(senderEmail, senderPassword, mailio::smtps::auth_method_t::LOGIN);
        gmail_smtp.submit(message);


        std::cout << "✅ Email sent successfully!" << std::endl;
    } catch (const std::exception& ex) {
        std::cerr << "❌ Error: " << ex.what() << std::endl;
    }
    
}

void extract_next_data_script(const std::string& html, std::string& retVal, std::string& json_text) {
    GumboOutput* output = gumbo_parse(html.c_str());

    std::function<void(const GumboNode*)> search_script;
    search_script = [&](const GumboNode* node) {
        if (node->type != GUMBO_NODE_ELEMENT) return;

        const GumboElement& element = node->v.element;
        if (element.tag == GUMBO_TAG_SCRIPT) {
            GumboAttribute* id_attr = gumbo_get_attribute(&element.attributes, "id");
            if (id_attr && std::string(id_attr->value) == "__NEXT_DATA__") {
                // Extract inner text of the <script> tag
                for (unsigned int i = 0; i < element.children.length; ++i) {
                    GumboNode* child = static_cast<GumboNode*>(element.children.data[i]);
                    if (child->type == GUMBO_NODE_TEXT) {
                        json_text = child->v.text.text;

                        std::cout << "\n----------------------\n";
                        try {
                            auto json_obj = nlohmann::json::parse(json_text);
                            retVal = json_obj.dump(2);
                            std::cout << "✅ Parsed JSON:\n";
                        } catch (const std::exception& e) {
                            std::cerr << "❌ JSON parsing failed: " << e.what() << std::endl;
                        }
                        std::cout << "----------------------\n";
                        return;
                    }
                }
            }
        }

        // Recurse into children
        const GumboVector* children = &element.children;
        for (unsigned int i = 0; i < children->length; ++i) {
            search_script(static_cast<GumboNode*>(children->data[i]));
        }
    };

    search_script(output->root);
    gumbo_destroy_output(&kGumboDefaultOptions, output);
}

std::string createResponse(const std::string & url, const::std::string recieverEmail, int noOfdays)
{
    std::string html = fetch_html(url);
    std::string retVal;
    std::string json_text;
    extract_next_data_script(html, retVal, json_text);

    
    std::cout<<"\nsending mail to "<<recieverEmail <<" , subsribed for "<< noOfdays<< " , ........\n";
    sendMail(recieverEmail, "Mail from krg json formatted", json_text);
    
    
    return retVal;
    
    
}

int main() {
    
    crow::SimpleApp app;

    // Health check
    CROW_ROUTE(app, "/health")([] {
        std::cout << "[INFO] Health check\n";
        return "OK";
    });
    
    // GET-based email trigger
    CROW_ROUTE(app, "/send")
    ([](const crow::request& req) {
        const auto& url = req.url_params;

        if (!url.get("src") || !url.get("dst") || !url.get("date") ||
            !url.get("email_id") || !url.get("no_of_days")) 
            {
                return crow::response(400, "Missing required parameters");
        }

        std::string src = url.get("src");
        std::string dst = url.get("dst");
        std::string date = url.get("date");
        std::string email = url.get("email_id");
        std::string days = url.get("no_of_days");

        std::string subject = "Trip Plan: " + src + " to " + dst;
        std::string body = "From: " + src + "\nTo: " + dst +
                           "\nDate: " + date + 
                           "\nDuration: " + days + " days" +
                           "\nReciever Email : " + email +
                           "\nNo of days to monitor : " + days +
                           "\n\nHave a great trip!\n\n";       
                           
        std::string httpsUrl = "https://www.goibibo.com/trains/dsrp";
        httpsUrl += "/";
        httpsUrl += src;
        httpsUrl += "/";
        httpsUrl += dst;
        httpsUrl += "/";
        httpsUrl += date;
        httpsUrl += "/GN/";
        
        
        

        try {
        
            std::string urlResponse = createResponse(httpsUrl, email, std::stoi(days));
            body += urlResponse;        
            if(!urlResponse.empty())
                std::cout << "[INFO] Sent email to " << email << "\n";
            else
                std::cout << "[INFO] email sending failed, " << email << "\n";
            
            return crow::response(200, body + "\n\n Email sent! \n\n");
        } catch (const std::exception& ex) {
            std::cerr << "[ERROR] " << ex.what() << "\n";
            return crow::response(500, ex.what());
        }
    });

    int port = std::getenv("PORT") ? std::stoi(std::getenv("PORT")) : 8080;
    std::cout << "[INFO] Running on port " << port << "\n";
    app.port(port).multithreaded().run();
    
    
    return 0;
}


std::string read_file_to_string(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::in | std::ios::binary); // binary avoids newline translation
    if (!file) {
        throw std::ios_base::failure("Failed to open file: " + filepath);
    }

    std::ostringstream buffer;
    buffer << file.rdbuf(); // read entire file into buffer
    return buffer.str();    // convert to string
}