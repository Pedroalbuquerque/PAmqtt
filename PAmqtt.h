
#include <AsyncMqttClient.h>
#include <Ticker.h>

AsyncMqttClient mqttClient;

Ticker mqttReconnectTimer;
Ticker wifiReconnectTimer;


void connectToMqtt() {
  DEBUG_MSG("[mqtt] Connecting to MQTT...");
  mqttClient.connect();
}


#ifdef ESP8266
  // for Esp8266 a trigger was implemented just when the connection is stablished by the client
  // to imediatly do something

  WiFiEventHandler wifiConnectHandler;      // handler for station connect
  WiFiEventHandler wifiDisconnectHandler;   // hander for Station diconnect


  void onWifiConnect(const WiFiEventStationModeGotIP& event) {
    DEBUG_MSG("[Wifi ST] Connected to Wi-Fi.");
    connectToMqtt();
  }

  void onWifiDisconnect(const WiFiEventStationModeDisconnected& event) {
    DEBUG_MSG("[Wifi ST] Disconnected from Wi-Fi.");
    delay(2000); //wait 2 sec and try again
    mqttReconnectTimer.detach(); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
    Esp.WiFiconnect(); // when wifi connect, mqtt will connect also
  }


#elif ESP32
  // for ESP32 still the detection of client connection is made by pooling interval
  wifi_interface_t current_wifi_interface;

  void onWifiConnect(WiFiEvent_t event, WiFiEventInfo_t info){
    //const WiFiEventStationModeGotIP& event) {
    DEBUG_MSG("[WiFi ST] Connected to Wi-Fi.");
    connectToMqtt();
  }

  void onWifiDisconnect(WiFiEvent_t event, WiFiEventInfo_t info) {
    DEBUG_MSG("[WiFi ST] Disconnected from Wi-Fi.");
    delay(2000); //wait 2 sec and try again
    mqttReconnectTimer.detach(); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
    //wifiReconnectTimer.once(2, WiFiconnect); // when wifi connect, mqtt will connect also
    Esp.WiFiconnect(); // when wifi connect, mqtt will connect also
  }

#endif

void mqttBuildTopic(char * topic,uint8_t nodeID, const char* subtopic){
  String tmpTopic;
  tmpTopic = String(MQTT_TOPIC) + String(subtopic);
  tmpTopic.replace("{nodeid}",String(nodeID,DEC) );
  strcpy(topic,tmpTopic.c_str());
  //DEBUG_MSG("[mqttbuild]%s\n",tmpTopic.c_str());
  return;
}

uint16_t mqttPublish(const char *topic,  char *payload){
  //mqttClient.publish(const char *topic, uint8_t qos, bool retain, optional const char *payload, optional size_t length)
  DEBUG_MSG("[mqtt]publish %s\tqos:%d\t payload:%s\n", topic, MQTT_QOS,payload);
  if(!mqttClient.connected()){
    Esp.WiFiconnect();
    mqttClient.connect();
  }
  if(mqttClient.connected())
    return mqttClient.publish(topic, MQTT_QOS, MQTT_RETAIN, payload, strlen(payload));
  else {    
    DEBUG_MSG("[mqtt] Mqtt not connected , not published")
  }
  return 0;
}

void mqttSubscribe(const char *topic, uint8_t qos){
  mqttClient.subscribe(topic, qos);
  DEBUG_MSG("[mqtt]subscribe %s\tqos:%d\n", topic, qos);
}



void onMqttConnect(bool sessionPresent) {
  DEBUG_MSG("[mqtt] Connected to MQTT.\n");
  DEBUG_MSG("[mqtt] Session present: %d\n",sessionPresent);
  char buf[50];
  mqttBuildTopic(buf,NODEID, MQTT_SUBTOPIC_CMD);
  mqttClient.subscribe(buf, 2);
  }

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  DEBUG_MSG("[mqtt]Disconnected from MQTT.\n");

  if (WiFi.isConnected()) {
    mqttReconnectTimer.once(2, connectToMqtt);
  }
}

void onMqttSubscribe(uint16_t packetId, uint8_t qos) {
  DEBUG_MSG("Subscribe acknowledged.");
  DEBUG_MSG("  packetId: %d\n",packetId);
  DEBUG_MSG("  qos: %d\n",qos);
}

void onMqttUnsubscribe(uint16_t packetId) {
  DEBUG_MSG("Unsubscribe acknowledged.");
  DEBUG_MSG("  packetId: %d\n",packetId);
}

void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
  uint8_t nodeID, start, end, topiclen;
  String tmpStr;
  String tmpTopic;
  char tmpPayload[50];
  tmpStr = String(topic);
  topiclen = strlen(tmpStr.c_str());
  end = tmpStr.lastIndexOf("/");
  start = tmpStr.lastIndexOf("/",end-1);
  nodeID = atoi(tmpStr.substring(start+1, end).c_str());
  tmpTopic = tmpStr.substring(end+1,topiclen);
  strncpy(tmpPayload,payload,len);
  tmpPayload[len] = NULL;
  
  DEBUG_MSG("MQTT MSG] topicLen:%d\t len:%d\t total:%d\n",topiclen,len,total);
  DEBUG_MSG("[MQTT MSG] nodeID:%d\ttopic:%s\t payload:%s\n",nodeID,tmpTopic.c_str(),tmpPayload);

  if(nodeID == NODEID){
    if(strcmp(tmpTopic.c_str(),"CMD") == 0) {
      DEBUG_MSG("[MQTT MSG] CMD rceived:%s\n",tmpPayload);
    }

    if(tmpStr.indexOf(MQTT_SUBTOPIC_CMD)!=-1){
        if(JsonDecode(payload)){  // if parsed OK
          //IO.outputSetAll();               // set output acording to CMD topic data
        }
    }
  }

  //DEBUG_MSG("Publish received.\n");
  //DEBUG_MSG("  topic: %s\n", topic);
  //DEBUG_MSG("  NODE ID:%d\t star:%d\t end:%d \n",nodeID,start, end);
  //DEBUG_MSG("  qos: %d\n", properties.qos);
  //DEBUG_MSG("  dup: %d\n",properties.dup);
  //DEBUG_MSG("  retain: %d\n", properties.retain);
  //DEBUG_MSG("  len: %d\n", len);
  //DEBUG_MSG("  index: %d\n",index);
  //DEBUG_MSG("  total: %d\n",total);
  //DEBUG_MSG("msg:%s\n",payload);


}

void onMqttPublish(uint16_t packetId) {
  //DEBUG_MSG("[mqtt] Publish acknowledged.\n");
  DEBUG_MSG("[mqtt on publish]  packetId: %d\n",packetId);
}


void mqttSetup() {

  #ifdef ESP8266
    wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);
    wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnect);

  #else //ESP32
    WiFi.onEvent(onWifiConnect, SYSTEM_EVENT_STA_CONNECTED);
    WiFi.onEvent(onWifiDisconnect, SYSTEM_EVENT_STA_DISCONNECTED);
  #endif


  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onSubscribe(onMqttSubscribe);
  mqttClient.onUnsubscribe(onMqttUnsubscribe);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.onPublish(onMqttPublish);
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);


}

void mqttShutDown(){
  #ifdef ESP8266
    wifiConnectHandler = NULL;
    wifiDisconnectHandler = NULL;

  #else //ESP32
    // ?? WiFi.onEvent(onWifiConnect, SYSTEM_EVENT_STA_CONNECTED);
    // ?? WiFi.onEvent(onWifiDisconnect, SYSTEM_EVENT_STA_DISCONNECTED);
  #endif

  WiFi.disconnect();
  mqttClient.disconnect();

}

uint16_t mqttPubStatus( char * message) {
      char topic[50];
      mqttBuildTopic(topic, NODEID,MQTT_SUBTOPIC_STATUS);
      //DEBUG_MSG("[mqtt loop] topic:%s:%s\n",topic,buf);
      return mqttPublish(topic ,message);
}

void mqttPubDebug( char * message ){
  char topic[50];
  mqttBuildTopic(topic, NODEID,MQTT_SUBTOPIC_DBG);
  mqttPublish(topic ,message);
}

//#endif
