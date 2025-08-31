#include <WiFi.h>
#include <WiFiUdp.h>
#include <driver/i2s.h>

//====WIFI conifg====
const char* ssid = "Marka";
const char* password = "12345465";
const char* receiverIP = "192.168.31.72";  // Replace with your PC IP
const uint16_t receiverPort = 12345;

WiFiUDP udp;

void setupWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected, IP: " + WiFi.localIP().toString());
  delay(5000);
}

/*
//====CONFIG MAX9814=====
#define MIC_PIN 35
#define SAMPLE_RATE 8000
#define SAMPLES_PER_PACKET 128

const int voiceThreshold = 700;  // Adjust based on your mic/noise
const int silenceTimeout = 2000; //ms

unsigned long lastVoiceTime = 0; //lastMicros
const int interval = 1000000 / SAMPLE_RATE;
uint8_t buffer[SAMPLES_PER_PACKET * 2];  // 2 bytes per sample
int bufIndex = 0;

bool isSpeaking = false;

void setup() {
  Serial.begin(115200);
  setupWiFi();

  analogReadResolution(12); // 0-4095 range
}
*/


//====CONFIG INMP441====
#define I2S_SD 33
#define I2S_WS 25
#define I2S_SCK 32

//i2s_config
#define I2S_PORT I2S_NUM_0
#define SAMPLE_RATE     16000
//#define SAMPLE_BITS 32

//i2s_read
#define SAMPLE_BUFFER_SIZE 4096

const int bufLen = 1024;
const int bufLenBytes = bufLen * 4; //Bytes to read from i2s
uint32_t buf[bufLen];

esp_err_t i2s_install()
{
  const i2s_config_t i2s_config = {
    .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE, //44100,
    //.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
    .communication_format = I2S_COMM_FORMAT_I2S_MSB,
    //.communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1, // default interrupt priority
    .dma_buf_count = 4,
    //.dma_buf_count = 4,
    .dma_buf_len = SAMPLE_BUFFER_SIZE / 4,
    .use_apll = false, //true
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };

  return i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
}

esp_err_t i2s_setpin()
{
  const i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE, //-1,
    .data_in_num = I2S_SD};

  return i2s_set_pin(I2S_PORT, &pin_config);
}

void setup() {
  Serial.begin(115200);
  setupWiFi();

  Serial.println("Setup I2S ...");

  auto res = i2s_install();
  Serial.printf("i2s_install: %d\n", res);
  if (res != 0)
  {
    while (1)
      delay(99999);
  }

  res = i2s_setpin();
  Serial.printf("i2s_setpin: %d\n", res);
  if (res != 0)
  {
    while (1)
      delay(99999);
  }

  res = i2s_start(I2S_PORT);
  Serial.printf("i2s_start: %d\n", res);
  if (res != 0)
  {
    while (1)
      delay(99999);
  }

  Serial.println("Done!");
}

/*
//====MAX9814 (working)====
void loop() {
  if (micros() - lastVoiceTime >= interval) {
    lastVoiceTime += interval;

    int sample = analogRead(MIC_PIN);  // 0–4095
    buffer[bufIndex++] = sample & 0xFF;
    buffer[bufIndex++] = (sample >> 8) & 0xFF;

    if (bufIndex >= sizeof(buffer)) {
      udp.beginPacket(receiverIP, receiverPort);
      udp.write(buffer, bufIndex);
      udp.endPacket();
      bufIndex = 0;
    }
  }
}
*/


//====INMP441====
void loop() {
  uint8_t buffer[SAMPLE_BUFFER_SIZE];
  size_t bytesRead;

  i2s_read(I2S_PORT, &buffer, sizeof(buffer), &bytesRead, portMAX_DELAY);
  udp.beginPacket(receiverIP, receiverPort);
  udp.write(buffer, bytesRead);
  udp.endPacket();
}


/*
//====MAX9814 with speech detection (not working)====
void loop() {
  bool voiceDetected = false;

  // Collect samples
  for (int i = 0; i < SAMPLES_PER_PACKET; i++) {
    int sample = analogRead(MIC_PIN); // 0-4095
    int delta = abs(sample - 2048);
    Serial.println(delta);


    if (delta > voiceThreshold) {
      voiceDetected = true;
    }

    // convert to 2-byte little-endian for sending
    buffer[2 * i]     = sample & 0xFF;
    buffer[2 * i + 1] = (sample >> 8) & 0xFF;
    
    delayMicroseconds(1000000 / SAMPLE_RATE);
  }

  // === Decide Whether to Send ===
  if (voiceDetected) {
    isSpeaking = true;
    lastVoiceTime = millis();

    udp.beginPacket(receiverIP, receiverPort);
    udp.write(buffer, sizeof(buffer));
    udp.endPacket();

    Serial.println(">> Sending audio...");
  } 
  else if (isSpeaking && (millis() - lastVoiceTime > silenceTimeout)) {
    isSpeaking = false;
    Serial.println("<< Silence detected, stopping transmission.");
  }
}
*/