#include <SPI.h>
#include <Wire.h>
#include <Adafruit_SH110X.h>
#include <GEM_adafruit_gfx.h>
#include <KeyDetector.h>
#include "RF24.h"
#include "esp_bt.h"
#include "esp_wifi.h"
#include "WiFi.h"

// Define signal identifiers for three outputs of encoder (channel A, channel B and a push-button)
#define KEY_UP 1
#define KEY_DOWN 2
#define KEY_OK 3

//Navigation buttons inputs
const byte downPin = 39;
//const byte leftPin = 3;
//const byte rightPin = 4;
const byte upPin = 34;
//const byte cancelPin = 6;
const byte okPin = 35;

byte chanB = HIGH; // Variable to store Channel B readings

Key keys[] = {{KEY_UP, upPin}, {KEY_OK, okPin}};

// Create KeyDetector object
// KeyDetector myKeyDetector(keys, sizeof(keys)/sizeof(Key));
// To account for switch bounce effect of the buttons (if occur) you may want to specify debounceDelay
// as the third argument to KeyDetector constructor.
// Make sure to adjust debounce delay to better fit your rotary encoder.
// Also it is possible to enable pull-up mode when buttons wired with pull-up resistors (as in this case).
// Analog threshold is not necessary for this example and is set to default value 16.
KeyDetector myKeyDetector(keys, sizeof(keys)/sizeof(Key), /* debounceDelay= */ 3, /* analogThreshold= */ 16, /* pullup= */ true);

bool secondaryPressed = false;  // If encoder rotated while key was being pressed; used to prevent unwanted triggers
bool cancelPressed = false;  // Flag indicating that Cancel action was triggered, used to prevent it from triggering multiple times
const int keyPressDelay = 1000; // How long to hold key in pressed state to trigger Cancel action, ms
long keyPressTime = 0; // Variable to hold time of the key press event
long now; // Variable to hold current time taken with millis() function at the beginning of loop()

#define i2c_Address 0x3c //initialize with the I2C screen addr 0x3C

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET -1   //   QT-PY / XIAO
Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Create variables that will be editable through the menu and assign them initial values
int number = -512;
bool enablePrint = true;

// Create variable that will be editable through option select and create associated option select.
// This variable will be passed to menu.invertKeysDuringEdit(), and naturally can be presented as a bool,
// but is declared as a byte type to be used in an option select rather than checkbox (for demonstration purposes)
byte invert = 1;
SelectOptionByte selectInvertOptions[] = {{"Invert", 1}, {"Normal", 0}};
GEMSelect selectInvert(sizeof(selectInvertOptions)/sizeof(SelectOptionByte), selectInvertOptions);

// Create menu item for option select with applyInvert() callback function
void applyInvert(); // Forward declaration
GEMItem menuItemInvert("Chars order:", invert, selectInvert, applyInvert);

// Create menu button that will trigger printData() function. It will print value of our number variable
// to Serial monitor if enablePrint is true. We will write (define) this function later. However, we should
// forward-declare it in order to pass to GEMItem constructor
void printData(); // Forward declaration
GEMItem menuItemButton("Print", printData);

void jammerBluetooth(); // Forward declaration
GEMItem menuItemJammerBluetooth("Jammer", jammerBluetooth);

void jammerWifi(); // Forward declaration
GEMItem menuItemJammerWifi("Jammer", jammerWifi);

void scanWifi(); // Forward declaration
GEMItem menuItemScanWifi("Scanner", scanWifi);

// Create menu page object of class GEMPage. Menu page holds menu items (GEMItem) and represents menu level.
// Menu can have multiple menu pages (linked to each other) with multiple menu items each
GEMPage menuPageMain("Main Menu"); // Main page
GEMPage menuPageBluetooth("Bluetooth"); // Bluetooth submenu
GEMPage menuPageWifi("Wifi"); // Wifi submenu
GEMPage menuPageSettings("Settings"); // Settings submenu

// Create menu item linked to Settings menu page
GEMItem menuItemMainBluetooth("Bluetooth", menuPageBluetooth);
GEMItem menuItemMainWifi("Wifi", menuPageWifi);
GEMItem menuItemMainSettings("Settings", menuPageSettings);

// Create menu object of class GEM_adafruit_gfx. Supply its constructor with reference to display object we created earlier
GEM_adafruit_gfx menu(display, GEM_POINTER_ROW, GEM_ITEMS_COUNT_AUTO);
// Which is equivalent to the following call (you can adjust parameters to better fit your screen if necessary):
// GEM_adafruit_gfx menu(display, /* menuPointerType= */ GEM_POINTER_ROW, /* menuItemsPerScreen= */ GEM_ITEMS_COUNT_AUTO, /* menuItemHeight= */ 10, /* menuPageScreenTopOffset= */ 10, /* menuValuesLeftOffset= */ 86);

SPIClass *hp = nullptr;

RF24 radio(16, 15, 16000000);   //HSPI CAN SET SPI SPEED TO 16000000 BY DEFAULT ITS 10000000
int ch = 45;    // Variable to store the radio channel

void setup() {
  // Serial communications setup
  Serial.begin(115200);

  pinMode(downPin, INPUT);
  pinMode(upPin, INPUT);
  pinMode(okPin, INPUT);

  // Show image buffer on the display hardware.
  // Since the buffer is intialized with an Adafruit splashscreen
  // internally, this will display the splashscreen.
  delay(250);                       // Wait for the OLED to power up
  display.begin(i2c_Address, true); // Address 0x3C default
  
  display.display();
  delay(2000);

  // Clear the buffer
  display.clearDisplay();
  display.setRotation(2);
  // Explicitly set correct colors for monochrome OLED screen
  menu.setForegroundColor(SH110X_WHITE);
  menu.setBackgroundColor(SH110X_BLACK);

  // Disable GEM splash (it won't be visible on the screen of buffer-equiped displays such as this one any way)
  menu.setSplashDelay(0);
  
  // Turn inverted order of characters during edit mode on (feels more natural when using encoder)
  menu.invertKeysDuringEdit(invert);
  
  // Menu init, setup and draw
  menu.init();
  setupMenu();
  menu.drawMenu();

  display.display();
  
  //Initialize 2.4G Radio 
  esp_bt_controller_deinit();
  esp_wifi_stop();
  esp_wifi_deinit();
  esp_wifi_disconnect();
  initHP();

  Serial.println("Initialized");
}

void setupMenu() {
  // Add menu items to main menu page
  menuPageMain.addMenuItem(menuItemMainBluetooth);
  menuPageMain.addMenuItem(menuItemMainWifi);
  menuPageMain.addMenuItem(menuItemMainSettings);
  menuPageMain.addMenuItem(menuItemButton);

  // Add menu items to Bluetooth menu page
  menuPageBluetooth.addMenuItem(menuItemJammerBluetooth);
  menuPageBluetooth.setParentMenuPage(menuPageMain);

  // Add menu items to Wifi menu page
  menuPageWifi.addMenuItem(menuItemJammerWifi);
  menuPageWifi.addMenuItem(menuItemScanWifi);
  menuPageWifi.setParentMenuPage(menuPageMain);

  // Add menu items to Settings menu page
  menuPageSettings.addMenuItem(menuItemInvert);
  menuPageSettings.setParentMenuPage(menuPageMain);

  // Add menu page to menu and set it as current
  menu.setMenuPageCurrent(menuPageMain);
}

void initHP() {
  hp = new SPIClass(HSPI);
  hp->begin();
  if (radio.begin(hp)) {
    Serial.println("HP Started !!!");
    radio.setAutoAck(false);
    radio.stopListening();
    radio.setRetries(0, 0);
    radio.setPALevel(RF24_PA_MAX, true);
    radio.setDataRate(RF24_2MBPS);
    radio.setCRCLength(RF24_CRC_DISABLED);
    radio.printPrettyDetails();
    radio.startConstCarrier(RF24_PA_MAX, ch);
  } else {
    Serial.println("HP couldn't start !!!");
  }
}

void loop() {
  // Get current time to use later on
  now = millis();

  // If menu is ready to accept button press...
  if (menu.readyForKey()) {
    chanB = digitalRead(downPin); // Reading Channel B signal beforehand to account for possible delays due to polling nature of KeyDetector algorithm
    // ...detect key press using KeyDetector library
    // and pass pressed button to menu
    myKeyDetector.detect();
  
    switch (myKeyDetector.trigger) {
      case KEY_UP:
        // Signal from Channel A of encoder was detected
        if (chanB == LOW) {
          // If channel B is low then the knob was rotated CCW
          if (myKeyDetector.current == KEY_OK) {
            // If push-button was pressed at that time, then treat this action as GEM_KEY_LEFT,...
            Serial.println("Rotation CCW with button pressed");
            menu.registerKeyPress(GEM_KEY_LEFT);
            // Button was in a pressed state during rotation of the knob, acting as a modifier to rotation action
            secondaryPressed = true;
          } else {
            // ...or GEM_KEY_UP otherwise
            Serial.println("Rotation CCW");
            menu.registerKeyPress(GEM_KEY_UP);
          }
        } else {
          // If channel B is high then the knob was rotated CW
          if (myKeyDetector.current == KEY_OK) {
            // If push-button was pressed at that time, then treat this action as GEM_KEY_RIGHT,...
            Serial.println("Rotation CW with button pressed");
            menu.registerKeyPress(GEM_KEY_RIGHT);
            // Button was in a pressed state during rotation of the knob, acting as a modifier to rotation action
            secondaryPressed = true;
          } else {
            // ...or GEM_KEY_DOWN otherwise
            Serial.println("Rotation CW");
            menu.registerKeyPress(GEM_KEY_DOWN);
          }
        }
        break;
      case KEY_OK:
        // Button was pressed
        Serial.println("Button pressed");
        // Save current time as a time of the key press event
        keyPressTime = now;
        break;
    }
    switch (myKeyDetector.triggerRelease) {
      case KEY_OK:
        // Button was released
        Serial.println("Button released");
        if (!secondaryPressed) {
          // If button was not used as a modifier to rotation action...
          if (now <= keyPressTime + keyPressDelay) {
            // ...and if not enough time passed since keyPressTime,
            // treat key that was pressed as Ok button
            menu.registerKeyPress(GEM_KEY_OK);
          }
        }
        secondaryPressed = false;
        cancelPressed = false;
        break;
    }
    // After keyPressDelay passed since keyPressTime
    if (now > keyPressTime + keyPressDelay) {
      switch (myKeyDetector.current) {
        case KEY_OK:
          if (!secondaryPressed && !cancelPressed) {
            // If button was not used as a modifier to rotation action, and Cancel action was not triggered yet
            Serial.println("Button remained pressed");
            // Treat key that was pressed as Cancel button
            menu.registerKeyPress(GEM_KEY_CANCEL);
            cancelPressed = true;
          }
          break;
      }
    }
    
    // Necessary to actually draw current state of the menu on screen
    display.display();
  }
}

void printData() {
  // If enablePrint flag is set to true (checkbox on screen is checked)...
  if (enablePrint) {
    // ...print the number to Serial
    Serial.print("Number is: ");
    Serial.println(number);
  } else {
    Serial.println("Printing is disabled, sorry:(");
  }
}

void jammerBluetooth(){
  Serial.println("Jamming bluetooth");
  while (true){
    //for (int i = 0; i < 120; i++) {
    //  radio.setChannel(i);
    //  delayMicroseconds(random(50));
    //  Serial.print("Jamming channel: "); Serial.println(i);
    //}
  radio.setChannel(random(80));
  delayMicroseconds(random(60));
  }
}

void jammerWifi(){
  Serial.println("Jamming wifi");
}

// Maximum characters that fit in one line with text size 1
#define MAX_CHARS_PER_LINE 21
#define NETWORKS_PER_PAGE 5
#define SCROLL_DELAY 300
#define SCAN_REFRESH_INTERVAL 10000 // Refresh scan every 10 seconds

void drawNetwork(int index, int yPos, int scrollOffset = 0) {
  String ssid = WiFi.SSID(index);
  String rssi = String(WiFi.RSSI(index));
  String prefix = String(index + 1) + ": ";
  
  display.setCursor(0, yPos);
  display.print(prefix);
  
  // If SSID is longer than available space, implement scrolling
  if (ssid.length() > MAX_CHARS_PER_LINE - prefix.length() - 6) { // -6 for RSSI
    int maxScroll = ssid.length() - (MAX_CHARS_PER_LINE - prefix.length() - 6);
    scrollOffset = scrollOffset % (maxScroll + 8); // Add 8 spaces for pause
    
    if (scrollOffset < maxScroll) {
      ssid = ssid.substring(scrollOffset);
    } else {
      ssid = ssid.substring(0); // Show from start during pause
    }
  }
  
  display.print(ssid);
  display.print(" ");
  display.print(rssi);
  display.println("dB");
}

void scanWifi() {
  // Set WiFi to station mode
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  int currentPage = 0;
  int textScrollOffset = 0;
  unsigned long lastScrollTime = millis();
  unsigned long lastScanTime = 0;
  bool exitScanner = false;
  int n = 0;
  int maxPages = 0;

  while (!exitScanner) {
    // Check if it's time to refresh the scan
    if (millis() - lastScanTime >= SCAN_REFRESH_INTERVAL || n == 0) {
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SH110X_WHITE);
      display.setCursor(0,10);
      display.println("Scanning WiFi...");
      display.display();

      // Delete previous scan results
      WiFi.scanDelete();
      
      // Start new scan
      n = WiFi.scanNetworks(true); // async scan
      while (n == WIFI_SCAN_RUNNING) {
        delay(1000);
        n = WiFi.scanComplete();
      }

      if (n == WIFI_SCAN_FAILED) {
        n = 0;
      }

      maxPages = (n + NETWORKS_PER_PAGE - 1) / NETWORKS_PER_PAGE;
      currentPage = min(currentPage, maxPages - 1);
      textScrollOffset = 0;
      lastScanTime = millis();

      if (n == 0) {
        display.clearDisplay();
        display.setCursor(0,10);
        display.println("No networks found");
        display.display();
        delay(1000);
        continue;
      }
    }

    // Update display with scroll effect
    if (millis() - lastScrollTime > SCROLL_DELAY) {
      textScrollOffset++;
      lastScrollTime = millis();

      display.clearDisplay();
      display.setCursor(0,10);
      display.print(n);
      display.print(" networks (Page ");
      display.print(currentPage + 1);
      display.print("/");
      display.print(maxPages);
      
      // Show time until next scan
      int secondsLeft = (SCAN_REFRESH_INTERVAL - (millis() - lastScanTime)) / 1000;
      display.print(" ");
      display.print(secondsLeft);
      display.println("s");

      int startIdx = currentPage * NETWORKS_PER_PAGE;
      int endIdx = min(startIdx + NETWORKS_PER_PAGE, n);

      for (int i = startIdx; i < endIdx; i++) {
        drawNetwork(i, 20 + (i - startIdx) * 10, textScrollOffset);
      }

      display.display();
    }

    // Check button inputs
    if (digitalRead(upPin) == LOW && currentPage > 0) {
      currentPage--;
      textScrollOffset = 0;
      delay(200); // Debounce
    }
    if (digitalRead(downPin) == LOW && currentPage < maxPages - 1) {
      currentPage++;
      textScrollOffset = 0;
      delay(200); // Debounce
    }
    if (digitalRead(okPin) == LOW) {
      exitScanner = true;
      delay(200); // Debounce
    }
  }

  // Clean up scan results before exiting
  WiFi.scanDelete();

  menu.drawMenu();
}

void applyInvert() {
  menu.invertKeysDuringEdit(invert);
  // Print invert variable to Serial
  Serial.print("Invert: ");
  Serial.println(invert);
}