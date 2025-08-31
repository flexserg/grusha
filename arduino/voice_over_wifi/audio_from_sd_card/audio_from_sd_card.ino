#include <SD.h>
#include <SPI.h>
#include <driver/i2s.h>

#define I2S_NUM         I2S_NUM_0
#define SD_CS_PIN       5
#define WAV_FILENAME    "/2.wav"

// I2S pins
#define I2S_DOUT 14
#define I2S_BCLK 12
#define I2S_LRC  13

void setup() {
  Serial.begin(115200);

  // Init SD
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("SD failed");
    while (1);
  }
  Serial.println("SD ok");

  // Init I2S
  i2s_config_t i2s_config = {
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

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_BCLK,
    .ws_io_num = I2S_LRC,
    .data_out_num = I2S_DOUT,
    .data_in_num = I2S_PIN_NO_CHANGE
  };

  i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM, &pin_config);
  i2s_zero_dma_buffer(I2S_NUM);

  playWav(WAV_FILENAME);
}

void loop() {
  // Nothing in loop
}

void skipWavHeader(File &file) {
  uint8_t header[44];
  file.read(header, 44);
}

void playWav(const char *filename) {
  File file = SD.open(filename);
  if (!file) {
    Serial.println("Failed to open WAV file!");
    return;
  }

  Serial.println("🎵 Playing: " + String(filename));
  skipWavHeader(file);

  uint8_t buffer[512];
  size_t bytesRead;

  while ((bytesRead = file.read(buffer, sizeof(buffer))) > 0) {
    size_t bytesWritten = 0;
    i2s_write(I2S_NUM, buffer, bytesRead, &bytesWritten, portMAX_DELAY);
  }

  Serial.println("Playback finished.");
  file.close();
}
