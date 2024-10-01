#include <Ethernet.h>
#include <SPI.h>
#include <ArduinoJson.h>
#include <Keypad.h>
#include <SoftwareSerial.h>

#include <Adafruit_GFX.h>   // Core graphics library
#include <MCUFRIEND_kbv.h>  // Hardware-specific library
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 480
MCUFRIEND_kbv tft;
SoftwareSerial printer(17, 18);

// Color definitions
#define BLACK 0x0000
#define WHITE 0xFFFF
#define RED 0xF800
#define GREEN 0x07E0
#define BLUE 0x001F
#define YELLOW 0xFFE0
#define LIGHT_BLUE 0xADD8E6

// Network settings
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 1, 105);    // Arduino IP
IPAddress server(192, 168, 1, 2);  // Server IP
int port = 80;                     // HTTP port

EthernetClient client;

const byte ROWS = 4;
const byte COLS = 4;

char keys[ROWS][COLS] = {
  { '1', '4', '7', '*' },
  { '2', '5', '8', '0' },
  { '3', '6', '9', '#' },
  { 'A', 'B', 'C', 'D' }
};
byte rowPins[ROWS] = { 22, 23, 24, 25 };
byte colPins[COLS] = { 26, 27, 28, 29 };
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// LiquidCrystal_I2C tft(0x27, 20, 4);
typedef String (*GetNameFunc)(void* item);
// to hold category data
struct Category {
  int category_id;
  String category_name;
};
Category categories[5];
int categoryCount = 0;

// to hold product data
struct Product {
  String product_name;
};
Product products[10];
int productCount = 0;

// to hold product size data
struct Size {
  int product_id;
  int size_id;
  String size_name;
};
Size sizes[4];
int sizeCount = 0;

struct Order {
  int product_id;
  String prod_name;
  int size;
  String prod_size;
  int quantity;
};
Order orders[6];
int orderCount = 0;

int product_quantity;

void setup(void) {
  Serial.begin(9600);
  printer.begin(9600);
  Ethernet.begin(mac, ip);
  tft.begin(tft.readID());
  tft.setRotation(0);
  tft.fillScreen(BLACK);
  tft.setTextColor(WHITE);
  tft.setTextSize(2);
  delay(1000);
}

void loop() {
  // Wait for input to start fetching products
  tft.fillScreen(BLACK);
  displayCenteredText("ARDUINO-BASED", 80, 3, WHITE);
  displayCenteredText("UNIFORM\nVENDING\nMACHINE\n", 110, 6, YELLOW);
  displayText(30, 420, "Press 'A' to start...", 2, WHITE);
  keyToStart('A');
  orderCount = 0;

  while (orderCount < 6) {
    int selectedCategoryId = categorySelection();
    Serial.print("Category ID: ");
    Serial.println(selectedCategoryId);
    int selectedProductIndex = productSelection(selectedCategoryId);
    Serial.print("selectedProductIndex: ");
    Serial.println(selectedProductIndex);
    int selectedSizeIndex = sizeSelection(selectedProductIndex);
    Serial.print("selectedSizeIndex: ");
    Serial.println(selectedSizeIndex);
    int selectedQuantity = quantitySelection(sizes[selectedSizeIndex].product_id);
    Serial.print("selectedQuantity: ");
    Serial.println(selectedQuantity);

    orders[orderCount].product_id = sizes[selectedSizeIndex].product_id;
    orders[orderCount].prod_name = products[selectedProductIndex].product_name;
    orders[orderCount].size = sizes[selectedSizeIndex].size_id;
    orders[orderCount].prod_size = sizes[selectedSizeIndex].size_name;
    orders[orderCount].quantity = selectedQuantity;

    Serial.print("PID: ");
    Serial.println(orders[orderCount].product_id);
    Serial.print("PName: ");
    Serial.println(orders[orderCount].prod_name);
    Serial.print("SID: ");
    Serial.println(orders[orderCount].size);
    Serial.print("SName: ");
    Serial.println(orders[orderCount].prod_size);
    Serial.print("Quan: ");
    Serial.println(orders[orderCount].quantity);

    tft.fillScreen(BLACK);
    displayText(30, 30, "Item " + String(orderCount + 1) + ":", 3, WHITE);
    displayText(30, 80, "Name: " + orders[orderCount].prod_name, 2, WHITE);
    displayText(30, 110, "Size: " + orders[orderCount].prod_size, 2, WHITE);
    displayText(30, 140, "Qty: " + String(orders[orderCount].quantity) + "pc(s).", 2, WHITE);
    orderCount++;
    delay(2000);

    displayText(30, 390, "Add another item?\n'A' = Yes | 'B' = No", 2, WHITE);


    char key = keypad.getKey();
    while (key != 'A' && key != 'B') {
      key = keypad.getKey();
      delay(100);
    }
    if (key == 'B') break;  // Exit loop if user does not want to add more
  }

  tft.fillScreen(BLACK);
  displayText(30, 30, "Saving order...", 2, YELLOW);
  sendOrderDetails(orderCount);  // Send order details to the server

  displayText(30, 60, "Done!", 3, GREEN);
  delay(1000);

  tft.fillScreen(BLACK);
  displayText(30, 30, "Take your receipt\nand bring it\nto the cashier.", 2, WHITE);
  delay(2000);
  tft.fillScreen(BLACK);
  displayText(30, 30, "Thank you!", 3, YELLOW);
  delay(2000);
}

void keyToStart(char expectedKey) {
  char key = 0;
  while (key != expectedKey) {
    key = keypad.getKey();
    delay(100);
  }
}

void displayText(int x, int y, String text, int size, uint16_t color) {
  tft.setTextColor(color);
  tft.setTextSize(size);

  int lineHeight = size * 15;  // Adjust line height based on the text size (8 is typical character height per size unit)
  int currentY = y;

  String currentLine = "";

  // Iterate through the text and handle newlines
  for (int i = 0; i < text.length(); i++) {
    char c = text.charAt(i);

    if (c == '\n') {
      // Print the current line and move to the next line
      tft.setCursor(x, currentY);
      tft.print(currentLine);
      currentY += lineHeight;
      currentLine = "";  // Clear the current line buffer
    } else {
      currentLine += c;  // Add the character to the current line
    }
  }

  // Print the last line if it exists
  if (currentLine.length() > 0) {
    tft.setCursor(x, currentY);
    tft.print(currentLine);
  }
}

void displayCenteredText(String text, int y, int textSize, uint16_t color) {
  tft.setTextSize(textSize);
  tft.setTextColor(color);

  int16_t x1, y1;
  uint16_t width, height;

  int lineHeight = textSize * 10;  // Approximate height of one line

  // Split and print each line based on the starting y position
  String currentLine = "";
  for (unsigned int i = 0; i < text.length(); i++) {
    if (text[i] == '\n' || i == text.length() - 1) {
      if (i == text.length() - 1) currentLine += text[i];  // Add last char

      // Get text bounds for the current line
      tft.getTextBounds(currentLine, 0, y, &x1, &y1, &width, &height);

      // Calculate x to center the text
      int16_t x = (tft.width() - width) / 2;

      // Set cursor and print the current line
      tft.setCursor(x, y);
      tft.print(currentLine);

      // Move the cursor down for the next line
      y += lineHeight;

      currentLine = "";  // Reset for the next line
    } else {
      currentLine += text[i];
    }
  }
}

void fetchData(String endpoint, int selectedId) {
  int retries = 3;  // Number of retries for connection
  bool connected = false;

  while (retries > 0 && !connected) {
    if (client.connect(server, port)) {
      connected = true;  // Set the flag if connected
      Serial.println("Connected to server");

      if (endpoint == "/vm-unif/lib/get_products.php") {
        endpoint += "?category_id=" + String(selectedId);
      } else if (endpoint == "/vm-unif/lib/get_sizes.php") {
        String encodedProductName = urlencode(products[selectedId].product_name);
        endpoint += "?product_name=" + encodedProductName;
      } else if (endpoint == "/vm-unif/lib/get_quantities.php") {
        endpoint += "?product_id=" + String(selectedId);
      }

      // Correct the GET request format
      client.print("GET ");
      client.print(endpoint);
      client.println(" HTTP/1.1");
      client.println("Host: 192.168.0.104");
      client.println("User-Agent: Arduino/1.0");
      client.println("Connection: close");
      client.println();
    } else {
      retries--;
      Serial.println("Connection failed, retrying...");
      // tft.fillScreen(BLACK);
      // tft.setCursor(20, 20);
      // tft.print("Connection failed");
      // tft.setCursor(20, 50);
      // tft.print("Retrying...");
      tft.fillScreen(BLACK);
      displayText(30, 30, "Connection failed.\nRetrying...", 2, RED);
      delay(1000);  // Wait before retrying
    }
  }

  if (!connected) {
    Serial.println("Failed to connect after retries");
    tft.fillScreen(BLACK);
    displayText(30, 30, "Failed to connect.\nPlease go to the\nTreasury or \nPurchasing office.", 2, RED);
    delay(2000);
    tft.fillScreen(BLACK);
    displayText(30, 30, "Sorry for\nthe inconvience.", 2, RED);
    delay(2000);
    asm volatile("  jmp 0");
  }

  // Wait for server response
  while (client.connected() && !client.available()) {
    delay(10);
  }

  // Read the server response
  String jsonResponse = "";
  bool isBody = false;

  while (client.available()) {
    String line = client.readStringUntil('\n');

    if (line == "\r") {
      isBody = true;
      continue;
    }

    if (isBody) {
      jsonResponse += line;
    }
  }

  client.stop();
  Serial.println("Disconnected from server");
  Serial.println("Raw JSON Response:");
  Serial.println(jsonResponse);

  parseJsonData(jsonResponse, endpoint);
}

void parseJsonData(String jsonResponse, String endpoint) {
  if (jsonResponse.length() == 0) {
    Serial.println("JSON response is empty.");
    return;
  }

  const size_t capacity = JSON_ARRAY_SIZE(24) + 24 * JSON_OBJECT_SIZE(3) + 24 * 50;
  DynamicJsonDocument doc(capacity);
  DeserializationError error = deserializeJson(doc, jsonResponse);

  if (error) {
    Serial.print("Failed to parse JSON: ");
    Serial.println(error.c_str());
    return;
  }

  if (endpoint.startsWith("/vm-unif/lib/get_categories.php")) {
    categoryCount = 0;
    JsonArray categoriesArray = doc.as<JsonArray>();

    for (JsonObject category : categoriesArray) {
      categories[categoryCount].category_id = category["category_id"];
      categories[categoryCount].category_name = String(category["category_name"].as<const char*>());
      categoryCount++;
    }
  } else if (endpoint.startsWith("/vm-unif/lib/get_products.php")) {
    productCount = 0;
    JsonArray productsArray = doc.as<JsonArray>();

    for (JsonObject product : productsArray) {
      products[productCount].product_name = String(product["product_name"].as<const char*>());
      productCount++;
    }
  } else if (endpoint.startsWith("/vm-unif/lib/get_sizes.php")) {
    sizeCount = 0;
    JsonArray sizesArray = doc.as<JsonArray>();

    for (JsonObject size : sizesArray) {
      sizes[sizeCount].product_id = size["product_id"];
      sizes[sizeCount].size_id = size["size_id"];
      sizes[sizeCount].size_name = String(size["size_name"].as<const char*>());
      sizeCount++;
    }
  } else if (endpoint.startsWith("/vm-unif/lib/get_quantities.php")) {
    product_quantity = doc[0]["product_quantity"];
  }
}

int categorySelection() {
  fetchData("/vm-unif/lib/get_categories.php", 0);
  return handleSelection("Categories", categories, categoryCount, sizeof(Category), getCategoryName, 1);
}

int productSelection(int categoryId) {
  fetchData("/vm-unif/lib/get_products.php", categoryId);
  return handleSelection("Products", products, productCount, sizeof(Product), getProductName, 0);
}

int sizeSelection(int productIndex) {
  fetchData("/vm-unif/lib/get_sizes.php", productIndex);
  return handleSelection("Sizes", sizes, sizeCount, sizeof(Size), getSizeName, 0);
}

int quantitySelection(int productId) {
  fetchData("/vm-unif/lib/get_quantities.php", productId);  // Fetch available quantity
  int availableQuantity = product_quantity;                 // Get available quantity
  return handleQuantitySelection(availableQuantity);        // Pass available quantity to handle function
}

// Function to display a list of items
void displayList(const char* title, void* items, int itemCount, int itemSize, GetNameFunc getName) {
  displayText(30, 30, title, 3, WHITE);  // Display the title

  for (int i = 0; i < itemCount; i++) {
    // Use itemSize to calculate the address of the current item
    void* currentItem = static_cast<void*>(static_cast<char*>(items) + (i * itemSize));
    String itemName = getName(currentItem);     // Retrieve the name
    displayListItem(i, itemName, 80 + i * 30);  // Display the item
  }
}

// Function to display a single list item
void displayListItem(int index, String itemName, int yPosition) {
  tft.setTextSize(2);
  tft.setCursor(30, yPosition);
  tft.print(index + 1);  // Print the index number
  tft.print(": ");
  tft.print(itemName);  // Print the item name
}

String getCategoryName(void* item) {
  return static_cast<Category*>(item)->category_name;
}

String getProductName(void* item) {
  return static_cast<Product*>(item)->product_name;
}

String getSizeName(void* item) {
  return static_cast<Size*>(item)->size_name;
}

// Generalized selection handler function
int handleSelection(const char* title, void* items, int itemCount, int itemSize, String (*getName)(void*), int displayOffset) {
  char key = 0;
  int selectedIndex = -1;
  tft.fillScreen(BLACK);
  displayList(title, items, itemCount, itemSize, getName);

  while (true) {
    key = keypad.getKey();

    // Check if the input is valid (between '1' and the number of items)
    if (key >= '1' && (key - '0') <= itemCount) {
      selectedIndex = key - '1';  // Convert to index
      tft.fillRect(30, 390, 300, 30, BLACK);
      displayText(30, 390, "Selected " + String(title) + ": " + String(selectedIndex + 1), 2, WHITE);
    } else if (key == '#') {
      if (selectedIndex != -1) {  // Only allow exiting if a valid selection is made
        tft.fillScreen(BLACK);
        displayText(30, 30, "You selected:\n" + String(selectedIndex + 1) + ": " + getName((char*)items + selectedIndex * itemSize), 2, WHITE);
        delay(1500);
        return selectedIndex + displayOffset;  // Return the selected index with offset (e.g., category_id)
      }
    }

    // Prompt the user to select and confirm with '#'
    displayText(30, 420, "Press '#' to enter", 2, WHITE);
    delay(100);
  }
}

int handleQuantitySelection(int availableQuantity) {
  int selectedQuantity = 0;
  char key = 0;

  // Determine the maximum quantity allowed (minimum between 2 and the available quantity)
  int maxQuantity = min(availableQuantity, 2);

  // Initial display
  tft.fillScreen(BLACK);
  displayText(30, 30, "Quantity: " + String(product_quantity), 2, WHITE);
  displayText(30, 60, "Max: " + String(maxQuantity), 2, WHITE);
  displayText(30, 390, "Selected quantity: " + String(selectedQuantity), 2, WHITE);
  displayText(30, 420, "Press '#' to enter", 2, WHITE);

  while (true) {
    key = keypad.getKey();

    // If '1' is pressed, set selectedQuantity to 1
    if (key == '1') {
      selectedQuantity = 1;
      tft.fillRect(30, 390, 300, 30, BLACK);
      displayText(30, 390, "Selected quantity: " + String(selectedQuantity), 2, WHITE);

      // If '2' is pressed, set selectedQuantity to 2 (if available quantity allows it)
    } else if (key == '2' && maxQuantity >= 2) {
      selectedQuantity = 2;
      tft.fillRect(30, 390, 300, 30, BLACK);
      displayText(30, 390, "Selected quantity: " + String(selectedQuantity), 2, WHITE);

      // If '#' is pressed and a valid quantity is selected, confirm the selection
    } else if (key == '#') {
      if (selectedQuantity > 0) {  // Ensure a valid quantity has been selected
        tft.fillScreen(BLACK);
        displayText(30, 30, "Confirmed quantity: " + String(selectedQuantity), 2, WHITE);
        delay(1500);
        return selectedQuantity;  // Return the selected quantity
      }
    }

    delay(100);  // Debounce delay
  }
}

void sendOrderDetails(int orderCount) {
  if (client.connect(server, port)) {
    Serial.println("Connected to server");

    // Create JSON string for order details
    String jsonOrderDetails = "{\"order_details\":[";
    for (int i = 0; i < orderCount; i++) {
      if (i > 0) jsonOrderDetails += ",";
      jsonOrderDetails += "{\"product_id\":" + String(orders[i].product_id) + ",\"quantity\":" + String(orders[i].quantity) + "}";
    }
    jsonOrderDetails += "]}";

    Serial.print("jsonOrderDetails: ");
    Serial.println(jsonOrderDetails);

    // Send the HTTP POST request
    client.println("POST /vm-unif/main-content/insert_order.php HTTP/1.1");
    client.println("Host: 192.168.1.22");
    client.println("Connection: close");
    client.println("Content-Type: application/json");
    client.print("Content-Length: ");
    client.println(jsonOrderDetails.length());
    client.println();
    client.println(jsonOrderDetails);

    // Wait for server response
    while (client.connected() && !client.available()) {
      delay(10);
    }

    // Read and print the response
    String response = "";
    while (client.available()) {
      response += client.readStringUntil('\n');
    }
    Serial.println("Received headers:");
    Serial.println(response);

    // Find the start of the JSON part
    int jsonIndex = response.indexOf("{");
    if (jsonIndex != -1) {
      String jsonResponse = response.substring(jsonIndex);  // Extract JSON part
      Serial.println("JSON Response:");
      Serial.println(jsonResponse);

      // Handle JSON parsing
      DynamicJsonDocument doc(2048);
      DeserializationError error = deserializeJson(doc, jsonResponse);

      if (error) {
        Serial.print("Failed to parse JSON: ");
        Serial.println(error.c_str());
        Serial.println("Received JSON: " + jsonResponse);  // Print received JSON for troubleshooting
        return;
      }

      if (doc["status"] == "success") {
        Serial.println("Order successfully placed!");
        int orderId = doc[0]["order_id"];
        printReceipt(orderId);
      } else {
        Serial.print("Error placing order: ");
        Serial.println(doc["message"].as<String>());
      }
    } else {
      Serial.println("JSON part not found in the response.");
    }
  } else {
    Serial.println("Connection failed");
  }
}

void printReceipt(int orderId) {
  // Initialize thermal printer and print the receipt
  Serial.println("Printing receipt...");
  

  // Print order details
  Serial.print("Order ID: ");
  Serial.println(orderId);
  for (int i = 0; i < orderCount; i++) {
    Serial.print(orders[i].prod_name);
    Serial.print(" (");
    Serial.print(orders[i].prod_size);
    Serial.print(") x ");
    Serial.println(orders[i].quantity);
  }

  // Example of how to print using your thermal printer (adjust commands as needed):
  printer.print("Order ID: ");
  printer.println(orderId);
  for (int i = 0; i < orderCount; i++) {
    printer.print(orders[i].prod_name);
    printer.print(" (");
    printer.print(orders[i].prod_size);
    printer.print(") x ");
    printer.println(orders[i].quantity);
  }
  printer.println("Thank you!");
  printer.println();
  printer.println();
  printer.println();
}

String urlencode(const String& str) {
  String encoded = "";
  for (unsigned int i = 0; i < str.length(); i++) {
    char c = str.charAt(i);
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += c;  // Add unencoded characters
    } else {
      encoded += String("%") + String(c, HEX);  // Encode others
    }
  }
  return encoded;
}
