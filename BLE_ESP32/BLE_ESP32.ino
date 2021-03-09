#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <cstring>
#include <iomanip>
#include <chrono>
#include <sstream>
#include <ctime>
#include <iostream>
#include <sys/time.h>

#define LocationService BLEUUID((uint16_t)0x1819)     //Location and Navigation Service
#define TimeService BLEUUID((uint16_t)0x1805)         //Current Time Service
#define Location_UUID BLEUUID((uint16_t)0x2A67)       //Location and Speed Characteristic
#define Latitude_UUID BLEUUID((uint16_t)0x2AAE)       //Latitude Characteristic
#define Longitude_UUID BLEUUID((uint16_t)0x2AAF)      //Longitude Characteristic
#define Time_UUID BLEUUID((uint16_t)0x2A2B)           //Current Time Characteristic
//The remote service we wish to connect to: it depends on what you want use
#define serviceUUID BLEUUID((uint16_t)0x180D)         //Heart Rate Service 
//The characteristic of the remote service we are interested in
#define charUUID BLEUUID((uint16_t)0x2A39)            //Heart Rate Control Point

typedef struct infoTuple {
  std::string relTime;
  std::string latitude;
  std::string longitude;
  //std::string airData;          //In case you want to add the air data to the structure
  struct infoTuple *next;
} infoTuple;

//Server variables
std::time_t t = std::time(NULL);
std::tm *now = std::localtime(&t);
std::time_t timer;
struct timeval tv;
int deviceConnected = 0;          //Number of connected devices
uint8_t data[13] = {0};           //Byte array used to set the flags into "Location and Speed" Characteristic
uint8_t dataTime[10] = {0};       //Array for date and time values
infoTuple globalTuple;            //Global tuple used to avoid wrong matches between written values
infoTuple *infoTuples = NULL;     //Initialization of the head of list
int nodesCounter = 0;             //Node counter before warning of memory full
bool memoryFlag = false;          //Flag to reset the value of the characteristic to 1
bool clientOn = false;            //Set this variable to true if you want to enable the client as well

//Client variables
static boolean doConnect = false;
static boolean connected = false;
static boolean doScan = false;
static BLERemoteCharacteristic* pRemoteCharacteristic;
static BLEAdvertisedDevice* myDevice;

/*** SERVER FUNCTIONS ***/
void parser(std::string timeValue) {
  //Example time: 20-02-2021 14:55:22
  std::istringstream ss1(timeValue);
  ss1 >> std::get_time(now, "%d-%m-%Y %H:%M:%S");
}

void insertInfoTuple(infoTuple **head, infoTuple *newTuple) {
  if (head == NULL) {
    *head = newTuple;
    nodesCounter++;
    return;
  }
  newTuple->next = *head;
  *head = newTuple;
  nodesCounter++;
  return;
}

void deleteInfoTuple(infoTuple **head, std::string relTime) {
  if (*head == NULL) return;                //Check if the list is empty
  if ((*head)->relTime == relTime) {        //Check if it's the first element to be removed
    infoTuple *tmp = *head;
    *head = (*head)->next;
    free(tmp);
    nodesCounter--;
    return;
  }
  for (infoTuple *curr = *head; curr->next != NULL; curr = curr->next) {
    if (curr->next->relTime == relTime) {
      infoTuple *tmp = curr->next;
      curr->next = curr->next->next;        //Connect the current node to the next deleted one
      free(tmp);
      nodesCounter--;
      return;
    }
  }
}

//Function to clear global tuples
void resetGlobalTuple() {
  globalTuple.latitude = "";
  globalTuple.longitude = "";
  globalTuple.relTime = "";
  Serial.println("Global values reset successfully");
  return;
}

BLECharacteristic LocationCharacteristic(
  Location_UUID,
  BLECharacteristic::PROPERTY_READ |
  BLECharacteristic::PROPERTY_NOTIFY
);

BLECharacteristic* LatitudeCharacteristic;
BLECharacteristic* LongitudeCharacteristic;
BLECharacteristic* TimeCharacteristic;

class ServerConnectionCallback: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected++;
      BLEDevice::startAdvertising();
      Serial.print("Client connected! Number of connected clients: ");
      Serial.println(deviceConnected);
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected--;
      Serial.print("Client disconnected! Number of connected clients: ");
      Serial.println(deviceConnected);
      pServer->startAdvertising();    //Restart advertising
    }
};

void makeTime(BLECharacteristic* TimeCharacteristic) {
  std::time_t t = std::time(NULL);
  now = std::localtime(&t);
  dataTime[0] = (1900 + now->tm_year) & 0xff;
  dataTime[1] = ((1900 + now->tm_year) >> 8) & 0xff;
  dataTime[2] = now->tm_mon + 1;
  dataTime[3] = now->tm_mday;
  dataTime[4] = now->tm_hour;
  dataTime[5] = now->tm_min;
  dataTime[6] = now->tm_sec;
  dataTime[7] = now->tm_wday;
  dataTime[8] = (std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                 .count() % 1000) * 256 / 1000;
}

class TimeCharacteristicCallbacks : public BLECharacteristicCallbacks {
  public: TimeCharacteristicCallbacks() {}
    virtual void onRead(BLECharacteristic* TimeCharacteristic) override {
      makeTime((BLECharacteristic*)&TimeCharacteristic);
      TimeCharacteristic->setValue(dataTime, sizeof(dataTime));
    }
    virtual void onWrite(BLECharacteristic* TimeCharacteristic) override {
      std::string value = TimeCharacteristic->getValue();
      parser(value);
      timer = mktime(now);
      tv.tv_sec = timer;
      tv.tv_usec = 0;
      settimeofday(&tv, NULL);
      globalTuple.relTime = value;
      makeTime((BLECharacteristic*)&TimeCharacteristic);
      TimeCharacteristic->setValue(dataTime, sizeof(dataTime));
      Serial.println("Time Characteristic received!");
      //TimeCharacteristic->notify();           //Uncomment this line if you want to notify the value
    }
};

class LatitudeCharacteristicCallbacks: public BLECharacteristicCallbacks {
  public: LatitudeCharacteristicCallbacks() {}
    void onWrite(BLECharacteristic* LatitudeCharacteristic) override {
      std::string value = LatitudeCharacteristic->getValue();
      globalTuple.latitude = value;
      Serial.println("Latitude Characteristic received!");
      //LatitudeCharacteristic->notify();      //Uncomment this line if you want to notify the value
    }
};

class LongitudeCharacteristicCallbacks: public BLECharacteristicCallbacks {
  public: LongitudeCharacteristicCallbacks() {}
    void onWrite(BLECharacteristic* LongitudeCharacteristic) override {
      std::string value = LongitudeCharacteristic->getValue();
      globalTuple.longitude = value;
      Serial.println("Longitude Characteristic received!");
      //LongitudeCharacteristic->notify();      //Uncomment this line if you want to notify the value
    }
};

/*** CLIENT FUNCTIONS ***/
class ConnectionClientCallback : public BLEClientCallbacks {
    void onConnect(BLEClient* pClient) {
    }
    void onDisconnect(BLEClient* pClient) {
      connected = false;
      Serial.println("A client has disconnected: onDisconnect.");
    }
};

class CheckAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
      Serial.print("BLE Advertised Device found: ");
      Serial.println(advertisedDevice.toString().c_str());

      if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID)) {
        BLEDevice::getScan()->stop();
        myDevice = new BLEAdvertisedDevice(advertisedDevice);
        doConnect = true;
        doScan = true;
      }
    }
};

bool connectToServer() {
  Serial.print("Forming a connection to ");
  Serial.println(myDevice->getAddress().toString().c_str());

  BLEClient* pClient = BLEDevice::createClient();
  Serial.println(" - Created client");
  pClient->setClientCallbacks(new ConnectionClientCallback());

  //Connect to the remote BLE Server
  pClient->connect(myDevice);
  Serial.println(" - Connected to server");

  //Obtain a reference to the service we are after in the remote BLE server
  BLERemoteService *pRemoteService = pClient->getService(serviceUUID);
  if (pRemoteService == nullptr) {
    Serial.print("Failed to find service UUID: ");
    Serial.println(serviceUUID.toString().c_str());
    pClient->disconnect();
    return false;
  }
  Serial.println(" - Service found");

  //Obtain a reference to the characteristic in the service of the remote BLE server
  pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
  if (pRemoteCharacteristic == nullptr) {
    Serial.print("Failed to find characteristic UUID: ");
    Serial.println(charUUID.toString().c_str());
    pClient->disconnect();
    return false;
  }
  Serial.println(" - Characteristic found");
  connected = true;
}

void setup() {
  Serial.begin(115200);

  //Create the BLE Device
  BLEDevice::init("ESP32");

  //Create the BLE Server
  BLEServer* pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerConnectionCallback());

  //Create the BLE Location Service
  BLEService* locationService = pServer->createService(LocationService);
  locationService->addCharacteristic(&LocationCharacteristic);

  //Create the BLE Time Service
  BLEService* timeService = pServer->createService(TimeService);

  //Create the BLE Time Characteristic
  BLECharacteristic* TimeCharacteristic = timeService->createCharacteristic(
      Time_UUID,
      BLECharacteristic::PROPERTY_READ |
      BLECharacteristic::PROPERTY_WRITE |
      BLECharacteristic::PROPERTY_NOTIFY
                                          );
  TimeCharacteristic->setValue(data, 10);
  TimeCharacteristic->setCallbacks(new TimeCharacteristicCallbacks());

  //Create the BLE Latitude Characteristic
  BLECharacteristic* LatitudeCharacteristic = locationService->createCharacteristic(
        Latitude_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_WRITE |
        BLECharacteristic::PROPERTY_NOTIFY
      );
  LatitudeCharacteristic->setCallbacks(new LatitudeCharacteristicCallbacks());

  //Create the BLE Longitude Characteristic
  BLECharacteristic* LongitudeCharacteristic = locationService->createCharacteristic(
        Longitude_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_WRITE |
        BLECharacteristic::PROPERTY_NOTIFY
      );
  LongitudeCharacteristic->setCallbacks(new LongitudeCharacteristicCallbacks());

  //Set flag of "Location and Speed"
  data[2] = 1;
  LocationCharacteristic.setValue(data, sizeof(data));

  //Create a BLE Descriptor
  LocationCharacteristic.addDescriptor(new BLE2902());              //Add notify descriptor for Location Characteristic
  LatitudeCharacteristic->addDescriptor(new BLE2902());             //Add notify descriptor for Latitude Characteristic
  LongitudeCharacteristic->addDescriptor(new BLE2902());            //Add notify descriptor for Longitude Characteristic
  TimeCharacteristic->addDescriptor(new BLE2902());                 //Add notify descriptor for Time Characteristic

  //Start the service
  locationService->start();
  timeService->start();

  //Start advertising
  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(LocationService);
  pAdvertising->addServiceUUID(TimeService);
  pAdvertising->setMinInterval(250 / 0.625f);
  pAdvertising->setMaxInterval(250 / 0.625f);
  BLEDevice::startAdvertising();
  Serial.println("Waiting a client connection to notify...");

  //Specify that we want active scanning and start the scan to run for 5 seconds
  if (clientOn) {
    BLEScan* pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new CheckAdvertisedDeviceCallbacks());
    pBLEScan->setInterval(1349);
    pBLEScan->setWindow(449);
    pBLEScan->setActiveScan(true);
    pBLEScan->start(5, false);
  }
}

void loop() {
  //TODO get air pollution data from sensor
  /*** SERVER ***/
  if (deviceConnected) {
    if (globalTuple.latitude != "" && globalTuple.longitude != "" && globalTuple.relTime != ""
        && nodesCounter <= 256 /*&& the LoRaWAN module is connected*/) {
      //TODO send values to LoRaWAN
      resetGlobalTuple();
      /*
        Check if there are nodes in the list and delete the nodes that have been sent
        TODO send values to LoRaWAN
        Get the time of the tuple sent and use it for delete the node
        deleteInfoTuple(&infoTuples, time);
      */
    }
    else if (globalTuple.latitude != "" && globalTuple.longitude != "" && globalTuple.relTime != "" && nodesCounter <= 256) {
      infoTuple **head = &infoTuples;
      infoTuple *tmp = (infoTuple*) calloc(1, sizeof(infoTuple));
      tmp->latitude = globalTuple.latitude;
      tmp->longitude = globalTuple.longitude;
      tmp->relTime = globalTuple.relTime;
      insertInfoTuple(&infoTuples, tmp);
      resetGlobalTuple();
    }
    else if (nodesCounter <= 256 && memoryFlag == true) {
      LocationCharacteristic.setValue(data, sizeof(data));
      Serial.println("Location flag resetted.");
      LocationCharacteristic.notify();
      memoryFlag = false;
    }
    else if (nodesCounter >= 256) {
      LocationCharacteristic.setValue("ERROR: Memory Full");
      Serial.println("ERROR: Memory full (The list is full, sending data failed for 256 attempts).");
      Serial.println("Location Characteristic error value notified!");
      LocationCharacteristic.notify();
      memoryFlag = true;
    }
  }
  //To debug the insertion use the code below
  /*if (nodesCounter < 256) {
    /*infoTuple **head = &infoTuples;
    infoTuple *tmp = (infoTuple*) calloc(1, sizeof(infoTuple));
    insertInfoTuple(&infoTuples, tmp);
    Serial.println(nodesCounter);
    }*/

  /*** CLIENT ***/
  if (clientOn) {
    if (doConnect) {
      if (connectToServer()) {
        Serial.println("Connected to the BLE Server.");
      } else {
        Serial.println("Failed to connect to the server.");
      }
      doConnect = false;
    }

    if (connected) {
      if (nodesCounter == 256) {
        std::string memoryFullAdvertise = "ERROR: Memory Full";
        Serial.println("ERROR: Memory full (The list is full, sending data failed for 256 attempts).");
        pRemoteCharacteristic->writeValue(memoryFullAdvertise.c_str(), memoryFullAdvertise.length());
      }
    } else if (doScan) {
      BLEDevice::getScan()->start(0);
    }
  }
  delay(1000);
}
