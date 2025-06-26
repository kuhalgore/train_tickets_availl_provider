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
#include <optional>

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
        message.content(body);

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

std::string createResponse(const std::string& url, const std::string& receiverEmail, int noOfDays, bool& emailSuccess) {
    auto htmlOpt = fetch_html(url);
    if (!htmlOpt) {
        std::cerr << "❌ Failed to fetch HTML from URL: " << url << "\n";
        emailSuccess = false;
        return "";
    }

    std::string html = *htmlOpt;
    std::string retVal;
    std::string json_text;
    extract_next_data_script(html, retVal, json_text);

    std::cout << "[INFO] Sending email to " << receiverEmail << " for " << noOfDays << " days\n";
    emailSuccess = sendMail(receiverEmail, "Train Info JSON", json_text);

    return retVal;
}

int main() {
    crow::SimpleApp app;

    CROW_ROUTE(app, "/health")([] {
        return "OK";
    });

    CROW_ROUTE(app, "/send")([](const crow::request& req) {
        const auto& url = req.url_params;

        if (!url.get("src") || !url.get("dst") || !url.get("date") ||
            !url.get("email_id") || !url.get("no_of_days")) {
            return crow::response(400, "Missing required parameters");
        }

        std::string src = url.get("src");
        std::string dst = url.get("dst");
        std::string date = url.get("date");
        std::string email = url.get("email_id");
        std::string days = url.get("no_of_days");

        int noOfDays;
        try {
            noOfDays = std::stoi(days);
            if (noOfDays <= 0) return crow::response(400, "Invalid number of days");
        } catch (...) {
            return crow::response(400, "Invalid number of days");
        }

        const std::string BASE_URL = "https://www.goibibo.com/trains/dsrp";
        std::string httpsUrl = BASE_URL + "/" + src + "/" + dst + "/" + date + "/GN/";

        bool emailSent = false;
        std::string urlResponse;

        try {
            urlResponse = createResponse(httpsUrl, email, noOfDays, emailSent);
        } catch (const std::exception& ex) {
            std::cerr << "[ERROR] Exception: " << ex.what() << "\n";
            return crow::response(500, "Internal server error");
        }

        std::ostringstream body;
        body << "From: " << src << "\nTo: " << dst
             << "\nDate: " << date
             << "\nDuration: " << noOfDays << " days"
             << "\nReceiver Email: " << email
             << "\n\nEmail status: " << (emailSent ? "✅ Sent" : "❌ Failed")
             << "\n\nExtracted JSON:\n" << urlResponse;

        return crow::response(200, body.str());
    });

    int port = std::getenv("PORT") ? std::stoi(std::getenv("PORT")) : 8080;
    std::cout << "[INFO] Server starting on port " << port << "\n";
    app.port(port).multithreaded().run();
}
