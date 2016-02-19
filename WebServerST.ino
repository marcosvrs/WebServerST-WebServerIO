/*
  Web server sketch for IDE v1.0.5 and w5100/w5200
  Originally posted November 2013 by SurferTim
  Last modified 18 January 2016 by MarcosVRS
*/

#include <Ethernet.h>
#include <SD.h>
//#include <utility/w5100.h>
#include <utility/socket.h>
//#include <IRremote.h>

// comment out the next line to eliminate the Serial.print stuff
// saves about 1.7K of program (flash) memory
#define ServerDEBUG

// comment out the next line to eliminate the Ethernet DHCP stuff
// saves about 3.7K of program (flash) memory
// if commented, you'll need to define the IPAddress's below
//#define ServerDHCP

#define PIN_OUTPUT_MIN 6
#define PIN_OUTPUT_MAX 9

// this must be unique
byte mac[] = { 0x90, 0xA2, 0xDA, 0x0F, 0x4D, 0x78 };

#ifndef ServerDHCP
IPAddress ip( 192, 168, 1, 50 );
IPAddress gateway( 192, 168, 1, 1 );
IPAddress subnet( 255, 255, 255, 0 );
#endif
EthernetServer server(80);
unsigned long connectTime[MAX_SOCK_NUM];

int freeRam() {
  extern int __heap_start, *__brkval;
  int v;
  return (int)&v - (!__brkval ? (int)&__heap_start : (int) __brkval);
}

void setup() {
#ifdef ServerDEBUG
  Serial.begin(115200);
#endif
  // disable w5100 SPI while starting SD
  digitalWrite(10, HIGH);
#ifdef ServerDEBUG
  Serial.print(F("Initializing SD card..."));
#endif
  if (!SD.begin(4)) {
#ifdef ServerDEBUG
    sendFailed();
#endif
    return;
#ifdef ServerDEBUG
  } else {
    sendOk();
#endif
  }
#ifndef ServerDEBUG
  pinMode(0, INPUT);
  pinMode(1, INPUT);
#endif
  pinMode(2, INPUT);
  pinMode(3, INPUT);

  for (byte x = PIN_OUTPUT_MIN; x <= PIN_OUTPUT_MAX; x++) {
    pinMode(x, OUTPUT);
  }

#ifdef ServerDEBUG
  Serial.print(F("Initializing ethernet..."));
#endif

#ifdef ServerDHCP
#ifdef ServerDEBUG
  Serial.print(F(" Getting IP..."));
#endif
  if (!Ethernet.begin(mac)) {
#ifdef ServerDEBUG
    sendFailed();
#endif
    return;
#ifdef ServerDEBUG
  } else {
    sendOk();
#endif
  }
#else
  Ethernet.begin(mac, ip, gateway, gateway, subnet);
#ifdef ServerDEBUG
  sendOk();
#endif
#endif

  delay(2000);
  server.begin();

  unsigned long thisTime = millis();
  for (byte i = 0; i < MAX_SOCK_NUM; i++) {
    connectTime[i] = thisTime;
  }

#ifdef ServerDEBUG
  Serial.println(F("Ready!"));
#endif
}

unsigned int requestNumber = 0;
boolean O_state[(PIN_OUTPUT_MAX - PIN_OUTPUT_MIN) + 1] = {0};

void loop() {
  checkServer();
  myStuff();
}

void myStuff() {
  /*#ifdef ServerDEBUG
    if (Serial.available()) {
    if (Serial.read() == 'r') ShowSockStatus();
    }
    #endif*/
  checkSockStatus();
#ifdef ServerDHCP
#ifdef ServerDEBUG
  switch (Ethernet.maintain()) {
    case DHCP_CHECK_RENEW_FAIL:
      Serial.println(F("DHCP renew failed."));
      break;
    case DHCP_CHECK_RENEW_OK:
      Serial.println(F("DHCP renew success."));
      break;
    case DHCP_CHECK_REBIND_FAIL:
      Serial.println(F("DHCP rebind fail."));
      break;
    case DHCP_CHECK_REBIND_OK:
      Serial.println(F("DHCP rebind success."));
      break;
    default:
      break;
  }
#else
  Ethernet.maintain();
#endif
#endif
}

/*void sendReverse(EthernetClient client, char *tBuf) {
  strcpy_P(tBuf, PSTR("maker.ifttt.com"));
  if (client.connect(tBuf, 443)) {
  client.print(F("GET /trigger/r/with/key/cw17pa84Z7iWFp6UeYHuUZ HTTP/1.1\r\nHost: maker.ifttt.com\r\nConnection: close"));
  client.stop();
  #ifdef ServerDEBUG
  Serial.println(F("PTTH Sent."));
  #endif
  }
  }*/

void checkServer() {
  EthernetClient client = server.available();
  if (client) {
    boolean currentLineIsBlank = true;
    boolean currentLineIsGet = true;
    byte tCount = 0;
    char *pch;
    char methodBuffer[8];
    char requestBuffer[48];
    char paramBuffer[48];
    //char protocolBuffer[9];
    char fileName[32];
    char fileType[4];
    char tBuf[65];
    requestNumber++;
#ifdef ServerDEBUG
    Serial.print(F("\r\nClient request #"));
    Serial.print(requestNumber, DEC);
    Serial.print(F(":"));
#endif
    // this controls the timeout
    unsigned int loopCount = 0;
    while (client.connected()) {
      while (client.available()) {
        char c = client.read();
        Serial.print(c);
        if (currentLineIsGet && tCount < 63) {
          tBuf[tCount] = c;
          tCount++;
          tBuf[tCount] = 0;
        }
        if (c == '\n' && currentLineIsBlank) {
          strtoupper(tBuf);
#ifdef ServerDEBUG
          //Serial.print(tBuf);
#endif
          while (client.available()) client.read();
          //int scanCount = sscanf_P(tBuf, PSTR("%7s %47s %8s"), methodBuffer, requestBuffer, protocolBuffer);
          //if (strstr_P(requestBuffer, PSTR("ajNrYTVkYWs=")) == NULL) sendBadRequest(client, tBuf); return;
          //if (scanCount != 3) {
          if (sscanf_P(tBuf, PSTR("%7s %47s %8s"), methodBuffer, requestBuffer) != 3) {
            sendBadRequest(client, tBuf);
            return;
          }
          pch = strtok_P(requestBuffer, PSTR("?"));
          if (pch != NULL) {
            strncpy(fileName, pch, 31);
            strncpy(tBuf, pch, 31);
            pch = strtok_P(NULL, PSTR("?"));
            if (pch != NULL) {
              strcpy(paramBuffer, pch);
            } else paramBuffer[0] = 0;
          }
          //strtoupper(requestBuffer);
          for (byte x = 0; x < strlen(requestBuffer); x++) {
            if (strchr_P(PSTR("ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890/.-_~"), requestBuffer[x]) == NULL) {
#ifdef ServerDEBUG
              Serial.println(F("Bad character!"));
#endif
              sendBadRequest(client, tBuf);
              return;
            }
          }
#ifdef ServerDEBUG
          Serial.print(F("File= "));
          Serial.println(requestBuffer);
#endif
          pch = strtok_P(tBuf, PSTR("."));
          if (pch != NULL) {
            pch = strtok_P(NULL, PSTR("."));
            if (pch != NULL) strncpy(fileType, pch, 4);
            else fileType[0] = 0;
#ifdef ServerDEBUG
            Serial.print(F("File type= "));
            Serial.println(fileType);
#endif
          }
#ifdef ServerDEBUG
          Serial.print(F("Method= "));
          Serial.println(methodBuffer);
#endif
          if (strcmp_P(methodBuffer, PSTR("GET")) && strcmp_P(methodBuffer, PSTR("HEAD"))) {
            sendBadRequest(client, tBuf);
            return;
          }
#ifdef ServerDEBUG
          Serial.print(F("Params= "));
          Serial.println(paramBuffer);
#endif
          pch = strtok_P(paramBuffer, PSTR("&"));
          while (pch != NULL) {
            if (!strncmp_P(pch, PSTR("D="), 2)) {
              byte d = atoi(pch + 2);
              SetIO(d);
#ifdef ServerDEBUG
              Serial.print(F("D="));
              Serial.println(d, DEC);
#endif
              /*} else if (!strncmp_P(pch, PSTR("T="), 2)) {
                byte d = strtoul((pch + 2), 0, 16);
                IRsend irsend;
                irsend.sendLG(d, 12);
                #ifdef ServerDEBUG
                Serial.print(F("T="));
                Serial.println(d, HEX);
                #endif
                } else if (!strncmp_P(pch, PSTR("S="), 2)) {
                byte d = strtoul((pch + 2), 0, 16);
                IRsend irsend;
                irsend.sendAiwaRCT501(d);
                #ifdef ServerDEBUG
                Serial.print(F("S="));
                Serial.println(d, HEX);
                #endif*/
            }
            pch = strtok_P(NULL, PSTR("&"));
          }
          /*#ifdef ServerDEBUG
            Serial.print(F("Protocol= "));
            Serial.println(protocolBuffer);
            #endif*/
          if (!strcmp_P(methodBuffer, PSTR("GET")) && !strcmp_P(fileName, PSTR("/IO.XML"))) {
#ifdef ServerDEBUG
            Serial.print(F("SRAM= "));
            Serial.println(freeRam());
            Serial.print(F("Sending XML reponse..."));
#else
            freeRam();
#endif
            client.write(sendHTTPHeader(tBuf, fileType));
            strcpy_P(tBuf, PSTR("<?xml version=\"1.0\"?><I>"));
            client.write(tBuf);
            for (byte x = 0; x <= (PIN_OUTPUT_MAX - PIN_OUTPUT_MIN); x++) {
              strcpy_P(tBuf, PSTR("<D>"));
              client.write(tBuf);
              itoa(O_state[x], tBuf, 10);
              client.write(tBuf);
              strcpy_P(tBuf, PSTR("</D>"));
              client.write(tBuf);
            }
            strcpy_P(tBuf, PSTR("</I>"));
            client.write(tBuf);
#ifdef ServerDEBUG
            sendOk();
#endif
          } else {
            if (!strcmp_P(fileName, PSTR("/"))) {
              strcpy_P(fileName, PSTR("/INDEX.HTM"));
              strcpy_P(fileType, PSTR("HTM"));
#ifdef ServerDEBUG
              Serial.println(F("Home page"));
#endif
            }
#ifdef ServerDEBUG
            Serial.print(F("SD file..."));
#endif
            if (strlen(fileName) > 30) {
#ifdef ServerDEBUG
              Serial.println(F(" filename too long!"));
#endif
              sendBadRequest(client, tBuf);
              return;
            } else if (strlen(fileType) > 3 || strlen(fileType) < 1) {
#ifdef ServerDEBUG
              Serial.println(F(" file type invalid size!"));
#endif
              sendBadRequest(client, tBuf);
              return;
            } else {
#ifdef ServerDEBUG
              Serial.print(F(" format OK..."));
#endif
              if (SD.exists(fileName)) {
#ifdef ServerDEBUG
                Serial.println(F(" found."));
                // SRAM check
                Serial.print(F("SRAM= "));
                Serial.println(freeRam());
                Serial.print(F("Opening file..."));
#else
                freeRam();
#endif
                File myFile = SD.open(fileName);
                if (!myFile) {
                  sendFileNotFound(client, tBuf);
                  return;
                }
#ifdef ServerDEBUG
                else sendOk();
#endif
                client.write(sendHTTPHeader(tBuf, fileType));
#ifdef ServerDEBUG
                Serial.print(F("Sending response..."));
#endif
                if (!strcmp_P(methodBuffer, PSTR("GET"))) {
                  while (myFile.available()) {
                    client.write((byte*)tBuf, myFile.read(tBuf, 64));
                  }
#ifdef ServerDEBUG
                  sendOk();
#endif
                }
                myFile.close();
#ifdef ServerDEBUG
                Serial.println(F("File closed."));
#endif
                client.stop();
#ifdef ServerDEBUG
                sendDisconnected();
#endif
                return;
              } else {
                sendFileNotFound(client, tBuf);
                return;
              }
            }
          }
          client.stop();
#ifdef ServerDEBUG
          sendDisconnected();
#endif
          return;
        } else if (c == '\n') {
          currentLineIsBlank = true;
          currentLineIsGet = false;
        } else if (c != '\r') {
          currentLineIsBlank = false;
        }
      }
      loopCount++;
      // if 1000ms has passed since last packet
      if (loopCount > 1000) {
        // close connection
        client.stop();
#ifdef ServerDEBUG
        Serial.println(F("\r\nTimeout."));
#endif
      }
      // delay 1ms for timeout timing
      delay(1);
    }
  }
}

#ifdef ServerDEBUG
void sendOk() {
  Serial.println(F(" ok."));
}

void sendFailed() {
  Serial.println(F(" FAILED!"));
}

void sendDisconnected() {
  Serial.println(F("Disconnected."));
}
#endif

char *sendHTTPHeader(char *_tBuf, char *_fileType) {
  strcpy_P(_tBuf, PSTR("HTTP/1.1 200 OK\r\nContent-Type: "));
  // send Content-Type
  if (!strcmp_P(_fileType, PSTR("HTM"))) strcat_P(_tBuf, PSTR("text/html"));
  else if (!strcmp_P(_fileType, PSTR("CSS"))) strcat_P(_tBuf, PSTR("text/css"));
  else if (!strcmp_P(_fileType, PSTR("JPG"))) strcat_P(_tBuf, PSTR("image/jpeg"));
  else if (!strcmp_P(_fileType, PSTR("JS"))) strcat_P(_tBuf, PSTR("application/javascript"));
  else if (!strcmp_P(_fileType, PSTR("ICO"))) strcat_P(_tBuf, PSTR("image/x-icon"));
  else if (!strcmp_P(_fileType, PSTR("GIF"))) strcat_P(_tBuf, PSTR("image/gif"));
  else if (!strcmp_P(_fileType, PSTR("PNG"))) strcat_P(_tBuf, PSTR("image/png"));
  else if (!strcmp_P(_fileType, PSTR("PHP"))) strcat_P(_tBuf, PSTR("text/html"));
  else if (!strcmp_P(_fileType, PSTR("PDF"))) strcat_P(_tBuf, PSTR("application/pdf"));
  else if (!strcmp_P(_fileType, PSTR("TXT"))) strcat_P(_tBuf, PSTR("text/plain"));
  else if (!strcmp_P(_fileType, PSTR("ZIP"))) strcat_P(_tBuf, PSTR("application/zip"));
  else if (!strcmp_P(_fileType, PSTR("XML"))) strcat_P(_tBuf, PSTR("text/xml"));
  else strcat_P(_tBuf, PSTR("text/plain"));
  strcat_P(_tBuf, PSTR("\r\nConnection: keep-alive\r\n\r\n"));
  return _tBuf;
}

void sendFileNotFound(EthernetClient _client, char *_tBuf) {
#ifdef ServerDEBUG
  Serial.println(F("File not found!"));
#endif
  strcpy_P(_tBuf, PSTR("HTTP/1.1 404 File Not Found\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n<html><body><h1>FILE NOT FOUND</h1></body></html>"));
  _client.write(_tBuf);
  _client.stop();
#ifdef ServerDEBUG
  sendDisconnected();
#endif
}

void sendBadRequest(EthernetClient _client, char *_tBuf) {
#ifdef ServerDEBUG
  Serial.println(F("Bad request!"));
#endif
  strcpy_P(_tBuf, PSTR("HTTP/1.1 400 Bad Request\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n<html><body><h1>BAD REQUEST</h1></body></html>"));
  _client.write(_tBuf);
  _client.stop();
#ifdef ServerDEBUG
  sendDisconnected();
#endif
}

void SetIO(byte a) {
  if (a == 99) {
    if (!O_state[0] && !O_state[1] && !O_state[2] && !O_state[3]) {
      for (byte x = PIN_OUTPUT_MIN; x <= PIN_OUTPUT_MAX; x++) {
        digitalWrite(x, HIGH);
        O_state[x - PIN_OUTPUT_MIN] = HIGH;
      }
    } else {
      for (byte x = PIN_OUTPUT_MIN; x <= PIN_OUTPUT_MAX; x++) {
        if (O_state[x - PIN_OUTPUT_MIN]) {
          digitalWrite(x, LOW);
          O_state[x - PIN_OUTPUT_MIN] = LOW;
        }
      }
    }
  } else {
    O_state[a - PIN_OUTPUT_MIN] = !O_state[a - PIN_OUTPUT_MIN];
    digitalWrite(a, O_state[a - PIN_OUTPUT_MIN]);
  }
}

void strtoupper(char *_tBuf) {
  while (*_tBuf) {
    *_tBuf = toupper(*_tBuf);
    _tBuf++;
  }
}

byte socketStat[MAX_SOCK_NUM];

/*void ShowSockStatus() {
  for (byte i = 0; i < MAX_SOCK_NUM; i++) {
  Serial.print(F("Socket#"));
  Serial.print(i, DEC);
  uint8_t s = W5100.readSnSR(i);
  socketStat[i] = s;
  Serial.print(F(":0x"));
  Serial.print(s, 16);
  //Serial.print(F(""));
  Serial.print(W5100.readSnPORT(i));
  Serial.print(F("D:"));
  uint8_t dip[4];
  W5100.readSnDIPR(i, dip);
  for (byte j = 0; j < 4; j++) {
  Serial.print(dip[j], 10);
  if (j < 3) Serial.print(F("."));
  }
  Serial.print(F("("));
  Serial.print(W5100.readSnDPORT(i));
  Serial.println(F(")"));
  }
  }*/

void checkSockStatus() {
  unsigned long _thisTime = millis();
  for (byte i = 0; i < MAX_SOCK_NUM; i++) {
    uint8_t s = W5100.readSnSR(i);
    if ((s == 0x17) || (s == 0x1C)) {
      if (_thisTime - connectTime[i] > 30000UL) {
#ifdef ServerDEBUG
        Serial.print(F("\r\nSocket frozen:"));
        Serial.println(i);
#endif
        close(i);
      }
    } else connectTime[i] = _thisTime;
    socketStat[i] = W5100.readSnSR(i);
  }
}
