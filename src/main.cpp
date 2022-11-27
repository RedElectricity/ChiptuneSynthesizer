#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFSEditor.h>

AsyncWebServer server(80);

int dPins[] = {16,5,4,0,2,14,12,13};
int WEPin = 10;

void SendByte(byte b)
{
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

void setup() {
    Serial.begin(115200);
    Serial.printf("Chiptune Synthesizer Booting...");

    boolean startAP = WiFi.softAP("Chiptune Synthesizer","chiptune");
    if (startAP == true)
    {
        Serial.printf("Wifi boot successful...");
        Serial.printf("SSID: Chiptune Synthesizer Password: chiptune");
    } 
    else
    {
        Serial.printf("Wifi boot fail...");
    }
    
    Serial.printf("Init Pins...");

    for (uint8_t i = 0; i < 8; i++)
    {
        pinMode(dPins[i], OUTPUT);
        digitalWrite(dPins[i], LOW);
    }
    
    pinMode(WEPin, OUTPUT);
    digitalWrite(WEPin, HIGH);

    Serial.printf("Boot Successful, Ready to play!");

}