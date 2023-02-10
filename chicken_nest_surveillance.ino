#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "esp_camera.h"
#include "esp_system.h"
#include <UniversalTelegramBot.h>

//********************************************************
// configurable stuff
//********************************************************

#define SSID "***********"
#define PASSWORD "***********"

// Initialize Telegram BOT
String BOTtoken = "*********:***********************************"; // your Bot Token (Get from Botfather)

String CHAT_ID = "**********";

//Sensor Pins
const int trigPin = 13;
const int echoPin = 15;

//Nest Params
const int minDistance = 5;
const int maxDistance = 40;

//********************************************************
// changing code after this at your own risk
//********************************************************

hw_timer_t *timer = NULL;

void IRAM_ATTR resetModule() {
  ets_printf("reboot\n");
  esp_restart();
}

WiFiClientSecure clientTCP;
UniversalTelegramBot bot(BOTtoken, clientTCP);

// Pin definition for CAMERA_MODEL_AI_THINKER
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27

#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

boolean startTimer = false;

int detected=0, photographedChicken=0, sittingChicken=0;
long duration;
int distance;

String sendPhotoTelegram() {
  const char* myDomain = "api.telegram.org";
  String getAll = "";
  String getBody = "";

  camera_fb_t * fb = NULL;
  fb = esp_camera_fb_get();

  if ( !fb ) {
    Serial.println("Camera capture failed");
    delay(1000);
    ESP.restart();
    return "Camera capture failed";
  }

  Serial.println("Connect to " + String(myDomain));

  clientTCP.setInsecure();
  if ( clientTCP.connect(myDomain, 443) ) {
    Serial.println("Connection successful");

    String head = "--RandomNerdTutorials\r\nContent-Disposition: form-data; name=\"chat_id\"; \r\n\r\n" + CHAT_ID +
    "\r\n--RandomNerdTutorials\r\nContent-Disposition: form-data; name=\"photo\"; filename=\"esp32-cam.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
    String tail = "\r\n--RandomNerdTutorials--\r\n";

    uint16_t imageLen = fb->len;
    uint16_t extraLen = head.length() + tail.length();
    uint16_t totalLen = imageLen + extraLen;

    clientTCP.println("POST /bot" + BOTtoken + "/sendPhoto HTTP/1.1");
    clientTCP.println("Host: " + String(myDomain));
    clientTCP.println("Content-Length: " + String(totalLen));
    clientTCP.println("Content-Type: multipart/form-data; boundary=RandomNerdTutorials");
    clientTCP.println();
    clientTCP.print(head);

    uint8_t *fbBuf = fb->buf;
    size_t fbLen = fb->len;
    
    for (size_t n = 0; n < fbLen; n = n + 1024) {
      if ( n + 1024 < fbLen ) {
        clientTCP.write(fbBuf, 1024);
        fbBuf += 1024;
      } else if ( fbLen % 1024 > 0 ) {
        size_t remainder = fbLen % 1024;
        clientTCP.write(fbBuf, remainder);
      }
    }

    clientTCP.print(tail);

    esp_camera_fb_return(fb);

    int waitTime = 10000; // timeout 10 seconds
    long startTimer = millis();
    boolean state = false;

    while ((startTimer + waitTime) > millis()) {
      Serial.print(".");
      delay(100);
    
      while (clientTCP.available()) {
        char c = clientTCP.read();
        if ( c == '\n' ) {
          if (getAll.length() == 0) state = true;
          getAll = "";
        } else if ( c != '\r' )
          getAll += String(c);
          if ( state == true ) getBody += String(c);
          startTimer = millis();
        }
      if ( getBody.length() > 0 ) break;
    }
  
    clientTCP.stop();
    Serial.println(getBody);
  } else {
    getBody = "Connected to api.telegram.org failed.";
    Serial.println("Connected to api.telegram.org failed.");
  }
  return getBody;
}

void setup() {
  Serial.begin(115200);

  while (!Serial) { ; }
  
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  WiFi.begin(SSID, PASSWORD);
  Serial.printf("WiFi connecting to %s\n", SSID);
  while(WiFi.status() != WL_CONNECTED) { Serial.print("."); delay(400); }
  Serial.printf("\nWiFi connected\nIP : ");
  Serial.println(WiFi.localIP());

  clientTCP.setCACert(TELEGRAM_CERTIFICATE_ROOT);

  timer = timerBegin(0, 80, true); //timer 0, div 80Mhz
  timerAttachInterrupt(timer, &resetModule, true);
  timerAlarmWrite(timer, 20000000, false);
  timerAlarmEnable(timer); //enable interrupt

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if( psramFound() ) {
    // FRAMESIZE_ +
    //QQVGA/160x120//QQVGA2/128x160//QCIF/176x144//HQVGA/240x176
    //QVGA/320x240//CIF/400x296//VGA/640x480//SVGA/800x600//XGA/1024x768
    //SXGA/1280x1024//UXGA/1600x1200//QXGA/2048*1536
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_QQVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  // Init Camera
  esp_err_t err = esp_camera_init(&config);
  if ( err != ESP_OK ) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();

  if ( s->id.PID == OV3660_PID ) {
    Serial.printf("esp_camera_sensor_get");
    s->set_vflip(s, 1); // flip it back
    s->set_brightness(s, 1); // up the brightness just a bit
    s->set_saturation(s, -2); // lower the saturation
    //s->set_hmirror(s, 1);
  }
}

void loop() {
  timerWrite(timer, 0); //reset timer (feed watchdog)
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  duration = pulseIn(echoPin, HIGH);
  distance= duration*0.034/2;
  Serial.print("Distance: ");
  Serial.println(distance);
  
  
  if ( distance <= minDistance ) { //incorrect measurement
    //do nothing
  } else if( distance <= maxDistance ) { //something happened in front of sonic sensor - maybe
    detected++;
  } else if ( detected > 0 ) { //nothing happened in front of sonic sensor - maybe
    detected--;
  }
  
  Serial.print(detected);

  //if something is detected for 30 Seconds
  if ( detected>30 ) {
    Serial.println("chick is sitting");
    sittingChicken=1;
    detected=30;
    
    //chicken is not photographed
    if ( photographedChicken == 0 ) {
      Serial.println("chicken say cheese!");
      Camera_capture();
      photographedChicken=1;
    }
  }

  //if nothing is detected for 30 Seconds
    if ( detected == 0 ) {
      Serial.println("chick is gone");
      sittingChicken=0;
      
      //reset capture status
      if ( photographedChicken == 1 )
        photographedChicken=0;
    }
    
  //loop every second
  delay(1000);
}

void Camera_capture() {
  Serial.println("capture chicken");
  sendPhotoTelegram();
}
