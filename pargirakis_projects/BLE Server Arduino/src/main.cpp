/*
    Video: https://www.youtube.com/watch?v=oCMOYS71NIU
    Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleNotify.cpp
    Ported to Arduino ESP32 by Evandro Copercini

   Create a BLE server that, once we receive a connection, will send periodic notifications.
   The service advertises itself as: 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
   Has a characteristic of: 6E400002-B5A3-F393-E0A9-E50E24DCCA9E - used for receiving data with "WRITE" 
   Has a characteristic of: 6E400003-B5A3-F393-E0A9-E50E24DCCA9E - used to send data with  "NOTIFY"

   The design of creating the BLE server is:
   1. Create a BLE Server
   2. Create a BLE Service
   3. Create a BLE Characteristic on the Service
   4. Create a BLE Descriptor on the characteristic
   5. Start the service.
   6. Start advertising.

   In this example rxValue is the data received (only accessible inside that function).
   And txValue is the data to be sent, in this example just a byte incremented every second. 
*/
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

BLECharacteristic *pCharacteristic;
bool deviceConnected = false;
float txValue = 0;
const int readPin = 32;         // Use GPIO number. See ESP32 board pinouts
const int numOfLeds = 3;
const int ledPin[numOfLeds] = {2, 4, 5}; // Could be different depending on the dev board. I used the DOIT ESP32 dev board.
int freq = 5000;
int ledCh[numOfLeds] = {0, 1, 2};
int resolution = 8;

//std::string rxValue; // Could also make this a global var to access it in loop()

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID "80865467-9c99-4cce-a94e-48058d175fed" // UART service UUID
#define CHARACTERISTIC_UUID_RX "011c9658-f282-4ddc-97a9-a1b1fb6c52b9"
#define CHARACTERISTIC_UUID_TX "88e28319-e781-4b31-b34f-5b9c65e4dd74"

class MyServerCallbacks : public BLEServerCallbacks
{
    void onConnect(BLEServer *pServer, esp_ble_gatts_cb_param_t *param)
    {
        esp_ble_conn_update_params_t conn_params = {0};
        memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
        conn_params.latency = 0;
        conn_params.max_int = 0x0c; // max_int = 0x0c*1.25ms = 15ms
        conn_params.min_int = 0x0c; // min_int = 0x0c*1.25ms = 15ms
        conn_params.timeout = 400;  // timeout = 200*10ms = 2000ms
        //start sent the update connection parameters to the peer device.
        esp_ble_gap_update_conn_params(&conn_params);

        // other
        deviceConnected = true;
    };

    void onDisconnect(BLEServer *pServer)
    {
        deviceConnected = false;
    }
};

class MyCallbacks : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic *pCharacteristic)
    {
        std::string rxValue = pCharacteristic->getValue();

        if (rxValue.length() > 0)
        {
            // Do stuff based on the command received from the app
            if (rxValue.size() == numOfLeds + 1)
            {
                Serial.print("RGB Read!");
                for (int i = 0; i < numOfLeds; i++) {
                    ledcWrite(ledCh[i], rxValue.at(i) * rxValue[numOfLeds] / 255);
                }
            }

            Serial.println("*********");
            Serial.print("Received Value: ");

            for (int i = 0; i < rxValue.length(); i++)
            {
                Serial.print((int) rxValue[i]);
                Serial.print(" ");
            }

            Serial.println();

            Serial.println();
            Serial.println("*********");
        }
    }
};

void setup()
{
    Serial.begin(115200);

    for (int i = 0; i < numOfLeds; i++) {
        ledcSetup(ledCh[i], freq, resolution);
        ledcAttachPin(ledPin[i], ledCh[i]);
    }

    // Create the BLE Device
    BLEDevice::init("BLE Test"); // Give it a name

    // Create the BLE Server
    BLEServer *pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    // Create the BLE Service
    BLEService *pService = pServer->createService(SERVICE_UUID);

    // Create a BLE Characteristic
    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_TX,
        BLECharacteristic::PROPERTY_NOTIFY);

    pCharacteristic->addDescriptor(new BLE2902());

    BLECharacteristic *pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_RX,
        BLECharacteristic::PROPERTY_WRITE_NR);

    pCharacteristic->setCallbacks(new MyCallbacks());

    // Start the service
    pService->start();

    // Start advertising
    BLEAdvertising* pAdvertising = pServer->getAdvertising();                   //advertiser
    BLEAdvertisementData adData;                       //advData
    adData.setFlags(ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT); //FLAGS
    adData.setCompleteServices(BLEUUID(SERVICE_UUID));
    pAdvertising->setAdvertisementData(adData);                                            //set
    pAdvertising->setMinInterval(0x20);
    pAdvertising->setMaxInterval(0x20);
    
    pAdvertising->start();
    Serial.println("Waiting a client connection to notify...");

}

void loop()
{
    if (deviceConnected)
    {
        // Fabricate some arbitrary junk for now...
        txValue = analogRead(readPin) / 3.456; // This could be an actual sensor reading!

        // Let's convert the value to a char array:
        char txString[8];                 // make sure this is big enuffz
        dtostrf(txValue, 1, 2, txString); // float_val, min_width, digits_after_decimal, char_buffer

        //    pCharacteristic->setValue(&txValue, 1); // To send the integer value
        //    pCharacteristic->setValue("Hello!"); // Sending a test message
        // pCharacteristic->setValue(txString);

        // pCharacteristic->notify(); // Send the value to the app!
        // Serial.print("*** Sent Value: ");
        // Serial.print(txString);
        // Serial.println(" ***");

        // You can add the rxValue checks down here instead
        // if you set "rxValue" as a global var at the top!
        // Note you will have to delete "std::string" declaration
        // of "rxValue" in the callback function.
        //    if (rxValue.find("A") != -1) {
        //      Serial.println("Turning ON!");
        //      digitalWrite(LED, HIGH);
        //    }
        //    else if (rxValue.find("B") != -1) {
        //      Serial.println("Turning OFF!");
        //      digitalWrite(LED, LOW);
        //    }
    }
    delay(1000);
}
