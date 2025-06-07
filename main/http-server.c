#include "conf.h"
#include "file-writing.h"
#include "esp_err.h"

#include "esp_http_server.h"
#include "esp_log.h"
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include "http-server.h"

#define TAG "Web_server"

esp_err_t send_favicon(httpd_req_t *req) {
    esp_err_t err = httpd_resp_sendstr_chunk(req, "<link rel=\"icon\" href=\"");
    err |= httpd_resp_sendstr_chunk(req, FAVICON_PATH);
    err |= httpd_resp_sendstr_chunk(req, "\" type=\"image/x-icon\">");
    return err;
}

esp_err_t send_page_name(httpd_req_t *req) {
    esp_err_t err = httpd_resp_sendstr_chunk(req, "<title>");
    err |= httpd_resp_sendstr_chunk(req, PAGE_NAME);
    err |= httpd_resp_sendstr_chunk(req, "</title>");
    return err;
}

esp_err_t list_files(httpd_req_t *req, const char *title, const char *dir_path) {
    DIR *dir = opendir(dir_path);
    if (!dir) {
        httpd_resp_sendstr_chunk(req, "<h3>");
        httpd_resp_sendstr_chunk(req, dir_path);
        httpd_resp_sendstr_chunk(req, " - no files</h3>");
        return ESP_FAIL;
    }

    httpd_resp_sendstr_chunk(req, "<h3>");
    httpd_resp_sendstr_chunk(req, title);
    httpd_resp_sendstr_chunk(req, "</h3><ul>");

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;  // skip dot files

        // Construct URI for onclick
        char uri[256];
        snprintf(uri, sizeof(uri), "%s%s", dir_path + strlen("/littlefs"), entry->d_name);

        // Send the list item with onclick JS
        char item[1024];
        // Then for each file, print a clickable link like:
        snprintf(item, sizeof(item),
                 "<li><button class=\"file-btn\" data-url=\"%s\">%s</button>"
                 "<pre class=\"file-content\"></pre></li>",
                 uri, entry->d_name);
        httpd_resp_sendstr_chunk(req, item);
    }

    httpd_resp_sendstr_chunk(req, "</ul>");
    closedir(dir);
    return ESP_OK;
}

esp_err_t root_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");

    httpd_resp_sendstr_chunk(req, "<html><head><title>ESP32 File Browser</title>");
    send_favicon(req);
    httpd_resp_sendstr_chunk(req,
                             "<style>"
                             "body { background-color: #121212; color: #e0e0e0; font-family: Consolas, 'Courier New', monospace; padding: 1em; }"
                             "h1, h5 { color: #c0c0c0; }"
                             "ul { list-style-type: none; padding-left: 0; }"
                             ".file-btn { "
                             "background: none; border: none; color: #a0a0a0; cursor: pointer; "
                             "font-family: Consolas, 'Courier New', monospace; font-size: 1em; padding: 0; margin: 0; text-align: left; "
                             "text-decoration: underline; "
                             "}"
                             ".file-btn:hover { color: #ffffff; }"
                             ".file-content { "
                             "display: none; background-color: #222222; color: #f0f0f0; border: 1px solid #444444; padding: 10px; margin-top: 0.25em; "
                             "white-space: pre-wrap; font-family: Consolas, 'Courier New', monospace; font-size: 0.9em; "
                             "}"
                             "</style>"
                             );
    httpd_resp_sendstr_chunk(req, "</head><body>");
    httpd_resp_sendstr_chunk(req, "<h1>ESP32 WiFi Scanner</h1>");
    httpd_resp_sendstr_chunk(req, "<p>See scan results below:</p>");

    list_files(req, "Port Files", port_root_dir);
    list_files(req, "IPv4 Info Files", ipv4_info_root_dir);
    list_files(req, "AP Records Files", ap_records_root_dir);

    httpd_resp_sendstr_chunk(req,
                             "<script>"
                             "document.querySelectorAll('.file-btn').forEach(btn => {"
                             "  btn.addEventListener('click', () => {"
                             "    const pre = btn.nextElementSibling;"
                             "    if (pre.style.display === 'none') {"
                             "      pre.style.display = 'block';"
                             "      if (!pre.textContent) {"
                             "        fetch(btn.dataset.url)"
                             "          .then(res => {"
                             "            if (!res.ok) throw new Error('Failed to load file');"
                             "            return res.text();"
                             "          })"
                             "          .then(text => pre.textContent = text)"
                             "          .catch(err => pre.textContent = err.message);"
                             "      }"
                             "    } else {"
                             "      pre.style.display = 'none';"
                             "    }"
                             "  });"
                             "});"
                             "</script>");

    httpd_resp_sendstr_chunk(req, "</body></html>");
    httpd_resp_sendstr_chunk(req, NULL); // End response

    return ESP_OK;
}

const char *get_content_type(const char *filename) {
    if (strstr(filename, ".html")) return "text/html";
    if (strstr(filename, ".css")) return "text/css";
    if (strstr(filename, ".js")) return "application/javascript";
    if (strstr(filename, ".json")) return "application/json";
    if (strstr(filename, ".txt")) return "text/plain";
    if (strstr(filename, ".ico")) return "image/x-icon";
    return "application/octet-stream";  // Default
}

// Catch-all handler for serving files
esp_err_t get_handler(httpd_req_t *req) {
    char filepath[523] = {0};
    snprintf(filepath, sizeof(filepath), BASE_PATH "%s", req->uri);

    ESP_LOGI(TAG, "HTTP Server accessing file: %s", filepath);
    struct stat st;
    if (stat(filepath, &st) == -1 || S_ISDIR(st.st_mode)) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found or is a directory");
        return ESP_FAIL;
    }

    int fd = open(filepath, O_RDONLY);
    if (fd == -1) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to open file");
        return ESP_FAIL;
    }

    const char *content_type = get_content_type(filepath);
    httpd_resp_set_type(req, content_type);

    char buf[128];
    ssize_t read_bytes;
    while ((read_bytes = read(fd, buf, sizeof(buf))) > 0) {
        if (httpd_resp_send_chunk(req, buf, read_bytes) != ESP_OK) {
            close(fd);
            httpd_resp_send_chunk(req, NULL, 0);
            return ESP_FAIL;
        }
    }
    close(fd);
    httpd_resp_send_chunk(req, NULL, 0); // End of response
    return ESP_OK;
}

httpd_uri_t root_get = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = root_get_handler,
    .user_ctx = NULL
};

httpd_uri_t catch_all_get = {
    .uri = "/*",
    .method = HTTP_GET,
    .handler = get_handler,
    .user_ctx = NULL
};

httpd_handle_t start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;
    config.uri_match_fn = httpd_uri_match_wildcard;

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &root_get);       // Handles "/"
        httpd_register_uri_handler(server, &catch_all_get);  // Handles all other requests
    }
    return server;
}

void stop_webserver(httpd_handle_t server) {
    if (server) {
        httpd_stop(server);
    }
}
