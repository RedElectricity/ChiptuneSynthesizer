#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SPI.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

AsyncWebServer server(80);

int dPins[] = {16,15,10,0,2,14,12,13};
int WEPin = 10;
float Clock = 4000000.0;

/*
    Config Scheme
    {
        "wifi": {
            "type": 0,
            "ssid": "ChiptuneSynthesizer",
            "pass": "chiptune"
        },
        "mode": 0
    }
*/
DynamicJsonDocument config(1024);

Adafruit_SSD1306 display = Adafruit_SSD1306(128, 64, &Wire, -1);

enum latchType {
    Tone,
    Volume
};

enum notes {
    A, AS, B, C, CS, D, DS, E, F, FS, G, GS
};

float getFreq (notes note, uint8 octave) {
    int keyNum;

    if (note < 3)
    {
        keyNum = note + 12 + ((octave - 1)* 12) + 1;
    }
    else
    {
        keyNum = note + ((octave - 1)* 12) + 1;
    }
    
    return 440 * pow(2, (keyNum - 49) / 12);
}

void SendByte(byte b) {
  digitalWrite(dPins[0], (b&1)?HIGH:LOW);
  digitalWrite(dPins[1], (b&2)?HIGH:LOW);
  digitalWrite(dPins[2], (b&4)?HIGH:LOW);
  digitalWrite(dPins[3], (b&8)?HIGH:LOW);
  digitalWrite(dPins[4], (b&16)?HIGH:LOW);
  digitalWrite(dPins[5], (b&32)?HIGH:LOW);
  digitalWrite(dPins[6], (b&64)?HIGH:LOW);
  digitalWrite(dPins[7], (b&128)?HIGH:LOW);
  digitalWrite(WEPin, HIGH);
  digitalWrite(WEPin, LOW);
  digitalWrite(WEPin, HIGH);
}

void muteAll() {
    SendByte(0x9F);
    SendByte(0xBF);
    SendByte(0xDF);
    SendByte(0xFF);
}

uint8_t newLatchByte(latchType type) {
    byte newByte = 0x80;

    if (type == Volume) {
        newByte |= (1 << 4);
    }
    
    return newByte;
}

void setChannel(uint8_t channel, byte &latchByte) {
    latchByte &= 0x9F;
    latchByte |= (channel << 5);
}

void setData(uint8_t &lByte, uint8_t &dByte, uint16_t dataVal) {
    lByte &= (lByte & 0b11110000);
    lByte |= (dataVal & 0b0000000000001111);
    dByte = (uint8_t)((dataVal >> 4) & 0b00111111);
}

void setVolume(uint8_t channel, uint8_t vol) {
    byte latchByte = newLatchByte(Volume);
    setChannel(channel, latchByte);
    if(vol > 15) vol = 15;
    vol = 15 - vol;
    latchByte |= (0b00001111 & vol);
    SendByte(latchByte);
}

void setTone(uint8_t channel, uint16_t toneVal) {
    if(channel < 3) {
        byte latchByte = newLatchByte(Tone);
        byte dataByte = 0b00000000;
        setChannel(channel, latchByte);
        setData(latchByte, dataByte, toneVal);
        SendByte(latchByte);
        SendByte(dataByte);
    }
}

void playNote(uint8_t channel, notes note, uint8 octave , uint16_t dur) {
    float freq = getFreq(note, octave);
    setTone(channel, (int)(Clock / (32.0 * freq)));
    delay(dur); // this is blocking, so we can only play one note at a time
}
 
void setup() {
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    display.clearDisplay();
    display.setTextSize(1);
    display.println("Chiptune Synthesizer Test Version by RedElectricity");
    display.display();

    Serial.begin(115200);
    display.println("Chiptune Synthesizer Booting...");
    display.display();

    // Boot File System
    if (LittleFS.begin()) {
        display.println("File System Boot successful...");
        display.display();
    }
    else {
        display.println("File System Boot fail...");
        display.display();

        ESP.restart();
    }


    // Read Config
    if (LittleFS.exists("config.json")) {
        display.printf("Found config file...");
        display.display();

        File configFile = LittleFS.open("config.json","r");
        String configString;
        for (uint8 i = 0; i < configFile.size(); i++) {
            configString += (char)configFile.read();
        }
        
        deserializeJson(config,configString);
    }
    else {
        display.printf("Not found config file, init one...");
        display.display();
        String originConfig = "{\"wifi\":{\"type\":0,\"ssid\":\"ChiptuneSynthesizer\",\"pass\":\"chiptune\"},\"mode\":0}";
        File configFile = LittleFS.open("config.json","w");

        configFile.println(originConfig);
        configFile.close();

        ESP.restart();
    }
    
    
    if ((int)config["wifi"]["type"] == 0) {
        boolean startAP = WiFi.softAP((String)config["wifi"]["ssid"],(String)config["wifi"]["pass"]);
        if (startAP == true) {
            display.println("Wifi AP boot successful...");
            display.display();
        } 
        else {
            display.println("Wifi AP boot fail...");
            display.display();
            ESP.restart();
        }
    }
    else if ((int)config["wifi"]["type"] == 1) {
        WiFi.begin((String)config["wifi"]["ssid"],(String)config["wifi"]["pass"]);
        if (WiFi.status() != WL_CONNECTED) {
            display.println("Wifi connect fail, try to boot AP...");
            display.display();

            boolean startAP = WiFi.softAP((String)config["wifi"]["ssid"],(String)config["wifi"]["pass"]);
            if (startAP == true) {
                display.println("Wifi AP boot successful...");
                display.display();
            } 
            else {
                display.println("Wifi AP boot fail...");
                display.display();
                ESP.restart();
            }
        }
    }
    
    display.println("Init Pins...");

    for (uint8_t i = 0; i < 8; i++) {
        pinMode(dPins[i], OUTPUT);
        digitalWrite(dPins[i], LOW);
    }
    
    pinMode(WEPin, OUTPUT);
    digitalWrite(WEPin, HIGH);

    muteAll();

    playNote(1, C, 5, 5000);

    display.println("Boot Successful, Ready to play!");
    display.display();

}

void loop() {
    
}