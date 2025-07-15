// Konfigurace SIM800L
#define TINY_GSM_MODEM_SIM800
#define TINY_GSM_RX_BUFFER 1024

#include <Wire.h>
#include <TinyGsmClient.h>

// TTGO T-Call piny 
#define MODEM_RST       5
#define MODEM_PWKEY     4
#define MODEM_POWER_ON  23
#define MODEM_TX        27
#define MODEM_RX        26
#define I2C_SDA         21
#define I2C_SCL         22

// Sériové komunikace
#define SerialMon       Serial
#define SerialAT        Serial1

// IP5306 Power Management
#define IP5306_ADDR         0x75
#define IP5306_REG_SYS_CTL0 0x00

// Funkce pro správu napájení IP5306
bool setPowerBoostKeepOn(int en) {
    Wire.beginTransmission(IP5306_ADDR);
    Wire.write(IP5306_REG_SYS_CTL0);
    Wire.write(en ? 0x37 : 0x35);
    return Wire.endTransmission() == 0;
}

void setup() {
    SerialMon.begin(115200);
    delay(100);
    SerialMon.println("=== TTGO T-Call SMS Monitor ===");

    // I2C a IP5306
    Wire.begin(I2C_SDA, I2C_SCL);
    SerialMon.print("IP5306 KeepPower: ");
    SerialMon.println(setPowerBoostKeepOn(1) ? "OK" : "FAIL");

    // Zapnutí modemu
    pinMode(MODEM_PWKEY, OUTPUT);
    digitalWrite(MODEM_PWKEY, LOW);
    delay(100);
    digitalWrite(MODEM_PWKEY, HIGH);
    delay(1000); // Dáme modemu čas na start

    // UART pro SIM800L
    SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);

    SerialMon.println("Inicializace modemu...");

    // Čekání na síť
    if (!waitForNetworkRegistration(60000)) { // Dáme více času na registraci
        SerialMon.println("Chyba: Modem se nepodařilo zaregistrovat do sítě.");
        while (true); // Zastavíme vykonávání
    }

    initSMSMode();

    SerialMon.println("=== Monitor připraven ===");
}

void loop() {
    processIncomingData();

    // Periodická kontrola stavu
    static unsigned long lastCheck = 0;
    if (millis() - lastCheck > 30000) {
        lastCheck = millis();
        SerialAT.println("AT+CSQ"); // Zjistíme sílu signálu
    }
}

bool waitForNetworkRegistration(unsigned long timeout) {
    unsigned long startTime = millis();
    while (millis() - startTime < timeout) {
        SerialAT.println("AT+CREG?");
        String response = readSerialAT(1000);
        if (response.indexOf("+CREG: 0,1") != -1 || response.indexOf("+CREG: 0,5") != -1) {
            SerialMon.println("Síť nalezena a registrována.");
            return true;
        }
        delay(2000); // Dáme pauzu mezi dotazy
    }
    return false;
}

void initSMSMode() {
    SerialMon.println("Nastavuji SMS režim...");
    SerialAT.println("AT+CMGF=1"); // Textový režim
    delay(200);
    SerialAT.println("AT+CNMI=2,2,0,0,0"); // Notifikace o příchozích SMS
    delay(200);
}

void processIncomingData() {
    if (SerialAT.available()) {
        String response = readSerialAT(1000);
        SerialMon.print("Data z modemu: ");
        SerialMon.println(response);

        if (response.indexOf("+CMTI:") != -1) {
            // Zpracování notifikace o nové SMS
            // Extraktujeme index SMS a přečteme ji
            int startIndex = response.indexOf(',') + 1;
            int endIndex = response.length();
            String indexStr = response.substring(startIndex, endIndex);
            indexStr.trim();
            int smsIndex = indexStr.toInt();
            readSMS(smsIndex);
        }
    }
}

void readSMS(int index) {
    SerialMon.print("Čtu SMS na indexu: ");
    SerialMon.println(index);
    SerialAT.print("AT+CMGR=");
    SerialAT.println(index);
    String smsContent = readSerialAT(2000); // Dáme více času na přečtení
    SerialMon.println("Obsah SMS:");
    SerialMon.println(smsContent);

    // Smazání přečtené SMS
    SerialAT.print("AT+CMGD=");
    SerialAT.println(index);
    delay(200);
}

String readSerialAT(unsigned long timeout) {
    String response = "";
    unsigned long startTime = millis();
    while (millis() - startTime < timeout) {
        if (SerialAT.available()) {
            response += (char)SerialAT.read();
        }
    }
    return response;
}