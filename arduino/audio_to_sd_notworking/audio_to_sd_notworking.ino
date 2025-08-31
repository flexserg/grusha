#include <SD.h>
#include <FS.h>
#include <driver/i2s.h>

//====CONFIG INMP441====
#define I2S_WS 26
#define I2S_SD 25
#define I2S_SCK 27
#define I2S_PORT I2S_NUM_0

#define SAMPLE_RATE     16000
#define BITS_PER_SAMPLE 16
#define CHANNELS        1

#define BUFFER_SIZE         1024
#define VAD_THRESHOLD       300  // Adjust experimentally
#define VAD_SILENCE_DURATION 1000  // in ms

#define SD_CS   33
#define SD_SCK   35
#define SD_MISO   34
#define SD_MOSI   32

File audioFile;
bool recording = false;
unsigned long lastVoiceTime = 0;

void setupI2S() {
  const i2s_config_t i2s_config = {
    .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = i2s_bits_per_sample_t(BITS_PER_SAMPLE),
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 1024
  };

  const i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD
  };

  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);
  i2s_zero_dma_buffer(I2S_NUM_0);
}

void writeWavHeader(File file, uint32_t dataSize) {
  uint32_t fileSize = dataSize + 36;
  uint32_t byteRate = SAMPLE_RATE * CHANNELS * (BITS_PER_SAMPLE / 8);
  uint16_t blockAlign = CHANNELS * (BITS_PER_SAMPLE / 8);
  uint32_t subchunk1Size = 16;
  uint16_t audioFormat = 1;
  uint16_t channels = CHANNELS;
  uint16_t bitsPerSample = BITS_PER_SAMPLE;
  uint32_t sampleRate = SAMPLE_RATE;

  file.seek(0);
  file.write((const uint8_t *)"RIFF", 4);
  file.write((uint8_t *)&fileSize, 4);
  file.write((const uint8_t *)"WAVE", 4);
  file.write((const uint8_t *)"fmt ", 4);
  file.write((uint8_t *)&subchunk1Size, 4);
  file.write((uint8_t *)&audioFormat, 2);
  file.write((uint8_t *)&channels, 2);
  file.write((uint8_t *)&sampleRate, 4);
  file.write((uint8_t *)&byteRate, 4);
  file.write((uint8_t *)&blockAlign, 2);
  file.write((uint8_t *)&bitsPerSample, 2);

  file.write((const uint8_t *)"data", 4);
  file.write((uint8_t *)&dataSize, 4);
}

uint16_t abs16(int16_t v) {
  return v < 0 ? -v : v;
}

bool detectSpeech(uint8_t* buffer, size_t bytesRead) {
  int16_t* samples = (int16_t*)buffer;
  size_t sampleCount = bytesRead / 2;

  uint32_t sum = 0;
  for (size_t i = 0; i < sampleCount; i++) {
    sum += abs16(samples[i]);
  }

  uint16_t avg = sum / sampleCount;
  return avg > VAD_THRESHOLD;
}

void startRecording() {
  static int fileCounter = 0;
  String filename = "/rec" + String(fileCounter++) + ".wav";
  audioFile = SD.open(filename.c_str(), FILE_WRITE);
  if (!audioFile) {
    Serial.println("Failed to create file.");
    return;
  }
  Serial.println("Recording started: " + filename);
  recording = true;
  audioFile.write(new uint8_t[44], 44); // placeholder for WAV header
}

void stopRecording(uint32_t dataSize) {
  writeWavHeader(audioFile, dataSize);
  audioFile.close();
  recording = false;
  Serial.println("Recording stopped.");
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
  Serial.println("Ready. Speak to record.");
}

void loop() {
  static uint8_t buffer[BUFFER_SIZE];
  static uint32_t bytesWritten = 0;

  size_t bytesRead;
  i2s_read(I2S_NUM_0, buffer, BUFFER_SIZE, &bytesRead, portMAX_DELAY);

  bool speech = detectSpeech(buffer, bytesRead);

  if (speech) {
    lastVoiceTime = millis();
    if (!recording) {
      startRecording();
      bytesWritten = 0;
    }
  }

  if (recording) {
    audioFile.write(buffer, bytesRead);
    bytesWritten += bytesRead;

    if ((millis() - lastVoiceTime) > VAD_SILENCE_DURATION) {
      stopRecording(bytesWritten);
    }
  }
}
