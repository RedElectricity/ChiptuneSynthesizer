#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SPI.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

#ifdef ESP32
#include <WiFi.h>
#include <AsyncTCP.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#endif

#include <ESPAsyncWebServer.h>


AsyncWebServer webCP(80);

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

float getFreq (notes note, uint8_t octave) {
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

void playNote(uint8_t channel, notes note, uint8_t octave , uint16_t dur) {
    float freq = getFreq(note, octave);
    setTone(channel, (int)(Clock / (32.0 * freq)));
    delay(dur); // this is blocking, so we can only play one note at a time
}
 
void setup() {
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

    Serial.begin(115200);
    Serial.println("Chiptune Synthesizer Test Version by RedElectricity");
    Serial.println("Chiptune Synthesizer Booting...");


    // Boot File System
    if (LittleFS.begin()) {
        Serial.println("File System Boot successful...");
    }
    else {
        Serial.println("File System Boot fail...");

        ESP.restart();
    }


    // Read Config
    if (LittleFS.exists("config.json")) {
        Serial.printf("Found config file...");

        File configFile = LittleFS.open("config.json","r");
        String configString;
        for (uint8_t i = 0; i < configFile.size(); i++) {
            configString += (char)configFile.read();
        }
        
        deserializeJson(config,configString);
    }
    else {
        Serial.printf("Not found config file, init one...");
        display.display();
        String originConfig = "{\"wifi\":{\"type\":0,\"ssid\":\"ChiptuneSynthesizer\",\"pass\":\"chiptune\"},\"mode\":0}";
        File configFile = LittleFS.open("config.json","w");

        configFile.println(originConfig);
        configFile.close();

        ESP.restart();
    }
    
    
    if ((int)config["wifi"]["type"] == 0) {
        boolean startAP = WiFi.softAP(config["wifi"]["ssid"],config["wifi"]["pass"]);
        if (startAP == true) {
            Serial.println("Wifi AP boot successful...");
        } 
        else {
            Serial.println("Wifi AP boot fail...");
            ESP.restart();
        }
    }
    else if ((int)config["wifi"]["type"] == 1) {
        WiFi.begin((char*)config["wifi"]["ssid"],(char*)config["wifi"]["pass"]);
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("Wifi connect fail, try to boot AP...");

            boolean startAP = WiFi.softAP((char*)config["wifi"]["ssid"],(char*)config["wifi"]["pass"]);
            if (startAP == true) {
                Serial.println("Wifi AP boot successful...");
            } 
            else {
                Serial.println("Wifi AP boot fail...");
                ESP.restart();
            }
        }
    }
    
    Serial.println("Init Pins...");

    for (uint8_t i = 0; i < 8; i++) {
        pinMode(dPins[i], OUTPUT);
        digitalWrite(dPins[i], LOW);
    }
    
    pinMode(WEPin, OUTPUT);
    digitalWrite(WEPin, HIGH);

    muteAll();

    setVolume(1,15);
    playNote(1, C, 5, 5000);

    Serial.println("Boot Successful, Ready to play!");

    /*
        WebCP Part - Normal
        We have `Play` and `Setting`
    */
    webCP.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        String indexHtml = LittleFS.open("index.html","r").readString();
        request->send(200, "text/html", indexHtml);
    });


    /*
        WebCP - API
        We have `PlayVgm` and `Upload Setting`
    */
    webCP.on("/playVgm", HTTP_POST, [](AsyncWebServerRequest *request) {

    });

}

void loop() {
    
}