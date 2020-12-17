#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#define customService BLEUUID((uint16_t)0x1700)    //0x1700 is the ID for the type of service
#define BUFFSIZE 56

typedef struct operation {
    int id;
    int timeLoop;               //timeout
    unsigned long lastTime = 0; //myTime-lastTime>=timeLoop(timeout) sends lastTime=myTime
    char coordinates[32];
    struct operation *next;
} operation;

BLECharacteristic *pCharacteristic;
bool deviceConnected = false;
char value[BUFFSIZE];           //Buffer that contains the commands
char oldValue[BUFFSIZE];
char ack[4] = "ACK";
char nack[5] = "NACK";
char separator[2] = ",";        //Macro for the delimiter
char **commandSeparated;        //Double pointer for separate commands
int commandSpace = 0;           //Count the elements number that can be inserted in the commandReceived
char *rcvString[BUFFSIZE];      //String received from the operation
char *strToChar;                //Conversion of string to char
operation *operations = NULL;
int id = 0;                     //ID assigned to the operation
unsigned long myTime;

void conversionStringToChar(std::string rcvString) {
  strToChar = new char[rcvString.length() + 1];
  strcpy(strToChar, rcvString.c_str());
}

void printTime2() {
  Serial.print("Time: ");
  myTime = millis();
  Serial.println(myTime);
}

/*
Function to divide the commands received from the client and insert each entry into an array
*/
void commandReceived(char buffer[]) {
  int i = 0;
  char *token;
  commandSeparated = (char **) calloc(1, sizeof(char *));
  commandSpace = 1;
  token = strtok(buffer, separator);
  while (token) {
    if (commandSpace <= i) {
      commandSeparated = (char **) realloc(commandSeparated, (i*2)*sizeof(char *));
      commandSpace = i*2;
    }
    commandSeparated[i] = (char *) malloc(BUFFSIZE);
    strncpy(commandSeparated[i], token, BUFFSIZE);
    token = strtok(NULL, separator);
    i++;
  }
  return;
}

void freeCommand(char **commandSeparated) {
  for (int i=0; i<commandSpace; i++) {
    if(commandSeparated[i] != 0) free(commandSeparated[i]);
  }
  free(commandSeparated);
  return;
}

void insertOperation(operation **head, operation *newDevice) {
  if (head == NULL){
    *head = newDevice;
    return;
  }
  newDevice->next = *head;
  *head = newDevice;
  return;
}

void deleteOperation(operation **head, int id) {
  if (*head == NULL) return;              //Check if the list is empty
  if ((*head)->id == id) {                //Check if it's the first element to be removed
    operation *tmp = *head;
    *head = (*head)->next;
    free(tmp);
    return;
  }

  for (operation *curr = *head; curr->next != NULL; curr = curr->next) {
    if (curr->next->id == id) {
      operation *tmp = curr->next;
      curr->next = curr->next->next;      //Connect the current node to the next deleted one
      free(tmp);
      return;
    }
  }
}

void parseRequest() {
  operation **head = &operations;
  if (strcmp(commandSeparated[0], "get") == 0) {
    buildAckGet();
    //Send values...
    return;
  }
  else if (strcmp(commandSeparated[0], "set") == 0) {
    operation *tmp = (operation *) calloc(1, sizeof(operation));
    tmp->id = ++id;      
    tmp->timeLoop = atoi(commandSeparated[2]);
    tmp->next = NULL;
    buildAckSet(tmp->id);
    insertOperation(&operations, tmp);
    return;
  }
  else if (strcmp(commandSeparated[0], "del") == 0) {
    for (operation *curr = *head; curr->next != NULL; curr = curr->next) {
      if (curr->id == atoi(commandSeparated[2])) {
        buildAckDel(curr->id);
        deleteOperation(&operations, atoi(commandSeparated[2]));
      }
    }
  }
  else {
    memset(value, 0, BUFFSIZE);
    strcpy(value, "Invalid command");
  }
  return;
}

/*******HEADER FUNCTIONS********/

void headerAck(bool good) {
  memset(value, 0, BUFFSIZE);
  if (good) {
    strcpy(value, ack);
  }
  else {
    strcpy(value, nack); 
  }
  strcat(value, separator);
  strcat(value, "SEQ:");
  strcat(value, commandSeparated[1]);      //SEQ
}

void buildAckGet() {
  headerAck(true);
}

void buildAckSet(int idOp) {
  char buf[8] = {0};
  sprintf(buf, "%ld", idOp);               //Convert ID to string
  headerAck(true);
  strcat(value, separator);
  strcat(value, "ID:");                            
  strcat(value, buf);                      //ID
}

void buildAckDel(int idOp) {
  char buf[8] = {0};
  sprintf(buf, "%ld", idOp);               //Convert ID to string
  headerAck(true);
  strcat(value, "ID:");                         
  strcat(value, buf);                      //ID
}

/*******END HEADER FUNCTIONS********/

BLECharacteristic customCharacteristic(
  BLEUUID((uint16_t)0x1A00),
  BLECharacteristic::PROPERTY_READ | 
  BLECharacteristic::PROPERTY_WRITE |
  BLECharacteristic::PROPERTY_NOTIFY
);

class ServerCallbacks: public BLEServerCallbacks{
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
  };

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
  };
};

class MyCharacteristicCallbacks: public BLECharacteristicCallbacks{
    void onWrite(BLECharacteristic *customCharacteristic) {
      std::string rcvString = customCharacteristic->getValue();
      conversionStringToChar(rcvString);
      if(strlen(strToChar)>0){
        Serial.println("Value Received from BLE: ");
        Serial.println(strToChar);
        commandReceived(strToChar);
        parseRequest();
        customCharacteristic->setValue((char*)&value);
        //freeCommand(commandSeparated);              //To be tested
      }
      else{
        Serial.println("Empty Value Received!");
      }
  }
};

void setup() {
  Serial.begin(115200);

  // Create the BLE Device
  BLEDevice::init("BLE-Server");

  // Create the BLE Server
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(customService);

  // Create a BLE Characteristic
  pService->addCharacteristic(&customCharacteristic); 
  customCharacteristic.addDescriptor(new BLE2902());                          //Needed with PROPERTY_NOTIFY

  BLEDescriptor ClientCharacteristicConfiguration(BLEUUID((uint16_t)0x2902)); //Add notify descriptor
  customCharacteristic.setCallbacks(new MyCharacteristicCallbacks());
  pServer->getAdvertising()->addServiceUUID(customService);

  // Add location Descriptor to the Characteristic
  BLEDescriptor LocationDescriptor(BLEUUID((uint16_t)0x1819));                //0x1819 is the ID of Device Location
  LocationDescriptor.setValue("Coordinates: 41 24.2028, 2 10.4418");          //Alternativa 41°24'12.2"N 2°10'26.5"E
  customCharacteristic.addDescriptor(&LocationDescriptor);

  // Add time Descriptor to the Characteristic
  BLEDescriptor TimeDescriptor(BLEUUID((uint16_t)0x1805));                    //0x1805 is the ID of CTS
  TimeDescriptor.setValue("Time: 12:00:00");                                  //Example time
  customCharacteristic.addDescriptor(&TimeDescriptor);

  // Configure Advertising with the Services to be advertised
  pServer->getAdvertising()->addServiceUUID(customService);

  // Start the service
  pService->start();

  // Start advertising
  pServer->getAdvertising()->start();

  Serial.println(value);
  customCharacteristic.setValue((char*)&value);
}

void loop() {
  if (deviceConnected) {
    if(strcmp(oldValue, value) != 0) {
      strcpy(oldValue, value);
      customCharacteristic.notify();
    }
    if(operations != NULL) {
      if (operations->next == NULL) {                       //Check if the list is with one element
        if (myTime-(operations->lastTime) >= operations->timeLoop) {
          operations->lastTime = myTime;
          customCharacteristic.notify();
        }
      }
      else {
        for (operation *curr = operations; curr->next != NULL; curr = curr->next) {
          if (myTime-(curr->lastTime) >= curr->timeLoop) {
            curr->lastTime = myTime;
            customCharacteristic.notify();                  //TODO override notify method to send desired data
          }
        }
      }
    }
  }
  printTime2();
  delay(500);
}
