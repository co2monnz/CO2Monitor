#include <cJSON.h>

struct config_data {
    globals::RestoringGlobalsComponent<int> *var;
    void (*sync)(double);
    double (*fetch)();
};
std::map<const char *, config_data> CONFIG_VARS;

void logJsonError(const char *msg)
{
    const char *errstr = cJSON_GetErrorPtr();
    if (errstr != NULL)
    {
        ESP_LOGE("setConfig", "%s: %s", msg, errstr);
    }
    else
    {
        ESP_LOGE("setConfig", msg);
    }
}

void logSerializationError(const char *item) {
    const char *errstr = cJSON_GetErrorPtr();
    if (errstr != NULL)
    {
        ESP_LOGE("getConfig", "error serializing %s to config: %s",item, errstr);
    }
    else
    {
        ESP_LOGE("getConfig", "error serializing %s to config", item);
    }
}

void setGlobal(globals::RestoringGlobalsComponent<int> *var, int value) {
    // What a palaver, first set the new value by calling the globals.set action manually,
    // which updates it in memory only
    globals::GlobalVarSetAction<globals::RestoringGlobalsComponent<int>> *setAction;
    setAction = new globals::GlobalVarSetAction<globals::RestoringGlobalsComponent<int>>(var);
    setAction->set_value(value);
    setAction->play();
    // Then make the global variable (in memory) write itself to the NVS queue
    var->loop();
    // next global_preferences->sync() needs to be called to actually persist the values to NVS,
    // but we don't do that here, to allow multiple values to be updated at once.
}

void publishConfiguration() {
    char idS[64];
    sprintf(idS, "co2monitor-dev/%u", id(node_id));
    char topic[256];
    sprintf(topic, "%s/up/config", idS);
    cJSON *config = cJSON_CreateObject();
    if (config == NULL)
    {
        ESP_LOGE("getConfig", "Error creating config JSON");
        return;
    }
    for (auto it = CONFIG_VARS.begin(); it != CONFIG_VARS.end(); it++) {
        cJSON *t = cJSON_CreateNumber(id(it->second.var));
        if (t == NULL) {
            logSerializationError(it->first);
            return;
        }
        cJSON_AddItemToObject(config, it->first, t);
    }
    char *configStr = cJSON_Print(config);
    if (configStr == NULL)
    {
        ESP_LOGE("getConfig", "Error serializing config JSON");
        return;
    }

    if (!id(mqttclient).publish(topic, configStr, strlen(configStr), 0, false)) {
        ESP_LOGE("getConfig", "Error publishing config");
    }
    ESP_LOGI("getConfig", "Published config to %s: %s", topic, configStr);
}

// Provisioning handler
void mqttSetNodeId(const std::string &topic, const std::string &payload)
{
    ESP_LOGD("setNodeId", "Got new nodeID: %s", payload.c_str());
    int newId = atoi(payload.c_str());
    setGlobal(node_id, newId);
    // Then force NVS to save now (instead of waiting 1 minute)
    global_preferences->sync();
    // So we can hopefully reboot with the new ID.
    ESP_LOGD("setNodeId", "nodeID is now : %u. Rebooting to action.", id(node_id));
    if (newId == id(node_id))
    {
        App.reboot();
    }
}

// OTA handler
void mqttOTA(const std::string &topic, const std::string &payload)
{
    ESP_LOGD("ota", "Got ota request: %s", payload.c_str());
    UpgradeFirmware(payload.c_str());
}

class MySCD : public esphome::scd4x::SCD4XComponent {
    public:
        double altitude() { return altitude_compensation_; }
        double temp_offset() { return temperature_offset_; }
};

void syncBrightness(double brightness) {
    double b = brightness;
    b /= 255;
    ESP_LOGD("syncBrightness", "Setting brightness to %.1f from %f", b, brightness);
    id(leds).turn_on().set_brightness(b).perform();
}
double getBrightness() {
    float bf;
    id(leds).current_values_as_brightness(&bf);
    return double(bf*255);
}

void syncAltitude(double altitude) {
    id(scd40).set_altitude_compensation(altitude);
}
double getAltitude() {
    MySCD *t = (MySCD *)&id(scd40);
    return t->altitude();
}

void syncTempOffset(double tempOffset) {
    id(scd40).set_temperature_offset(tempOffset);
}

double getTempOffset() {
    MySCD *t = (MySCD *)&id(scd40);
    return t->temp_offset();
}
// Config handler
void mqttSetConfig(const std::string &topic, const std::string &payload)
{
    ESP_LOGD("setConfig", "Got new config: %s", payload.c_str());
    cJSON *json = cJSON_Parse(payload.c_str());
    if (json == NULL)
    {
        logJsonError("Error parsing JSON");
        return;
    }
    bool sync_nvs = false;
    for (auto it = CONFIG_VARS.begin(); it != CONFIG_VARS.end(); it++) {
        const cJSON *t = cJSON_GetObjectItemCaseSensitive(json, it->first);
        if (cJSON_IsNumber(t)) {
            ESP_LOGD("setConfig", "Setting %s to %d", it->first, t->valueint);
            if (it->second.var != NULL ) {
                setGlobal(it->second.var, t->valueint);
                sync_nvs = true;
            }
            if (it->second.sync != NULL) {
                it->second.sync(t->valuedouble);
            }
        }
    }
    if (sync_nvs) {
        global_preferences->sync();
    }
}

void mqttGetConfig(const std::string &topic, const std::string &payload)
{
    ESP_LOGD("setConfig", "Got config request");
    publishConfiguration();
}

void mqttDebugDump(const std::string &topic, const std::string &payload)
{
    ESP_LOGD("setConfig", "Got debug dump request");
    id(scd40).dump_config();
    App.schedule_dump_config();
}


void mqttSetup() {
    // Setup config var mapping.
    CONFIG_VARS.insert(std::make_pair("co2GreenThreshold", config_data{co2Green, NULL, NULL}));
    CONFIG_VARS.insert(std::make_pair("co2YellowThreshold", config_data{co2Orange, NULL, NULL}));
    CONFIG_VARS.insert(std::make_pair("co2RedThreshold", config_data{co2Red, NULL, NULL}));
    CONFIG_VARS.insert(std::make_pair("co2DarkRedThreshold", config_data{co2DarkRed, NULL, NULL}));
    CONFIG_VARS.insert(std::make_pair("altitude", config_data{NULL, syncAltitude, getAltitude}));
    CONFIG_VARS.insert(std::make_pair("tempOffset", config_data{NULL, syncTempOffset, getTempOffset}));
    CONFIG_VARS.insert(std::make_pair("brightness", config_data{NULL, syncBrightness, getBrightness}));
    // And sync config values to other components
    /*for (auto it = CONFIG_VARS.begin(); it != CONFIG_VARS.end(); it++) {
        if (it->second.sync != NULL) {
            it->second.sync();
        }
    }*/
    // Initialize MQTT listeners and publishers
    char idS[64];
    sprintf(idS, "co2monitor-dev/%u", id(node_id));
    id(mqttclient).set_topic_prefix(idS);
    char topic[256];
    // Provisioning handler
    sprintf(topic, "%s/down/setNodeId", idS);
    id(mqttclient).subscribe(topic, mqttSetNodeId, 2);
    // OTA handler
    sprintf(topic, "%s/down/ota", idS);
    id(mqttclient).subscribe(topic, mqttOTA, 2);
    // Config handler
    sprintf(topic, "%s/down/setConfig", idS);
    id(mqttclient).subscribe(topic, mqttSetConfig, 2);
    sprintf(topic, "%s/down/getConfig", idS);
    id(mqttclient).subscribe(topic, mqttGetConfig, 2);
    sprintf(topic, "%s/down/debugDump", idS);
    id(mqttclient).subscribe(topic, mqttDebugDump, 2);
    ESP_LOGD("mqtt", "Subscriptions setup");
}