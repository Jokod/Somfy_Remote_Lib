#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <EEPROMRollingCodeStorage.h>
#include <ELECHOUSE_CC1101_SRC_DRV.h>
#include <SomfyRemote.h>

WiFiClient espClient;
PubSubClient client(espClient);

const char* ssid = "";
const char* password = "";

const char* mqtt_server = "192.168.1.54";
const unsigned int mqtt_port = 1883;
const char* mqtt_user = "";
const char* mqtt_pass = "";

String clientId = "Somfy";
String mqtt_channel = "Somfy-";

#define EMITTER_GPIO 5

// Rolling codes and remote IDs
#define REMOTE_COUNT 6
#define REMOTE1 0x5184c8
#define REMOTE2 0x65dc01
#define REMOTE3 0x25b5d5
#define REMOTE4 0xc6c78f
#define REMOTE5 0x59714b
#define REMOTE6 0x121313

EEPROMRollingCodeStorage rollingCodeStorage1(0);
EEPROMRollingCodeStorage rollingCodeStorage2(2);
EEPROMRollingCodeStorage rollingCodeStorage3(4);
EEPROMRollingCodeStorage rollingCodeStorage4(6);
EEPROMRollingCodeStorage rollingCodeStorage5(8);
EEPROMRollingCodeStorage rollingCodeStorage6(10);
SomfyRemote somfyRemote1(EMITTER_GPIO, REMOTE1, &rollingCodeStorage1); // Tous
SomfyRemote somfyRemote2(EMITTER_GPIO, REMOTE2, &rollingCodeStorage2); // Chambre Jordan
SomfyRemote somfyRemote3(EMITTER_GPIO, REMOTE3, &rollingCodeStorage3); // Chambre
SomfyRemote somfyRemote4(EMITTER_GPIO, REMOTE4, &rollingCodeStorage4); // Chambre Anne
SomfyRemote somfyRemote5(EMITTER_GPIO, REMOTE5, &rollingCodeStorage5); // Salon
SomfyRemote somfyRemote6(EMITTER_GPIO, REMOTE6, &rollingCodeStorage6); // Cuisine

String demand[6] = { "Wait", "Wait", "Wait", "Wait", "Wait", "Wait" };

SomfyRemote somfyRemotes[] = { somfyRemote1, somfyRemote2, somfyRemote3, somfyRemote4, somfyRemote5, somfyRemote6 };

void setup() {
  Serial.begin(115200);
  Serial.println("Starting Somfy");
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println(WiFi.localIP());

  for (int i = 0; i < REMOTE_COUNT; i++) {
    somfyRemotes[i].setup();
  }

  ELECHOUSE_cc1101.Init();
  ELECHOUSE_cc1101.setMHZ(433.42);

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  EEPROM.begin(2 * REMOTE_COUNT);
}

void callback(char* topic, byte* payload, unsigned int length) {

  if (strncmp(topic, "Somfy-", 6) == 0 && strstr(topic, "/Feedback") == NULL) {

    char demand_str[length + 1];
    strncpy(demand_str, (char*)payload, length);
    demand_str[length] = '\0';

    char blind_number_str[2];
    strncpy(blind_number_str, (char*)&topic[6], 2);
    int blind_number = String(blind_number_str).toInt();

    String string = String(demand_str);

    if (isValidCommand(getSomfyCommand(string))) {
      demand[blind_number] = string;
    } else {
      demand[blind_number] = "Wait";
    }

    Serial.print("Remote[");
    Serial.print(blind_number);
    Serial.print("]: ");
    Serial.println(demand[blind_number]);
  }
}

void sendCC1101Command(Command command, int remote) {
  ELECHOUSE_cc1101.SetTx();
  somfyRemotes[remote].sendCommand(command);
  ELECHOUSE_cc1101.setSidle();
}

void loop() {

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  for (int i = 0; i < REMOTE_COUNT; i++) {

    if (demand[i] == "Wait") {
      continue;
    }

    const Command current_command = getSomfyCommand(demand[i]);

    if (isValidCommand(current_command)) {
      demand[i] = "Wait";

      char feedback_channel[30];
      sprintf(feedback_channel, "Somfy-%d/Feedback", i);

      client.publish(feedback_channel, getCommandString(current_command).c_str());
      client.subscribe(feedback_channel);

      Serial.print("Send Command to Remote n°");
      Serial.print(i);
      Serial.print(": ");
      Serial.println(getCommandString(current_command));

      sendCC1101Command(current_command, i);
    }
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_pass)) {
      Serial.println(" connected.");
      for (int i = 0; i < REMOTE_COUNT; i++) {
        char subscription_channel[10];
        snprintf(subscription_channel, sizeof(subscription_channel), "Somfy-%d", i);

        char feedback_channel[20];
        snprintf(feedback_channel, sizeof(feedback_channel), "Somfy-%d/Feedback", i);

        client.subscribe(subscription_channel);
        client.subscribe(feedback_channel);
      }
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 3 seconds");
      delay(3000);
    }
  }
}

bool isValidCommand(Command command) {
  switch (command) {
    case Command::My:
    case Command::Up:
    case Command::Down:
    case Command::Prog:
      return true;
    default:
      return false;
  }
}

String getCommandString(Command command) {
  switch (command) {
    case Command::My:
      return "My";
    case Command::Up:
      return "Up";
    case Command::Down:
      return "Down";
    case Command::Prog:
      return "Prog";
    default:
      return "Unknown";
  }
}
