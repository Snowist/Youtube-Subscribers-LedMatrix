#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <YoutubeApi.h>         
#include <ArduinoJson.h>                   
#include <MD_Parola.h>
#include <MD_MAX72xx.h>

#define DEBUG 1
#define SPRITE 1
#define BUZZER 0
#define SHOWSUBSCOUNT 1
#define SHOWVIEWCOUNT 1
#define SHOWVIDEOCOUNT 1

#if DEBUG
#define PRINT(s, v)   { Serial.print(F(s)); Serial.print(v); }        // Print a string followed by a value (decimal)
#define PRINTX(s, v)  { Serial.print(F(s)); Serial.print(v, HEX); }   // Print a string followed by a value (hex)
#define PRINTB(s, v)  { Serial.print(F(s)); Serial.print(v, BIN); }   // Print a string followed by a value (binary)
#define PRINTC(s, v)  { Serial.print(F(s)); Serial.print((char)v); }  // Print a string followed by a value (char)
#define PRINTS(s)     { Serial.print(F(s)); }                         // Print a string
#else
#define PRINT(s, v)   // Print a string followed by a value (decimal)
#define PRINTX(s, v)  // Print a string followed by a value (hex)
#define PRINTB(s, v)  // Print a string followed by a value (binary)
#define PRINTC(s, v)  // Print a string followed by a value (char)
#define PRINTS(s)     // Print a string
#endif

// MAX7219 control pins and number of chips
#define MAX_DEVICES 4 //how many 8x8 panels
#define CLK_PIN     D5
#define CS_PIN      D6
#define DATA_PIN    D7  // DIN

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define BUZZER_PIN D4

// Hardware SPI connection
MD_Parola P = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);
// Arbitrary pins
//MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);

//------- Replace with your details! ------
char ssid[] = "";                // your wifi SSID (name)
char password[] = "";            // your wifi pass

// Get API_KEY by following this guide https://www.slickremix.com/docs/get-api-key-for-youtube/ or https://www.youtube.com/watch?v=VqML5F8hcRQ
// Get CHANNEL_ID from this page https://www.youtube.com/account_advanced
#define API_KEY ""
#define CHANNEL_ID ""   

bool isSubsHidden;
unsigned long subCount = 0;                // stores the subscriber count from the API
unsigned long viewCount;
unsigned long videoCount;
unsigned long api_mtbs = 180000;           // time between api requests, in mS.  60,000 mS = 1 minute intervals
unsigned long api_lasttime = 0;            // last time api request occurred
char displayBuffer[12];
String thisString_prev;
uint8_t i = 0;
uint8_t scrollSpeed = 25;                  // default frame delay value
uint16_t scrollPause = 1500;               // in milliseconds

static textPosition_t scrollAlign = PA_CENTER;

uint8_t  inFX, outFX;
textEffect_t  effect[] =
{
  PA_PRINT,
  PA_SCAN_HORIZ,
  PA_SCROLL_LEFT,
  PA_WIPE,
  PA_SCROLL_UP_LEFT,
  PA_SCROLL_UP,
  PA_OPENING_CURSOR,
  PA_GROW_UP,
  PA_MESH,
  PA_SCROLL_UP_RIGHT,
  PA_BLINDS,
  PA_CLOSING,
  PA_RANDOM,
  PA_GROW_DOWN,
  PA_SCAN_VERT,
  PA_SCROLL_DOWN_LEFT,
  PA_WIPE_CURSOR,
  PA_DISSOLVE,
  PA_OPENING,
  PA_CLOSING_CURSOR,
  PA_SCROLL_DOWN_RIGHT,
  PA_SCROLL_RIGHT,
  PA_SLICE,
  PA_SCROLL_DOWN,
};

const char  *msg[] = { "Hello", "WiFi..", "WiFi Ok", "Wait.", "Subs:", "View:", "Video Count:", };

WiFiClientSecure client;                   // Instantiate the WiFiClientSecure object for use with Youtube API
YoutubeApi api(API_KEY, client);

#if SPRITE
  // Sprite Definitions
const uint8_t F_PMAN = 6;
const uint8_t W_PMAN = 8;
const uint8_t PROGMEM pacman[F_PMAN * W_PMAN] =  // gobbling pacman animation
{
  0x00, 0x81, 0xc3, 0xe7, 0xff, 0x7e, 0x7e, 0x3c,
  0x00, 0x42, 0xe7, 0xe7, 0xff, 0xff, 0x7e, 0x3c,
  0x24, 0x66, 0xe7, 0xff, 0xff, 0xff, 0x7e, 0x3c,
  0x3c, 0x7e, 0xff, 0xff, 0xff, 0xff, 0x7e, 0x3c,
  0x24, 0x66, 0xe7, 0xff, 0xff, 0xff, 0x7e, 0x3c,
  0x00, 0x42, 0xe7, 0xe7, 0xff, 0xff, 0x7e, 0x3c,
};

const uint8_t F_YT = 1;
const uint8_t W_YT = 9;
const uint8_t PROGMEM yt[F_YT * W_YT] =   // YouTube sprite
{
  254, 254, 254, 130, 198, 238, 254, 254, 254,
};
#endif

void setup() {
  client.setInsecure();                    // Required if you are using ESP8266 V2.5 or above
  P.begin();
  P.setIntensity(5); 
  P.print(msg[0]);  // Hello
  delay(1200);

#if BUZZER
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
#endif

#if SPRITE
  P.setSpriteData(yt, W_YT, F_YT, // entry sprite
                  pacman, W_PMAN, F_PMAN); // exit sprite
#endif 

  // set WiFi to station mode and disconnect from an AP if previously connected
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

#if DEBUG
  Serial.begin(115200);
  PRINTS("\nConnecting to WiFi");
#endif
  P.write(msg[1]);  // WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    PRINTS(".");
    delay(500);
    }

  // show WiFi status in serial monitor
  PRINTS("\nWiFi connected.");
  P.write(msg[2]);  //WiFi Ok
  delay(800);
  
  IPAddress ip = WiFi.localIP();
  PRINT("\nIP address: ", ip);
  
  // get channel info from Youtube API for initial display
  pollYoutubeAPI();    // get sub count immediately so we don't wait for poll timer to expire for initial reading
  displaySerialReport();     // show the counts
}  // end setup()

void loop() {
  if(P.displayAnimate()){  // animates and returns true when an animation is completed
      switch(i){
        case 0: 
         P.setTextBuffer(msg[3]); // Wait
         P.setTextEffect(effect[random(ARRAY_SIZE(effect))], effect[random(ARRAY_SIZE(effect))]);
         P.setTextAlignment(scrollAlign);
         P.setSpeedInOut(scrollSpeed, scrollSpeed);
         P.setPause(scrollPause);
        break;
        case 1: 
         if(!SHOWSUBSCOUNT) break;
         P.setTextBuffer(msg[4]); // Subs count
         P.setTextEffect(effect[random(ARRAY_SIZE(effect))], PA_SCROLL_LEFT);
         P.setPause(scrollPause);
        break;
        case 2:
         if(!SHOWSUBSCOUNT) break;
         sprintf(displayBuffer, "%05d", subCount ); 
         P.setTextBuffer(displayBuffer);
         P.setTextEffect(PA_SCROLL_LEFT, PA_SCROLL_LEFT);
        break;
        case 3: 
         if(!SHOWVIEWCOUNT) break;
         P.setTextBuffer(msg[5]); // View count
         P.setTextEffect(effect[random(ARRAY_SIZE(effect))], PA_SCROLL_LEFT);
        break;
        case 4:
         if(!SHOWVIEWCOUNT) break;
         sprintf(displayBuffer, "%05d", viewCount ); 
         P.setTextBuffer(displayBuffer);
         P.setTextEffect(PA_SCROLL_LEFT, PA_SCROLL_LEFT);
        break;
        case 5:
         if(!SHOWVIDEOCOUNT) break;
         P.setTextBuffer(msg[6]); // Video count:
         P.setTextEffect(PA_SCROLL_LEFT, PA_SCROLL_LEFT);
         P.setPause(0);
        break;
        case 6:
         if(!SHOWVIDEOCOUNT) break;
         sprintf(displayBuffer, "%05d", videoCount ); 
         P.setTextBuffer(displayBuffer);
         P.setTextEffect(PA_SCROLL_LEFT, PA_SCROLL_LEFT);
         P.setPause(scrollPause);
        break;        
      }
      if (i++ >= 6){ i = 1; }
      delay(500);

      if (millis() > api_lasttime + api_mtbs) {
        P.setTextBuffer(msg[3]);  // Wait
        pollYoutubeAPI(); 
        displaySerialReport();
      }
      P.displayReset();     
  }
}

void displaySerialReport() {
  PRINTS("\n---------Stats---------");
  PRINT("\nSubscriber Count: ", subCount);
  PRINT("\nSubscriber Hidden?: ", isSubsHidden);
  PRINT("\nView Count: ", viewCount);
  PRINT("\nVideo Count: ", videoCount);
  PRINTS("\n-----------------------");
}  

void pollYoutubeAPI() {
  if (api.getChannelStatistics(CHANNEL_ID)) {
    subCount  = api.channelStats.subscriberCount;
    isSubsHidden  = api.channelStats.hiddenSubscriberCount;
    viewCount = api.channelStats.viewCount;
    videoCount = api.channelStats.videoCount;
    
#if BUZZER
    String thisString = String(subCount, DEC);
    if(thisString != thisString_prev)
      {
        analogWrite(BUZZER_PIN,2000);
        delay(1000);
        analogWrite(BUZZER_PIN,LOW);       
      }
    thisString_prev = thisString;    
#endif    
  }
  api_lasttime = millis();   // start a new delay for next API poll interval
} 
