#ifndef SMART_HOME_HTTP_SERVER_H
#define SMART_HOME_HTTP_SERVER_H

#include "smart_home_controller.h"

#include <esp_http_server.h>

class SmartHomeHttpServer {
public:
    explicit SmartHomeHttpServer(SmartHomeController* controller);
    ~SmartHomeHttpServer();

    bool Start();
    void Stop();

private:
    static constexpr size_t kMaxRequestBodySize = 512;

    static esp_err_t StateHandler(httpd_req_t* req);
    static esp_err_t HistoryHandler(httpd_req_t* req);
    static esp_err_t DeviceHandler(httpd_req_t* req);
    static esp_err_t ModeHandler(httpd_req_t* req);
    static esp_err_t EnvironmentHandler(httpd_req_t* req);
    static esp_err_t ContextHandler(httpd_req_t* req);
    static esp_err_t AlarmAckHandler(httpd_req_t* req);
    static esp_err_t OptionsHandler(httpd_req_t* req);

    esp_err_t HandleState(httpd_req_t* req);
    esp_err_t HandleHistory(httpd_req_t* req);
    esp_err_t HandleDevice(httpd_req_t* req);
    esp_err_t HandleMode(httpd_req_t* req);
    esp_err_t HandleEnvironment(httpd_req_t* req);
    esp_err_t HandleContext(httpd_req_t* req);
    esp_err_t HandleAlarmAck(httpd_req_t* req);
    esp_err_t HandleOptions(httpd_req_t* req);

    esp_err_t SendJson(httpd_req_t* req, cJSON* json, int status_code = 200);
    esp_err_t SendError(httpd_req_t* req, int status_code, const char* message);
    bool ReadJsonBody(httpd_req_t* req, cJSON** root);

    SmartHomeController* controller_ = nullptr;
    httpd_handle_t server_ = nullptr;
};

#endif // SMART_HOME_HTTP_SERVER_H
