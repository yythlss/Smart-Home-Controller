#include "smart_home_http_server.h"

#include <cJSON.h>
#include <esp_log.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>

#define TAG "SmartHomeHttp"

namespace {
constexpr uint16_t kSmartHomeHttpPort = 8080;

bool TokenEquals(const char* value, const char* expected) {
    if (value == nullptr || expected == nullptr) {
        return false;
    }
    while (*value != '\0' && *expected != '\0') {
        if (std::toupper(static_cast<unsigned char>(*value)) !=
            std::toupper(static_cast<unsigned char>(*expected))) {
            return false;
        }
        ++value;
        ++expected;
    }
    return *value == '\0' && *expected == '\0';
}

const char* JsonString(cJSON* root, const char* key) {
    cJSON* item = cJSON_GetObjectItem(root, key);
    return cJSON_IsString(item) ? item->valuestring : nullptr;
}

bool JsonBool(cJSON* root, const char* key, bool default_value) {
    cJSON* item = cJSON_GetObjectItem(root, key);
    if (cJSON_IsBool(item)) {
        return cJSON_IsTrue(item);
    }
    if (cJSON_IsNumber(item)) {
        return item->valueint != 0;
    }
    return default_value;
}

int JsonInt(cJSON* root, const char* key, int default_value) {
    cJSON* item = cJSON_GetObjectItem(root, key);
    return cJSON_IsNumber(item) ? item->valueint : default_value;
}

float JsonFloat(cJSON* root, const char* key, float default_value) {
    cJSON* item = cJSON_GetObjectItem(root, key);
    return cJSON_IsNumber(item) ? static_cast<float>(item->valuedouble) : default_value;
}

const char* StatusText(int status_code) {
    switch (status_code) {
        case 200:
            return "200 OK";
        case 204:
            return "204 No Content";
        case 400:
            return "400 Bad Request";
        case 500:
            return "500 Internal Server Error";
        default:
            return "500 Internal Server Error";
    }
}
} // namespace

SmartHomeHttpServer::SmartHomeHttpServer(SmartHomeController* controller)
    : controller_(controller) {
}

SmartHomeHttpServer::~SmartHomeHttpServer() {
    Stop();
}

bool SmartHomeHttpServer::Start() {
    if (server_ != nullptr) {
        return true;
    }
    if (controller_ == nullptr) {
        ESP_LOGE(TAG, "Cannot start without SmartHomeController");
        return false;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = kSmartHomeHttpPort;
    config.ctrl_port = kSmartHomeHttpPort + 1;
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.stack_size = 6144;
    config.max_uri_handlers = 10;

    esp_err_t err = httpd_start(&server_, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start failed: %s", esp_err_to_name(err));
        server_ = nullptr;
        return false;
    }

    httpd_uri_t state = {};
    state.uri = "/api/state";
    state.method = HTTP_GET;
    state.handler = StateHandler;
    state.user_ctx = this;
    httpd_register_uri_handler(server_, &state);

    httpd_uri_t history = {};
    history.uri = "/api/history";
    history.method = HTTP_GET;
    history.handler = HistoryHandler;
    history.user_ctx = this;
    httpd_register_uri_handler(server_, &history);

    httpd_uri_t device = {};
    device.uri = "/api/device";
    device.method = HTTP_POST;
    device.handler = DeviceHandler;
    device.user_ctx = this;
    httpd_register_uri_handler(server_, &device);

    httpd_uri_t mode = {};
    mode.uri = "/api/mode";
    mode.method = HTTP_POST;
    mode.handler = ModeHandler;
    mode.user_ctx = this;
    httpd_register_uri_handler(server_, &mode);

    httpd_uri_t environment = {};
    environment.uri = "/api/environment";
    environment.method = HTTP_POST;
    environment.handler = EnvironmentHandler;
    environment.user_ctx = this;
    httpd_register_uri_handler(server_, &environment);

    httpd_uri_t context = {};
    context.uri = "/api/context";
    context.method = HTTP_POST;
    context.handler = ContextHandler;
    context.user_ctx = this;
    httpd_register_uri_handler(server_, &context);

    httpd_uri_t alarm_ack = {};
    alarm_ack.uri = "/api/alarm/ack";
    alarm_ack.method = HTTP_POST;
    alarm_ack.handler = AlarmAckHandler;
    alarm_ack.user_ctx = this;
    httpd_register_uri_handler(server_, &alarm_ack);

    httpd_uri_t options = {};
    options.uri = "/api/*";
    options.method = HTTP_OPTIONS;
    options.handler = OptionsHandler;
    options.user_ctx = this;
    httpd_register_uri_handler(server_, &options);

    ESP_LOGI(TAG, "Mini program HTTP API started on port %u: /api/state /api/history /api/device /api/mode /api/environment /api/context /api/alarm/ack",
        kSmartHomeHttpPort);
    return true;
}

void SmartHomeHttpServer::Stop() {
    if (server_ != nullptr) {
        httpd_stop(server_);
        server_ = nullptr;
    }
}

esp_err_t SmartHomeHttpServer::StateHandler(httpd_req_t* req) {
    return static_cast<SmartHomeHttpServer*>(req->user_ctx)->HandleState(req);
}

esp_err_t SmartHomeHttpServer::HistoryHandler(httpd_req_t* req) {
    return static_cast<SmartHomeHttpServer*>(req->user_ctx)->HandleHistory(req);
}

esp_err_t SmartHomeHttpServer::DeviceHandler(httpd_req_t* req) {
    return static_cast<SmartHomeHttpServer*>(req->user_ctx)->HandleDevice(req);
}

esp_err_t SmartHomeHttpServer::ModeHandler(httpd_req_t* req) {
    return static_cast<SmartHomeHttpServer*>(req->user_ctx)->HandleMode(req);
}

esp_err_t SmartHomeHttpServer::EnvironmentHandler(httpd_req_t* req) {
    return static_cast<SmartHomeHttpServer*>(req->user_ctx)->HandleEnvironment(req);
}

esp_err_t SmartHomeHttpServer::ContextHandler(httpd_req_t* req) {
    return static_cast<SmartHomeHttpServer*>(req->user_ctx)->HandleContext(req);
}

esp_err_t SmartHomeHttpServer::AlarmAckHandler(httpd_req_t* req) {
    return static_cast<SmartHomeHttpServer*>(req->user_ctx)->HandleAlarmAck(req);
}

esp_err_t SmartHomeHttpServer::OptionsHandler(httpd_req_t* req) {
    return static_cast<SmartHomeHttpServer*>(req->user_ctx)->HandleOptions(req);
}

esp_err_t SmartHomeHttpServer::HandleState(httpd_req_t* req) {
    return SendJson(req, controller_->BuildStateJson());
}

esp_err_t SmartHomeHttpServer::HandleHistory(httpd_req_t* req) {
    return SendJson(req, controller_->BuildHistoryJson());
}

esp_err_t SmartHomeHttpServer::HandleDevice(httpd_req_t* req) {
    cJSON* root = nullptr;
    if (!ReadJsonBody(req, &root)) {
        return SendError(req, 400, "invalid json body");
    }

    const char* device = JsonString(root, "device");
    if (device == nullptr) {
        device = JsonString(root, "target");
    }
    const bool power = JsonBool(root, "power", true);
    const int level = JsonInt(root, "level", power ? 1 : 0);

    bool handled = false;
    if (TokenEquals(device, "purifier") || TokenEquals(device, "air_purifier")) {
        controller_->SetPurifier(power, level);
        handled = true;
    } else if (TokenEquals(device, "fresh_air") || TokenEquals(device, "fan")) {
        controller_->SetFreshAir(power, level);
        handled = true;
    } else if (TokenEquals(device, "humidifier")) {
        controller_->SetHumidifier(power, level);
        handled = true;
    } else if (TokenEquals(device, "light")) {
        controller_->SetLight(power);
        handled = true;
    }
    cJSON_Delete(root);

    if (!handled) {
        return SendError(req, 400, "unknown device");
    }
    return SendJson(req, controller_->BuildStateJson());
}

esp_err_t SmartHomeHttpServer::HandleMode(httpd_req_t* req) {
    cJSON* root = nullptr;
    if (!ReadJsonBody(req, &root)) {
        return SendError(req, 400, "invalid json body");
    }

    const char* mode = JsonString(root, "mode");
    const bool power = JsonBool(root, "power", true);

    bool handled = false;
    if (TokenEquals(mode, "auto")) {
        controller_->SetAutoMode(power);
        handled = true;
    } else if (TokenEquals(mode, "eco")) {
        controller_->SetEcoMode(power);
        handled = true;
    }
    cJSON_Delete(root);

    if (!handled) {
        return SendError(req, 400, "unknown mode");
    }
    return SendJson(req, controller_->BuildStateJson());
}

esp_err_t SmartHomeHttpServer::HandleEnvironment(httpd_req_t* req) {
    cJSON* root = nullptr;
    if (!ReadJsonBody(req, &root)) {
        return SendError(req, 400, "invalid json body");
    }

    const bool enabled = JsonBool(root, "enabled", true);
    const char* preset = JsonString(root, "preset");
    bool handled = true;
    if (!enabled) {
        controller_->SetManualEnvironmentMode(false);
    } else if (preset != nullptr) {
        handled = controller_->SetEnvironmentPreset(preset);
    } else {
        const EnvironmentSample last_sample = controller_->GetLastSample();
        const float temperature_c = JsonFloat(root, "temperature_c",
            last_sample.has_temperature ? last_sample.temperature_c : 26.0f);
        const float humidity_percent = JsonFloat(root, "humidity_percent",
            last_sample.has_humidity ? last_sample.humidity_percent : 55.0f);
        const int air_score = JsonInt(root, "air_score", last_sample.air_score > 0 ? last_sample.air_score : 88);
        const int mq135_raw = JsonInt(root, "mq135_raw", -1);
        controller_->SetManualEnvironment(temperature_c, humidity_percent, air_score, mq135_raw);
    }
    cJSON_Delete(root);

    if (!handled) {
        return SendError(req, 400, "unknown environment preset");
    }
    return SendJson(req, controller_->BuildStateJson());
}

esp_err_t SmartHomeHttpServer::HandleContext(httpd_req_t* req) {
    cJSON* root = nullptr;
    if (!ReadJsonBody(req, &root)) {
        return SendError(req, 400, "invalid json body");
    }

    bool handled = false;
    cJSON* occupied = cJSON_GetObjectItem(root, "occupied");
    if (cJSON_IsBool(occupied) || cJSON_IsNumber(occupied)) {
        controller_->UpdatePresence(cJSON_IsTrue(occupied) ||
                                    (cJSON_IsNumber(occupied) && occupied->valueint != 0));
        handled = true;
    }

    cJSON* ambient_light = cJSON_GetObjectItem(root, "ambient_light_percent");
    if (cJSON_IsNumber(ambient_light)) {
        controller_->UpdateAmbientLight(static_cast<float>(ambient_light->valuedouble));
        handled = true;
    }
    cJSON_Delete(root);

    if (!handled) {
        return SendError(req, 400, "missing occupied or ambient_light_percent");
    }
    return SendJson(req, controller_->BuildStateJson());
}

esp_err_t SmartHomeHttpServer::HandleAlarmAck(httpd_req_t* req) {
    (void)req;
    controller_->AcknowledgeAlarm();
    return SendJson(req, controller_->BuildStateJson());
}

esp_err_t SmartHomeHttpServer::HandleOptions(httpd_req_t* req) {
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET,POST,OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
    httpd_resp_set_status(req, StatusText(204));
    return httpd_resp_send(req, nullptr, 0);
}

esp_err_t SmartHomeHttpServer::SendJson(httpd_req_t* req, cJSON* json, int status_code) {
    if (json == nullptr) {
        return SendError(req, 500, "json allocation failed");
    }

    char* body = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    if (body == nullptr) {
        return SendError(req, 500, "json serialization failed");
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
    if (status_code != 200) {
        httpd_resp_set_status(req, StatusText(status_code));
    }
    esp_err_t err = httpd_resp_sendstr(req, body);
    cJSON_free(body);
    return err;
}

esp_err_t SmartHomeHttpServer::SendError(httpd_req_t* req, int status_code, const char* message) {
    cJSON* json = cJSON_CreateObject();
    cJSON_AddBoolToObject(json, "ok", false);
    cJSON_AddStringToObject(json, "error", message);
    return SendJson(req, json, status_code);
}

bool SmartHomeHttpServer::ReadJsonBody(httpd_req_t* req, cJSON** root) {
    if (root == nullptr || req->content_len <= 0 || req->content_len > kMaxRequestBodySize) {
        return false;
    }

    std::string body;
    body.resize(req->content_len);
    size_t received = 0;
    while (received < body.size()) {
        const int ret = httpd_req_recv(req, &body[received], body.size() - received);
        if (ret <= 0) {
            return false;
        }
        received += static_cast<size_t>(ret);
    }

    *root = cJSON_ParseWithLength(body.data(), body.size());
    return *root != nullptr;
}
