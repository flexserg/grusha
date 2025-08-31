#include <WiFi.h>
#include <WiFiUdp.h>
#include "driver/i2s.h"

const char* ssid = "Marka";
const char* password = "12345465";

WiFiUDP udp;
const int localPort = 12345;

#define I2S_DOUT 14
#define I2S_BCLK 12
#define I2S_LRC  13
#define I2S_PORT I2S_NUM_0

#define AUDIO_BUFFER_SIZE 250000  // 100 KB max
uint8_t* audioBuffer;
size_t totalReceived = 0;
bool receiving = true;

void setupWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("Connected. IP address: " + WiFi.localIP().toString());
  udp.begin(localPort);
}

void setupI2S() {
  const i2s_config_t i2s_config = {
    .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = 16000, // Match WAV file sample rate (or 44100)
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,  // Mono
    .communication_format = I2S_COMM_FORMAT_I2S_MSB,
    .intr_alloc_flags = 0,
    .dma_buf_count = 8,
    .dma_buf_len = 512,
    .use_apll = false,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };

  const i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_BCLK,
    .ws_io_num = I2S_LRC,
    .data_out_num = I2S_DOUT,
    .data_in_num = I2S_PIN_NO_CHANGE
  };

  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin_config);
}

void setup() {
  Serial.begin(115200);
  setupWiFi();
  setupI2S();
  udp.begin(localPort);
  Serial.println("Listening for WAV data...");

 /* 
  // Allocate buffer
  audioBuffer = (uint8_t*)malloc(AUDIO_BUFFER_SIZE);
  if (!audioBuffer) {
    Serial.println("Failed to allocate audio buffer!");
    while (true);
  }
  */
}

void loop() {
  static uint8_t packet[1024];
  int len = udp.parsePacket();
  if (len > 0) {
    Serial.print("Received bytes: ");
    Serial.println(len);
    int bytesRead = udp.read(packet, sizeof(packet));
    size_t bytesWritten;
    i2s_write(I2S_PORT, packet, bytesRead, &bytesWritten, portMAX_DELAY);
  }
}

/*
void loop() {
  if (receiving) {
    int packetSize = udp.parsePacket();
    if (packetSize > 0) {
      if (totalReceived + packetSize <= AUDIO_BUFFER_SIZE) {
        int len = udp.read(audioBuffer + totalReceived, packetSize);
        totalReceived += len;
        Serial.printf("Received: %d bytes, Total: %d\n", len, totalReceived);
      } else {
        Serial.println("Buffer full, stopping reception.");
        receiving = false;
      }
    }
  } else {
    Serial.println("Starting playback...");
    size_t bytesWritten;
    i2s_write(I2S_PORT, audioBuffer, totalReceived, &bytesWritten, portMAX_DELAY);
    Serial.println("Playback done.");
    delay(5000);  // Avoid repeating
  }
}
*/