#include "arduino_secrets.h"

#include <ArduinoMqttClient.h>
#include <MKRNB.h>

const char broker[] = SECRET_BROKER;

String imei;

String deviceTenant;
String deviceUsername;
String devicePassword;

const char* c8yDownstreamDeviceCredsTopic = "s/dcr";
const char* c8yUpstreamDeviceCredsTopic = "s/ucr";
const char* c8yUpstreamPublishTopic = "s/us";

int measurementMsgFreq = 60000;

bool receivedDeviceCreds = false;

unsigned long lastMillis = 0;

NB nbAccess;
GPRS gprs;
NBSSLClient sslClient;
MqttClient mqttClient(sslClient);
NBModem modem;


/*
 * Connects to network, registers and then creates device on Cumulocity
 */
void setup() {
  // Wait until Serial is connected. Comment out if desired
  Serial.begin(115200);
  while (!Serial);

  // Using IMEI for MQTT client id
  getImei();

  // Connect to network
  connectNB();

  // C8y setup
  Serial.println("Registering device");
  registerC8yDevice();
  Serial.println("Device registered\n");

  Serial.println("Creating device");
  createC8yDevice();
  Serial.println("Device created");
}

/*
 * Connects to LTE network (if not already connected)
 * Connects to MQTT broker (if not already connected)
 * Sends measurement every X seconds
 */
void loop() {
  // If disconnected from LTE network, reconnect
  if (nbAccess.status() != NB_READY || gprs.status() != GPRS_READY) {
    connectNB();
  }

  // If disconnected from broker, reconnect
  if (!mqttClient.connected()) {
    connectMQTT();
  }

  // Poll for new MQTT messages
  mqttClient.poll();

  // Publish measurement every X seconds
  if (millis() - lastMillis > measurementMsgFreq) {
    lastMillis = millis();

    publishMeasurementMessage();
  }
}

/* 
 * Register the device with Cumulocity by requesting the device creds
 */
void registerC8yDevice() {
  // Use IMEI as client id for MQTT
  mqttClient.setId(imei);
  mqttClient.setUsernamePassword(SECRET_USER, SECRET_PASSWORD);

  // Set the MQTT message callback
  mqttClient.onMessage(onMessageReceived);

  // Connect to device creds broker
  Serial.print("Attempting to connect to MQTT broker: ");
  Serial.print(broker);
  Serial.println(" ");

  while (!mqttClient.connect(broker, 8883)) {
    // failed, retry
    Serial.print(".");
    Serial.println(mqttClient.connectError());
    delay(5000);
  }
  Serial.println();

  Serial.println("You're connected to the MQTT broker");
  Serial.println();

  // Subscribe to topic to receive device creds
  mqttClient.subscribe(c8yDownstreamDeviceCredsTopic);

  // Wait to receive device creds
  int publishRegisterMsgFreq = 2000;
  unsigned long lastMillisRegister = 0;
  unsigned long timeOutMillis = millis() + 60000;
  
  while (!receivedDeviceCreds) {
    mqttClient.poll();
    
    if (millis() - lastMillisRegister > publishRegisterMsgFreq) {
      lastMillisRegister = millis();
      
      publishRegisterDeviceMessage(); // Send again
      
    } else if (millis() > timeOutMillis) { // Time out
      break;
    }
  }

  // Disconnect from broker
  mqttClient.stop();

  if (!receivedDeviceCreds) {
    Serial.println("Timed out waiting to receive device creds.");
    while (true) {
      // Do nothing
    }
  }
}

/* 
 * Create device in Cumulocity
 */
void createC8yDevice() {
  // Use IMEI as client id for MQTT
  mqttClient.setId(imei);
  Serial.print("Using username: ");
  Serial.println(deviceTenant + "/" + deviceUsername);
  Serial.print("and password: ");
  Serial.println(devicePassword);
  mqttClient.setUsernamePassword(deviceTenant + "/" + deviceUsername, devicePassword);

  // Set the MQTT message callback
  mqttClient.onMessage(onMessageReceived);

  // Connect to device creds broker
  Serial.print("Attempting to connect to MQTT broker: ");
  Serial.print(broker);
  Serial.println(" ");

  while (!mqttClient.connect(broker, 8883)) {
    // Failed, retry
    Serial.print(".");
    Serial.println(mqttClient.connectError());
    delay(5000);
  }
  Serial.println();

  Serial.println("You're connected to the MQTT broker");
  Serial.println();

  // Create device
  publishCreateDeviceMessage();
}

/* 
 * Gets the IMEI of the modem
 */
void getImei() {
  // Get IMEI
  if (!modem.begin()) {
    Serial.println("ERROR, no modem answer.");
  }
  
  imei = modem.getIMEI();
  
  if (imei != NULL) {
    Serial.println("Modem's IMEI: " + imei);
  } else {
    Serial.println("Error: Could not get IMEI");
  }
}

/*
 * Connect/reconnect to the LTE Network
 */
void connectNB() {
  Serial.println("\nConnecting to LTE Network");

  while ((nbAccess.begin() != NB_READY) ||
         (gprs.attachGPRS() != GPRS_READY)) {
    Serial.print(".");
    delay(1000);
  }

  Serial.println("Connected to LTE Network");
  Serial.println();
}

/*
 * Connect/reconnect to broker 
 * Refer to ArduinoMqttClient/src/MqttClient.h for error codes
 */
void connectMQTT() {
  Serial.print("Attempting to connect to MQTT broker: ");
  Serial.print(broker);
  Serial.println(" ");

  while (!mqttClient.connect(broker, 8883)) {
    // Failed, retry
    Serial.print(".");
    Serial.println(mqttClient.connectError());
    delay(5000);
  }
  Serial.println();

  Serial.println("You're connected to the MQTT broker");
  Serial.println();
  
}

/*
 * Pings until the device is accepted on Cumulocity
 */
void publishRegisterDeviceMessage() {
  Serial.println("Publishing register device message");

  mqttClient.beginMessage(c8yUpstreamDeviceCredsTopic);
  mqttClient.print("");
  mqttClient.endMessage();
}

/*
 * Creates the device on Cumulocity
 */
void publishCreateDeviceMessage() {
  Serial.println("Publishing create device message");

  String createDeviceMessage = "100,";
  createDeviceMessage += imei;
  createDeviceMessage += ",c8y_MQTTdevice";

  mqttClient.beginMessage(c8yUpstreamPublishTopic);
  mqttClient.print(createDeviceMessage);
  mqttClient.endMessage();
}

/*
 * Publishes measurement to Cumulocity
 */
void publishMeasurementMessage() {
  Serial.println("Publishing measurement message");

  String newMessage = getMeasurementMessage();

  mqttClient.beginMessage(c8yUpstreamPublishTopic);
  mqttClient.print(newMessage);
  mqttClient.endMessage();
}

/*
 * Creates the measurement message
 * Currently using random data, would add sensor code here
 */
String getMeasurementMessage() {
  long temp = random(10,30);

  String measurementMessage = "200,myCustomTemperatureMeasurement,celcius,";
  measurementMessage += temp;
  measurementMessage += ",C";

  Serial.println(measurementMessage);
  return measurementMessage;
}

/*
 * Handler for receiving messages from subscribed topics
 */
void onMessageReceived(int messageSize) {
  String topic = mqttClient.messageTopic();
  
  Serial.print("Received a message on topic: ");
  Serial.println(topic);
  
  // Store message
  char* message = (char*) malloc(sizeof(char) * (messageSize + 1));
  int i = 0;
  while (mqttClient.available()) {
    message[i] = (char) mqttClient.read();
    i++;
  }
  message[messageSize] = '\0';
  
  Serial.println(message);
  
  // Handle based on topic
  if (topic == c8yDownstreamDeviceCredsTopic) {
    handleDeviceCredsMessage(message);

  } else {
    Serial.print("Warning: Received message on an unhandled topic: ");
    Serial.println(topic);
  }
  
}

/*
 * Handler for received device creds message
 */
void handleDeviceCredsMessage(char* message) {
  // Check message id
  String messageId = getValueInCsv((char *) message, 0);
  if (messageId == "70") {
    Serial.println("Received device creds");

    // Stop requesting device creds
    receivedDeviceCreds = true;
    
    // Extract creds
    deviceTenant = getValueInCsv((char *) message, 1);
    deviceUsername = getValueInCsv((char *) message, 2);
    devicePassword = getValueInCsv((char *) message, 3);
    
  } else {
    Serial.print("Warning: Received unhandled message id on device creds topic: ");
    Serial.println(messageId);
  }
}

/*
 * Returns the element in the provided index of a comma-separated char array
 * If the index is out of bounds (i.e. beyond the number of commas) it will return NULL
 */
char* getValueInCsv(char* message, int index) {
  int found = 0;
  int n = -1;
  int m = 0;
  int messageLength = strlen(message);

  // Cycle over message, counting commas
  for (int i = 0; i <= messageLength && found <= index; i++) {
    if (message[i] == ',' || i == messageLength) {
      found++;
      m = n + 1;
      n = (i == messageLength) ? i+1 : i;
    }
  }
  
  if (found <= index) { // Less items than provided index
    return NULL;
  }
  
  int len = n - m;

  // Alloc
  char* subMessage = (char*) malloc(sizeof(char) * (len + 1));

  // Copy len number of chars starting from m-th index
  strncpy(subMessage, &message[m], len);
  subMessage[len] = '\0';

  return subMessage;
}
