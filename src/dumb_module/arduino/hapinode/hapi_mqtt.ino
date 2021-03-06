#include <Arduino.h>

/*
#*********************************************************************
#Copyright 2016 Maya Culpa, LLC
#
#This program is free software: you can redistribute it and/or modify
#it under the terms of the GNU General Public License as published by
#the Free Software Foundation, either version 3 of the License, or
#(at your option) any later version.
#
#This program is distributed in the hope that it will be useful,
#but WITHOUT ANY WARRANTY; without even the implied warranty of
#MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#GNU General Public License for more details.
#
#You should have received a copy of the GNU General Public License
#along with this program.  If not, see <http://www.gnu.org/licenses/>.
#*********************************************************************
*/

void (*resetFunc)(void) = 0; //declare reset function @ address

boolean sendMQTTStatus(void) {
  StaticJsonBuffer<128> hn_topic_status;                   // Status data for this HN
  JsonObject& status_message = hn_topic_status.createObject();

  // Publish current status
  // Set the time
  currentTime = now();
  // Identify HAPInode
  status_message["Node"] = HN_Id;
  // Returns the current status of the HN itself
  // Includes firmware version, MAC address, IP Address, Free RAM and Idle Mode
  status_message["FW"] = HAPI_FW_VERSION;
  status_message["time"] = currentTime;
  status_message["DIO"] = String(NUM_DIGITAL);
  status_message["AIO"] = String(NUM_ANALOG);
  status_message["Free SRAM"] = String(freeRam()) + "k";
  status_message["Idle"] = idle_mode;

  status_message.printTo(MQTTOutput, 128);          // MQTT JSON string is max 96 bytes
  strcpy(mqtt_topic, mqtt_topic_status);            // Generic status response topic
  strcat(mqtt_topic, hostString);                   // Add the NodeId

  Serial.print(mqtt_topic);
  Serial.print(F(" : "));
  Serial.println(MQTTOutput);

  // PUBLISH to the MQTT Broker
  if (MQTTClient.publish(mqtt_topic, MQTTOutput))
    return true;

  // If the message failed to send, try again, as the connection may have broken.
  Serial.println(F("Status Message failed to publish. Reconnecting to MQTT Broker and trying again .. "));
  if (!MQTTClient.connect(clientID, MQTT_broker_username, MQTT_broker_password)) {
    Serial.println(F("Connection to MQTT Broker failed..."));
    return false;
  }
  Serial.println(F("reconnected to MQTT Broker!"));
  delay(100); // This delay ensures that client.publish doesn't clash with the client.connect call
  if (MQTTClient.publish(mqtt_topic_status, MQTTOutput)) {
    Serial.println(F("Status Message published after one retry."));
    return true;
  }
  else {
    Serial.println(F("Status Message failed to publish after one retry."));
    return false;
  }
}

void sendAllMQTTAssets(void) {
  //Process digital pins
  for (int i = 0; i < NUM_DIGITAL; i++) {
    switch (pin_configurations[i].mode) {
    case DIGITAL_INPUT_PIN:
    case DIGITAL_INPUT_PULLUP_PIN:
    case DIGITAL_OUTPUT_PIN:
    case ANALOG_OUTPUT_PIN: // ^^^ does not jive with NUM_DIGITAL
      while (!sendMQTTAsset(SENSORID_DIO, i))  // Until it is sent
        ;
      break;
    default:
      break;
    }
  }
  //Process analog pins
  for (int i = 0; i < NUM_ANALOG; i++)
    while (!sendMQTTAsset(SENSORID_AIO, i+NUM_DIGITAL))  // Until it is sent
      ;
  // Process Custom Functions
  for (int i = 0; i < ARRAY_LENGTH(s_functions); i++)
    while (!sendMQTTAsset(SENSORID_FN, i))  // Until it is sent
      ;
  // Process Custom Functions
  for (int i = 0; i < ARRAY_LENGTH(c_functions); i++)
    while (!sendMQTTAsset(CONTROLID_FN, i))  // Until it is sent
      ;
  // Process Custom Functions
  for (int i = 0; i < ARRAY_LENGTH(c_functions); i++)
    while (!sendMQTTAsset(CONTROLDATA1_FN, i))  // Until it is sent
      ;
  // Process Custom Functions
  for (int i = 0; i < ARRAY_LENGTH(c_functions); i++)
    while (!sendMQTTAsset(CONTROLDATA2_FN, i))  // Until it is sent
      ;
}

boolean sendMQTTAsset(int AssetIdx, int i) {
  createAssetJSON(AssetIdx, i);                // (Store result in MQTTOutput)
  strcpy(mqtt_topic, mqtt_topic_asset);             // Generic asset response topic
  strcat(mqtt_topic, hostString);                   // Add the NodeId
  strcat(mqtt_topic, "/");                           // /
  strcat(mqtt_topic, s_functions[i].fName); // sensor name
  publishJSON(mqtt_topic);                          // Publish it

  Serial.print(mqtt_topic);
  Serial.print(F(" : "));
  Serial.println(MQTTOutput);
}

boolean sendMQTTException(int AssetIdx, int i) {
  createAssetJSON(AssetIdx, i);
  publishJSON(mqtt_topic_exception);
}

boolean createAssetJSON(int AssetIdx, int i) {
  //For custom functions
  FuncDef f;
  CFuncDef c;
  ControlData d;
  int pinValue;
  float funcVal = (-9.99);
  StaticJsonBuffer<256> hn_asset;                   // Asset data for this HN
  JsonObject& asset_message = hn_asset.createObject();

  // Set the NodeId
  asset_message["Node"] = HN_Id;

  // Assembles a message with values from pins and custom functions
  // Returns a JSON string
  currentTime = now();
  asset_message["t"] = currentTime;               //  into the JSON message

  switch (AssetIdx) {
  case SENSORID_DIO:
    asset_message["Asset"] = F("DIO");            // Asset ID
    asset_message["ctxt"] = F("PIN");             // Context
    asset_message["unit"] = F("");                // Units of measurement
    pinValue = digitalRead(i);
    asset_message["data"] = pinValue;           // Data
    break;
  case SENSORID_AIO:
    asset_message["Asset"] = F("AIO");
    asset_message["ctxt"] = F("PIN");
    asset_message["unit"] = F("");
    pinValue = analogRead(i);
    asset_message["data"] = pinValue;
    break;
  case SENSORID_FN:
    f = s_functions[i];
    asset_message["Asset"] = (String)f.fName;
    asset_message["ctxt"] = (String)f.fType;
    asset_message["unit"] = (String)f.fUnit;
    funcVal = f.fPtr(i);
    asset_message["data"] = funcVal;     // Two decimal points
    break;
  case CONTROLID_FN:
    c = c_functions[i];
    asset_message["Asset"] = (String)c.fName;
    asset_message["ctxt"] = (String)c.fType;
    asset_message["unit"] = (String)c.fUnit;
    funcVal = c.iPtr(i);
    asset_message["data"] = funcVal;     // Two decimal points
    break;
  case CONTROLDATA1_FN:
    d = c_data[i];
    asset_message["Asset"] = (String)d.hc_name;
    asset_message["pol"] = (boolean)d.hc_polarity;
    asset_message["stt"] = (unsigned long)d.hc_start;
    asset_message["end"] = (unsigned long)d.hc_end;
    asset_message["rpt"] = (unsigned long)d.hc_repeat;
    break;
  case CONTROLDATA2_FN:
    d = c_data[i];
    asset_message["Asset"] = (String)d.hc_name;
    asset_message["von"] = (float)d.hcs_onValue;
    asset_message["voff"] = (float)d.hcs_offValue;
    break;

  default:
    break;
  }
  asset_message.printTo(MQTTOutput, 128);          // MQTT JSON string is max 96 bytes
  Serial.println(MQTTOutput);
}

boolean publishJSON(const char *topic) {
  // PUBLISH to the MQTT Broker
  if (MQTTClient.publish(topic, MQTTOutput))
    return true;

  // If the message failed to send, try again, as the connection may have broken.
  Serial.println(F("Send Message failed. Reconnecting to MQTT Broker and trying again .. "));
  if (!MQTTClient.connect(clientID, MQTT_broker_username, MQTT_broker_password)) {
    Serial.println(F("Connection to MQTT Broker failed..."));
    return false;
  }
  Serial.println(F("reconnected to MQTT Broker!"));
  delay(100); // This delay ensures that client.publish doesn't clash with the client.connect call
  if (MQTTClient.publish(topic, MQTTOutput)) {
    //^^^ why no Serial.println() here?
    return true;
  }
  else {
    Serial.println(F("Send Message failed after one retry."));
    return false;
  }
}

void MQTTcallback(char *topic, byte *payload, unsigned int length) {
  int i;
  const char *node = "*";     // NodeId for target HAPInode, preset for anyone
  const char *command = " ";  // Command to execute
  char *hn_topic;             // Variable to hold all node topics
  int AssetIdx;               // Target Sensor Index
  int data;                   // Data for output

  hn_topic = &MQTTOutput[0];
  StaticJsonBuffer<200> hn_topic_command;            // Parsing buffer

  Serial.println(topic);
  // Copy topic to char *buffer
  for (i = 0; i < length; i++) {
    MQTTInput[i] = (char)payload[i];
    Serial.print(MQTTInput[i]);
  }
  MQTTInput[i] = '\0';
  Serial.println();

  //Parse the topic data
  JsonObject& command_topic = hn_topic_command.parseObject(MQTTInput);
  if (!command_topic.success())
    return;

  Serial.println(F("Parsing .. "));
  for (JsonObject::iterator it = command_topic.begin(); it != command_topic.end(); ++it) { //^^^ why ++it instead if it++?
    Serial.print(it->key);
    Serial.print(F(":"));
    Serial.println(it->value.as<char*>());
  }

  Serial.print(F("Node - "));
  Serial.println(node);
  // Check correct node ID
  if (command_topic.containsKey("Node")) { // NodeId is required for all messages, even if it is "*"
    node = command_topic["Node"];
  }

  // Check for COMMAND/ topic based commands
  // =======================================
  if (strcmp(node, hostString) != 0 && strcmp(node, "*") != 0)
    return;

  // Handle wildcard
  if (strcmp(topic, mqtt_topic_command) == 0) { //^^^ Yikes! mqtt_topic_command is global
    if (!command_topic.containsKey("Cmnd")) // Cmnd is required
      return;
    command = command_topic["Cmnd"];
    // Commands that do not require an Asset ID
    // ----------------------------------------
    if (strcmp(command, "assets") == 0) {
      sendAllMQTTAssets();
      return;
    }
    if (strcmp(command, "status") == 0) {
      sendMQTTStatus();
      return;
    }

    // Commands that do require an Asset ID
    // ------------------------------------
    if (!command_topic.containsKey(F("Asset"))) // AssetID is required
      return;

    Serial.println(F("Processing Asset"));
    // Digital IO
    if (strcmp(command_topic["Asset"], "DIO") == 0) { // Digital IO
      if (!command_topic.containsKey("pin")) // pin - required
        return;
      i = command_topic["pin"];

      if (strcmp(command, "din") == 0) {
        AssetIdx = SENSORID_DIO;
        sendMQTTAsset(AssetIdx, i);         // Publish digital data
        return;
      }
      if (strcmp(command, "dout") == 0) {
        if (!command_topic.containsKey("data")) // Data - required
          return;
        data = command_topic["data"];
        digitalWrite(i, data);               // Set the digital pin
        return;
      }
    }
    Serial.println(F(" .. not DIO"));

    // Analog IO
    if (strcmp(command_topic["Asset"], "AIO") == 0) { // Analog IO
      if (!command_topic.containsKey("pin")) // pin - required
        return;
      i = command_topic["pin"];
      if (strcmp(command, "ain") == 0) {
        AssetIdx = SENSORID_AIO;
        sendMQTTAsset(AssetIdx, i);         // Publish analog data
        return;
      }
      if (strcmp(command, "aout") == 0) {
        if (!command_topic.containsKey("data")) // Data - required
          return;
        data = command_topic["data"];
#ifndef HN_ESP32
        analogWrite(i, data);               // Set the analog pin
#endif
        return;
      }
    }
    Serial.println(F(" .. not AIO"));

    // Function IO
    AssetIdx = SENSORID_FN;                    // Asset Function IO
    for (i = 0; i < ARRAY_LENGTH(s_functions); i++) {    // Scan for a match on the sensor name
      if (strcmp(command_topic["Asset"], s_functions[i].fName) == 0) { // Asset match?
        // Match for Sensor name
        sendMQTTAsset(AssetIdx, i); // Publish sensor or control function data
        return;
      }
    }
    Serial.println(F(" .. not Sensor Read"));
    // Did not find a sensor, so try controls
    AssetIdx = CONTROLID_FN;                 // Control Function IO
    for (i = 0; i < ARRAY_LENGTH(c_functions); i++) { // Scan for a match on the control name
      if (strcmp(command_topic["Asset"], c_functions[i].fName) == 0) {  // Asset match?
        break; // Match for control name
      }
    }
    if (i < ARRAY_LENGTH(c_functions)) {
      if (strcmp(command, "fnin") == 0) {
        sendMQTTAsset(AssetIdx, i);       // Publish sensor or control function data
        return;
      }
      if (strcmp(command, "fnout") != 0) // Found a valid control name but no valid command or data
        return;

      // Function out only works for controls
      // Control
      if (command_topic.containsKey("pol")) {  // Polarity ( boolean)
        c_data[i].hc_polarity = command_topic["pol"];
      }
      if (command_topic.containsKey("stt")) {  // Start time (unix secs)
        Serial.println(F("writing stt"));
        c_data[i].hc_start = command_topic["stt"];
      }
      if (command_topic.containsKey("end")) {  // End time (unix secs)
        c_data[i].hc_end = command_topic["end"];
      }
      if (command_topic.containsKey("rpt")) {  // Repeat time (s)
        c_data[i].hc_repeat = command_topic["rpt"];
      }
      // Associated sensor
      if (command_topic.containsKey("von")) {  // Value to turn on
        c_data[i].hcs_onValue = command_topic["von"];
      }
      if (command_topic.containsKey("voff")) {  // Value to turn off
        c_data[i].hcs_offValue = command_topic["voff"];
      }
      return;
    }
    Serial.println(F(" .. not Control I/O"));
  } // End (strcmp COMMAND/ topic

  Serial.println(F(" .. not COMMAND/"));

  // Check for CONFIG/ only topic values
  // ===================================
  if (strcmp(topic, mqtt_topic_config) == 0) {
    if (command_topic.containsKey("timeZone")) {  // Time Zone ?
      timeZone = command_topic["timeZone"];
    }
    // Add extra CONFIG values here
    // ----------------------------
    else
      return;
  }
  Serial.println(F(" .. not CONFIG/"));

  // STATUS topics
  // =============
  Serial.print(F("Checking .. "));
  Serial.println(topic);

  strcpy(hn_topic, mqtt_topic_array[STATUSSTART]);     // Status query, any NodeId
  if (strcmp(topic, hn_topic) == 0) {
    sendMQTTStatus();
    return;
  }
  Serial.print(F(" .. not "));
  Serial.println(hn_topic);

  // ASSET topics
  // ============
  // Handle wildcards
  Serial.println(mqtt_topic_array[ASSETSTART]);   // Assets start
  for (int i = ASSETSTART; i <= ASSET_END; i++) { // Wildcard topics
    strcpy(hn_topic, mqtt_topic_array[i]);         // Asset query, any NodeId
    if (strcmp(topic, hn_topic) == 0) {
      sendAllMQTTAssets();
      return;
    }
    Serial.print(F(" .. not "));
    Serial.println(hn_topic);
  }

  // Handle sensors
  AssetIdx = SENSORID_FN;                    // Sensor Function IO
  for (i = 0; i < ARRAY_LENGTH(s_functions); i++) {    // Scan for a match on the sensor name
    strcpy(hn_topic, mqtt_topic_array[ASSETSTART+1]);     // Set base topic for a specific asset query
    strcat(hn_topic, hostString);              // NodeId next
    strcat(hn_topic, "/");                     //  .. MQTT separator
    strcat(hn_topic, s_functions[i].fName); //  .. and the sensor name
    if (strcmp(topic, hn_topic) == 0) {         // Asset match?
      // Match for Sensor name
      sendMQTTAsset(AssetIdx, i); // Publish sensor or control function data
      return;
    }
  }
  Serial.print(F(" .. not "));
  Serial.println(hn_topic);
  // Handle Controls
  AssetIdx = CONTROLID_FN;                   // Control Function IO
  for (i = 0; i < ARRAY_LENGTH(c_functions); i++) {   // Scan for a match on the control name
    strcpy(hn_topic, mqtt_topic_array[1]);     // Set base topic for an asset query
    strcat(hn_topic, hostString);              // NodeId next
    strcat(hn_topic, "/");                     //  .. MQTT separator
    strcat(hn_topic, c_functions[i].fName); //  .. and the control name
    if (strcmp(topic, hn_topic) == 0) {         // Asset match?
      // Match for Sensor name
      sendMQTTAsset(AssetIdx, i);         // Publish sensor or control function data
      return;
    }
  }
  Serial.print(F(" .. not "));
  Serial.println(hn_topic);

  // CONFIG topic
  // ============
  // Wildcards are not allowed in CONFIG
  // It must have a valid NodeId, Asset and data to work
  for (i = 0; i < ARRAY_LENGTH(c_functions); i++) {     // Scan for a match on the control name
    strcpy(hn_topic, mqtt_topic_array[CONFIGSTART]);       // Set base topic for a specific asset query
    strcat(hn_topic, hostString);              // NodeId next
    strcat(hn_topic, "/");                     //  .. MQTT separator
    strcat(hn_topic, c_functions[i].fName); //  .. and the sensor name
    if (strcmp(topic, hn_topic) == 0) {       // Asset match?
      break; // Match for Sensor name
    }
  }
  if (i < ARRAY_LENGTH(c_functions)) {
    // Match for Sensor name
    // Control
    if (command_topic.containsKey("pol")) {  // Polarity ( boolean)
      c_data[i].hc_polarity = command_topic["pol"];
    }
    if (command_topic.containsKey("stt")) {  // Start time (unix secs)
      c_data[i].hc_start = command_topic["stt"];
    }
    if (command_topic.containsKey("end")) {  // End time (unix secs)
      c_data[i].hc_end = command_topic["end"];
    }
    if (command_topic.containsKey("rpt")) {  // Repeat time (s)
      c_data[i].hc_repeat = command_topic["rpt"];
    }
    // Associated sensor
    if (command_topic.containsKey("von")) {  // Value to turn on
      c_data[i].hcs_onValue = command_topic["von"];
    }
    if (command_topic.containsKey("voff")) {  // Value to turn off
      c_data[i].hcs_offValue = command_topic["voff"];
    }
    return;
  }
  Serial.print(F(" .. not "));
  Serial.println(hn_topic);

  // Other topics go here
  // ====================
}
