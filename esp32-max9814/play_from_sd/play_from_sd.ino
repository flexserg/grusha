#include <SD.h>
#include <SPI.h>
#include "driver/i2s.h"

//i2s_speaker_config
#define I2S_DOUT 14
#define I2S_BCLK 12
#define I2S_LRC  13
#define I2S_PORT I2S_NUM_0

//sd-card_config
#define SD_CS   5
#define SD_MOSI   23
#define SD_SCK   18
#define SD_MISO   19

#define WAV_FILENAME    "/wifi_recording_inmp441.wav"

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

void skipWavHeader(File &file) {
  uint8_t header[44];
  file.read(header, 44);
}

void playWav(const char *filename) {
  File file = SD.open(filename);
  if (!file) {
    Serial.println("❌ Failed to open WAV file!");
    return;
  }

  Serial.println("🎵 Playing: " + String(filename));
  skipWavHeader(file);

  uint8_t buffer[512];
  size_t bytesRead;

  while ((bytesRead = file.read(buffer, sizeof(buffer))) > 0) {
    size_t bytesWritten = 0;
    i2s_write(I2S_PORT, buffer, bytesRead, &bytesWritten, portMAX_DELAY);
  }

  Serial.println("✅ Playback finished.");
  file.close();
}

void setup() {
  Serial.begin(115200);
  
  Serial.print("Initializing SD card...");
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS, SPI)) {
    Serial.println(" failed!");
    Serial.println("🔍 Diagnostics:");

    Serial.print("➤ CS pin: ");
    Serial.println(SD_CS);

    Serial.println("⚠️ Possible causes:");
    Serial.println("  - Wrong CS pin");
    Serial.println("  - Incorrect wiring (MISO/MOSI/SCK)");
    Serial.println("  - Card not inserted or bad");
    Serial.println("  - Incompatible SD card (try FAT32 formatted)");
    Serial.println("  - Power issue (check 3.3V for logic-level SD modules)");

    while (true); // Halt execution
  }

  Serial.println(" done.");
  Serial.println("SD card detected.");
  Serial.print("Card type: ");
  switch (SD.cardType()) {
    case CARD_MMC:   Serial.println("MMC"); break;
    case CARD_SD:    Serial.println("SDSC"); break;
    case CARD_SDHC:  Serial.println("SDHC"); break;
    default:         Serial.println("Unknown / No card"); break;
  }

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.print("Card size: ");
  Serial.print(cardSize);
  Serial.println(" MB");

  setupI2S();

  Serial.println("Playing WAV data...");
  playWav(WAV_FILENAME);
}

void loop() {
}