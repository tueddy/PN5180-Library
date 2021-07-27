//for reference https://github.com/tueddy/PN5180-Library/blob/master/examples/PN5180-LowPowerCardDetection/PN5180-LPCD-ESP32-deep-sleep.ino

/*
 * demo for low power card detection to wake-up the ESP-32 from deep-sleep mode
 * 
 */
#include <PN5180.h>
#define PN5180_NSS  21
#define PN5180_BUSY 39
#define PN5180_RST  12
#define PN5180_IRQ  36 

#define LED_BUILTIN 22

// SPI controller used in ESP32 (ESP32-DevKitC v4 board (ESP32 WROOM 32D Module)) to conduct SPI communication is SPI3/VSPI. 
// So by default SCLK is Pin 18,MISO is Pin 19, MOSI is Pin 23

                           

/*
 * Method to print the reason by which ESP32 has been awaken from sleep
 */
void print_wakeup_reason(){
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
 }
}

/*
 * Method to print the GPIO that triggered the wakeup
 */
void print_GPIO_wake_up(){
  int GPIO_reason = esp_sleep_get_ext1_wakeup_status();
  Serial.print("GPIO that triggered the wake up: GPIO ");
  Serial.println((log(GPIO_reason))/log(2), 0);
}

PN5180 nfc(PN5180_NSS, PN5180_BUSY, PN5180_RST);

uint16_t wakeupCounterInMs = 0x3FF; //  must be in the range of 0x0 - 0xA82. max wake-up time is 2960 ms.

void setup() {
  Serial.begin(115200);
  Serial.println(F("PN5180 Demo Sketch for wake-up ESP-32 from deep-sleep with LPCD interrupt"));
  Serial.println("");
  pinMode(12, INPUT); // turn NeoPixel off
  // turn on LED
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  Serial.print("LED_BUILTIN: ");
  Serial.println(LED_BUILTIN);
  // print wakeup reason
  print_wakeup_reason();
  // print wakeup pin
  print_GPIO_wake_up();
    
  // disable pin hold from deep sleep 
  gpio_deep_sleep_hold_dis();
  gpio_hold_dis(gpio_num_t(PN5180_NSS)); // NSS
  gpio_hold_dis(gpio_num_t(PN5180_RST)); // RST

  // Init PN 5180
  pinMode(PN5180_IRQ, INPUT);
  nfc.begin();

  Serial.println(F("----------------------------------"));
  Serial.println(F("PN5180 Hard-Reset..."));
  nfc.reset();

  Serial.println(F("----------------------------------"));
  Serial.println(F("Reading product version..."));
  uint8_t productVersion[2];
  nfc.readEEprom(PRODUCT_VERSION, productVersion, sizeof(productVersion));
  Serial.print(F("Product version="));
  Serial.print(productVersion[1]);
  Serial.print(".");
  Serial.println(productVersion[0]);

  nfc.clearIRQStatus(0xffffffff);
  Serial.println(digitalRead(PN5180_IRQ)); //reads 0 because IRQ pin pin config is set to active high (eeprom@0x1A)  //should read 1 because when interrupt is raised GPIO4 is LOW
  Serial.println(F("Reading IRQ-Pin..."));
  uint8_t irqPin[1];
  nfc.readEEprom(IRQ_PIN_CONFIG, irqPin, sizeof(irqPin));
  Serial.print(F("irqPin="));
  Serial.println(irqPin[0]); //should read 1 i.e. pin IRQ is high(bolean 1/3.3v) when active(interrupted)  

  if (nfc.prepareLPCD()) {
    Serial.println("prepareLPCD success");
  }
  Serial.print("IRQ-pin:"); Serial.println(digitalRead(PN5180_IRQ));

  // turn on LPCD
  if (nfc.switchToLPCD(wakeupCounterInMs)) {
    Serial.println("switchToLPCD success");
      // configure wakeup pin for deep-sleep wake-up
      // esp_sleep_enable_ext0_wakeup(gpio_num_t(PN5180_IRQ), 1); //1 = High, 0 = Low
      esp_sleep_enable_ext1_wakeup((1ULL << (PN5180_IRQ)), ESP_EXT1_WAKEUP_ANY_HIGH);
      // freeze pin states in deep sleep
      gpio_hold_en(gpio_num_t(PN5180_NSS)); // NSS
      gpio_hold_en(gpio_num_t(PN5180_RST)); // RST
      gpio_deep_sleep_hold_en();
      Serial.println("Good night..");
      esp_deep_sleep_start();
      Serial.println("This will never be printed");
  } else {
    Serial.println("switchToLPCD failed");
  }
  // ++ go to sleep ++
  
}

void loop() {
  if (digitalRead(PN5180_IRQ) == HIGH) {
    showIRQStatus(nfc.getIRQStatus());
    uint32_t registerValue;
    nfc.readRegister(0x26, &registerValue);  Serial.print(registerValue); 
    delay(2000);
    nfc.reset(); //very important to have for lpcd to work
	
    // read card data here!
    // ..
    
    // turn back on LPCD
    if (nfc.switchToLPCD(wakeupCounterInMs)) {
      // LPCD success
      Serial.println("switchToLPCD success");
      // prepare deep sleep
      Serial.println("Going to sleep now");
      digitalWrite(LED_BUILTIN, HIGH);
      pinMode(LED_BUILTIN, INPUT);
      // configure wakeup pin for deep-sleep wake-up
      //esp_sleep_enable_ext0_wakeup(gpio_num_t(PN5180_IRQ), 1); //1 = High, 0 = Low
      esp_sleep_enable_ext1_wakeup(WAKEUP_PIN_MASK, ESP_EXT1_WAKEUP_ANY_HIGH);
      // freeze pin states in deep sleep
      gpio_hold_en(gpio_num_t(PN5180_NSS)); // NSS
      gpio_hold_en(gpio_num_t(PN5180_RST)); // RST
      gpio_deep_sleep_hold_en();
      Serial.println("Good night..");
      esp_deep_sleep_start();
      Serial.println("This will never be printed");
    } else {
      Serial.println("switchToLPCD failed");
    }
  }
}
void showIRQStatus(uint32_t irqStatus) {
    Serial.print(F("IRQ-Status 0x"));
    Serial.print(irqStatus, HEX);
    Serial.print(": [ ");
    if (irqStatus & (1 << 0)) Serial.print(F("RQ | "));
    if (irqStatus & (1 << 1)) Serial.print(F("TX | "));
    if (irqStatus & (1 << 2)) Serial.print(F("IDLE | "));
    if (irqStatus & (1 << 3)) Serial.print(F("MODE_DETECTED | "));
    if (irqStatus & (1 << 4)) Serial.print(F("CARD_ACTIVATED | "));
    if (irqStatus & (1 << 5)) Serial.print(F("STATE_CHANGE | "));
    if (irqStatus & (1 << 6)) Serial.print(F("RFOFF_DET "));
    if (irqStatus & (1 << 7)) Serial.print(F("RFON_DET "));
    if (irqStatus & (1 << 8)) Serial.print(F("TX_RFOFF | "));
    if (irqStatus & (1 << 9)) Serial.print(F("TX_RFON | "));
    if (irqStatus & (1 << 10)) Serial.print(F("RF_ACTIVE_ERROR "));
    if (irqStatus & (1 << 11)) Serial.print(F("TIMER0 "));
    if (irqStatus & (1 << 12)) Serial.print(F("TIMER1 "));
    if (irqStatus & (1 << 13)) Serial.print(F("TIMER2 "));
    if (irqStatus & (1 << 14)) Serial.print(F("RX_SOF_DET "));
    if (irqStatus & (1 << 15)) Serial.print(F("RX_SC_DET "));
    if (irqStatus & (1 << 16)) Serial.print(F("TEMPSENS_ERROR "));
    if (irqStatus & (1 << 17)) Serial.print(F("GENERAL_ERROR "));
    if (irqStatus & (1 << 18)) Serial.print(F("HV_ERROR "));
    if (irqStatus & (1 << 19)) Serial.print(F("LPCD"));
    Serial.println("]");
}
