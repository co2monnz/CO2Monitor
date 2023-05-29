#include <cJSON.h>


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

void publishConfiguration() {
    char idS[64];
    sprintf(idS, "co2monitor-dev/%u", id(node_id));
    char topic[256];
    sprintf(topic, "%s/up/config", idS);
    cJSON *config = cJSON_CreateObject();
    if (config == NULL)
    {
        logJsonError("Error creating config JSON");
        return;
    }
    float bf;
    id(leds).current_values_as_brightness(&bf);
    cJSON *b = cJSON_CreateNumber(bf*255);
    if (b == NULL)
    {
        logJsonError("Error serializing brightness to config");
        return;
    }
    char *configStr = cJSON_Print(config);
    if (configStr == NULL)
    {
        logJsonError("Error serializing config JSON");
        return;
    }
    /* after creation was successful, immediately add it to the monitor,
     * thereby transferring ownership of the pointer to it */
    cJSON_AddItemToObject(config, "brightness", b);
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
    // What a palaver, first set the new value by calling the globals.set action manually,
    // which updates it in memory only
    globals::GlobalVarSetAction<globals::RestoringGlobalsComponent<int>> *setAction;
    setAction = new globals::GlobalVarSetAction<globals::RestoringGlobalsComponent<int>>(node_id);
    setAction->set_value(newId);
    setAction->play();
    // Then make the global variable (in memory) write itself to the NVS queue
    node_id->loop();
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
    const cJSON *jsonBrightness = cJSON_GetObjectItemCaseSensitive(json, "brightness");
    if (cJSON_IsNumber(jsonBrightness))
    {
        ESP_LOGD("setConfig", "Setting brightness to %d", jsonBrightness->valueint);
        id(leds).turn_on().set_brightness(jsonBrightness->valueint / 255 * 100).perform();
    }
}
void mqttGetConfig(const std::string &topic, const std::string &payload)
{
    ESP_LOGD("setConfig", "Got config request");
    publishConfiguration();
}