#include <SPI.h>
#include <Ethernet.h>
#include <ArduinoJson.h>
#include <Servo.h>
#include <SoftwareSerial.h>
SoftwareSerial qrscanner(11, 12);

// Network settings
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 1, 105);    // Arduino IP
IPAddress server(192, 168, 1, 2);  // Server IP
int port = 80;                     // HTTP port

EthernetClient client;

// Structure to hold item information
struct Item {
  int product_id;
  String productName;
  int cell_num;
  int quantity;
  int index;
};
const int maxItems = 5;
Item items[maxItems];
int itemCount = 0;

const int irSensorPin = 3;
const int numServos = 5;
Servo servos[numServos];
int servoPins[numServos] = { 22, 23, 24, 25 };

void setup() {
  Serial.begin(9600);
  qrscanner.begin(9600);
  Ethernet.begin(mac, ip);

  for (int i = 0; i < numServos; i++) {
    servos[i].attach(servoPins[i]);
  }

  while (!Serial) { ; }  // Wait for serial connection
}

void loop() {
  String qrCode = "";
  while (true) {
    if (qrscanner.available())  // Check if there is Incoming Data in the Serial Buffer.
    {
      while (qrscanner.available())  // Keep reading Byte by Byte from the Buffer till the Buffer is empty
      {
        char input = qrscanner.read();  // Read 1 Byte of data and store it in a character variable
        qrCode += input;                // Append the byte to the QR code string
        delay(5);                       // A small delay
      }
      Serial.println("QR Code Data: " + qrCode);  // Print the QR code data
      break;                                      // Exit the loop when a QR code value is scanned
    }
  }

  // Attempt to connect to the server
  if (connectToServer()) {
    processServerResponse(qrCode);
  } else {
    Serial.println("Connection to server failed.");
  }
  delay(100);
}

bool connectToServer() {
  Serial.println("Connecting to server...");
  if (client.connect(server, port)) {
    Serial.println("Connected to server");
    return true;
  }
  return false;
}

void processServerResponse(String qrCode) {
  String encodedQrCode = urlEncode(qrCode);
  client.print("GET /vm-unif/main-content/search_transaction.php?qrcode=");
  client.print(encodedQrCode);
  client.println(" HTTP/1.1");

  client.println("Host: 192.168.1.3");
  client.println("Connection: close");
  client.println();

  String response;
  bool headersEnded = false;

  while (client.connected() || client.available()) {
    if (client.available()) {
      char c = client.read();
      if (!headersEnded) {
        if (c == '\n' && response.endsWith("\r\n\r\n")) {
          headersEnded = true;
          response = "";  // Clear any remaining header content
        } else {
          response += c;
        }
      } else {
        response += c;  // Accumulate JSON data
      }
    }
  }

  Serial.println("Raw Server Response: ");
  Serial.println(response);

  int jsonStart = response.indexOf('{');
  if (jsonStart != -1) {
    String jsonData = response.substring(jsonStart);
    parseJsonResponse(jsonData);
  } else {
    Serial.println("No JSON data found in server response.");
  }

  client.stop();  // Close the connection
}

void parseJsonResponse(String jsonData) {
  if (jsonData.startsWith("{")) {
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, jsonData);

    if (!error) {
      const char* status = doc["status"];
      if (strcmp(status, "success") == 0) {
        Serial.println("Transaction found!");
        itemCount = 0;  // Reset item count

        JsonArray itemArray = doc["items"];
        Serial.println("Items for dispensing:");

        for (JsonObject item : itemArray) {
          if (itemCount < maxItems) {
            const int productId = item["product_id"];
            const char* productName = item["product_name"];
            int cell_num = item["cell_num"];
            int quantity = item["quantity"];

            items[itemCount] = { productId, String(productName), cell_num, quantity, itemCount };
            itemCount++;
            
            Serial.print("ID: ");
            Serial.print(productId);
            Serial.print("Item: ");
            Serial.print(productName);
            Serial.print(" | Cell: ");
            Serial.print(cell_num);
            Serial.print(" | Quantity: ");
            Serial.print(quantity);
            Serial.println();
          }
        }

        Serial.println("Ready for dispensing.");
        dispenseItems();
      } else {
        Serial.println("Transaction not found or already processed.");
      }
    } else {
      Serial.print("Error parsing server response: ");
      Serial.println(error.c_str());
    }
  } else {
    Serial.println("Server response does not start with JSON object.");
  }
}

void dispenseItems() {
  int sensorValue = digitalRead(irSensorPin);

  for (int i = 0; i < itemCount; i++) {
    Serial.print("Dispensing on cell: ");
    Serial.println(items[i].cell_num);
    Serial.print("Item Name: ");
    Serial.println(items[i].productName);
    Serial.print("Quantity: ");
    Serial.println(items[i].quantity);

    int j = 0;
    int servoIndex = items[i].cell_num - 1;

    while (j < items[i].quantity) {
      sensorValue = digitalRead(irSensorPin);

      if (sensorValue == 1) {
        servos[servoIndex].write(0);  // Activate servo
      } else if (sensorValue == 0) {
        while (sensorValue == 0) {
          servos[servoIndex].write(90);  // Reset servo
          sensorValue = digitalRead(irSensorPin);
        }
        j++;
        updateStockOnServer(items[i].product_id, 1);
      }
      delay(200);  // Wait before dispensing next item
    }
    delay(1000);  // Delay before the next item
  }
}

String urlEncode(const String& str) {
  String encoded = "";
  for (size_t i = 0; i < str.length(); i++) {
    char c = str.charAt(i);
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += c;  // Safe characters
    } else {
      encoded += '%' + String(c, HEX);
    }
  }
  return encoded;
}

void updateStockOnServer(const int product_id, int quantity) {
  if (connectToServer()) {
    client.print("GET /vm-unif/main-content/update_stock.php?product_id=");
    client.print(product_id);
    client.print("&quantity=");
    client.println(quantity);
    client.println(" HTTP/1.1");
    client.println("Host: 192.168.1.3");
    client.println("Connection: close");
    client.println();

    // Read the server's response (optional, you can log it if needed)
    String response;
    while (client.connected() || client.available()) {
      if (client.available()) {
        response += (char)client.read();
      }
    }
    Serial.println("Stock update response: ");
    Serial.println(response);

    client.stop();  // Close the connection
  } else {
    Serial.println("Failed to connect to server for stock update.");
  }
}

