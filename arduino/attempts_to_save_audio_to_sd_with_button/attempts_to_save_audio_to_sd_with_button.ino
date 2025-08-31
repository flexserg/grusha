#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <driver/i2s.h>

//====CONFIG MAX9814=====
#define MIC_PIN 35
#define SAMPLE_RATE 8000
#define BITS_PER_SAMPLE 128
#define CHANNELS        1

#define BUFFER_SIZE 256  // Number of samples per write

uint8_t audioBuffer[BUFFER_SIZE];  
int bufferIndex = 0;

/*
// --- CONFIGURATION INMP441 ---

#define I2S_SD          25
#define I2S_WS          26
#define I2S_SCK         27

#define I2S_DOUT        14
#define I2S_BCLK        12
#define I2S_LRC         13

#define CHANNELS        1
#define SAMPLE_RATE     16000
#define BITS_PER_SAMPLE I2S_BITS_PER_SAMPLE_16BIT
#define BUFFER_SIZE     1024
*/

// Add these for SD card buffering
#define SD_WRITE_SIZE 512
uint8_t writeBuffer[SD_WRITE_SIZE];
size_t bufferPos = 0;


// --- GLOBALS ---
#define BUTTON_PIN      0
bool isRecording = false;
bool lastButtonState = HIGH;
uint32_t lastDebounceTime = 0;
const uint32_t debounceDelay = 50;
File audioFile;
String filename = "";
uint32_t samplesWritten = 0;

/*
// --- I2S CONFIG ---
i2s_config_t i2sMicConfig = {
  .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
  .sample_rate = SAMPLE_RATE,
  .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,  // Keep as 32-bit
  .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
  .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
  .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
  .dma_buf_count = 8,  // Increased for better stability
  .dma_buf_len = BUFFER_SIZE,
  .use_apll = true     // Better clock accuracy
};

i2s_pin_config_t i2sMicPins = {
  .bck_io_num = I2S_SCK,
  .ws_io_num = I2S_WS,
  .data_out_num = -1,
  .data_in_num = I2S_SD
};

i2s_config_t i2sDacConfig = {
  .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
  .sample_rate = SAMPLE_RATE,
  .bits_per_sample = BITS_PER_SAMPLE,
  .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
  .communication_format = I2S_COMM_FORMAT_I2S,
  .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
  .dma_buf_count = 4,
  .dma_buf_len = 1024,
  .use_apll = false
};

i2s_pin_config_t i2sDacPins = {
  .bck_io_num = I2S_BCLK,
  .ws_io_num = I2S_LRC,
  .data_out_num = I2S_DOUT,
  .data_in_num = -1
};


void writeWavHeader(File file, uint32_t sampleRate, uint32_t totalSamples) {
  uint32_t dataSize = totalSamples * CHANNELS * 2;
  uint32_t chunkSize = dataSize + 36;
  uint16_t audioFormat = 1;
  uint16_t numChannels = CHANNELS;
  uint16_t bitsPerSample = 16;
  uint32_t byteRate = sampleRate * numChannels * bitsPerSample / 8;
  uint16_t blockAlign = numChannels * bitsPerSample / 8;

  file.seek(0);
  file.write((const uint8_t *)"RIFF", 4);
  file.write((uint8_t *)&chunkSize, 4);
  file.write((const uint8_t *)"WAVE", 4);
  file.write((const uint8_t *)"fmt ", 4);

  uint32_t subchunk1Size = 16;
  file.write((uint8_t *)&subchunk1Size, 4);
  file.write((uint8_t *)&audioFormat, 2);
  file.write((uint8_t *)&numChannels, 2);
  file.write((uint8_t *)&sampleRate, 4);
  file.write((uint8_t *)&byteRate, 4);
  file.write((uint8_t *)&blockAlign, 2);
  file.write((uint8_t *)&bitsPerSample, 2);
  file.write((const uint8_t *)"data", 4);
  file.write((uint8_t *)&dataSize, 4);
}

void recordAudioChunk() {
  int32_t rawBuffer[BUFFER_SIZE/4];  // 32-bit samples
  int16_t audioBuffer[BUFFER_SIZE/2]; // 16-bit output
  size_t bytesRead;
  
  // Read from I2S
  i2s_read(I2S_NUM_0, rawBuffer, BUFFER_SIZE, &bytesRead, portMAX_DELAY);
  
  // Convert 24-bit in 32-bit to 16-bit
  size_t samplesRead = bytesRead / 4;
  for(size_t i=0; i<samplesRead; i++) {
    // INMP441 specific conversion:
    // 1. Right shift 32->24 bit (remove padding)
    // 2. Scale down to 16-bit with proper gain
    int32_t rawSample = rawBuffer[i] >> 8;  // Get 24-bit value
    
    // Apply gain adjustment (INMP441 is quite sensitive)
    rawSample = rawSample / 64;  // Adjust this value based on your recordings
    
    // Clip to 16-bit range
    if(rawSample > 32767) rawSample = 32767;
    if(rawSample < -32768) rawSample = -32768;
    
    audioBuffer[i] = (int16_t)rawSample;
  }
  
  // Write to SD card
  audioFile.write((uint8_t*)audioBuffer, samplesRead * sizeof(int16_t));
  samplesWritten += samplesRead;
}

void startRecording() {
  // Initialize I2S
  i2s_driver_install(I2S_NUM_0, &i2sMicConfig, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &i2sMicPins);
  i2s_set_clk(I2S_NUM_0, SAMPLE_RATE, BITS_PER_SAMPLE, I2S_CHANNEL_MONO);
  
  // Allow microphone to stabilize
  delay(100);
  
  // Create file and write empty header
  filename = "/rec_" + String(millis()) + ".wav";
  audioFile = SD.open(filename, FILE_WRITE);
  for(int i=0; i<44; i++) audioFile.write(0);
  samplesWritten = 0;
}

void stopRecording() {
  // Write any remaining data in buffer
  if (bufferPos > 0) {
    audioFile.write(writeBuffer, bufferPos);
    bufferPos = 0;
  }
  
  i2s_driver_uninstall(I2S_NUM_0);
  writeWavHeader(audioFile, SAMPLE_RATE, samplesWritten);
  audioFile.flush();
  audioFile.close();
  Serial.println("💾 Recording saved as: " + filename);
}
*/

void writeWavHeader(File file, uint32_t sampleRate, uint32_t totalSamples) {
  uint32_t dataSize = totalSamples * CHANNELS * BITS_PER_SAMPLE / 8;
  uint32_t byteRate = sampleRate * CHANNELS * BITS_PER_SAMPLE / 8;
  uint32_t chunkSize = dataSize + 36;
  uint32_t subchunk1Size = 16;
  uint16_t audioFormat = 1;
  uint16_t numChannels = CHANNELS;
  uint16_t bitsPerSample = BITS_PER_SAMPLE;
  uint16_t blockAlign = CHANNELS * BITS_PER_SAMPLE / 8;;

  file.seek(0);
  file.write((const uint8_t *)"RIFF", 4);
  file.write((uint8_t *)&chunkSize, 4);
  file.write((const uint8_t *)"WAVE", 4);
  file.write((const uint8_t *)"fmt ", 4);
  file.write((uint8_t *)&subchunk1Size, 4);
  file.write((uint8_t *)&audioFormat, 2);
  file.write((uint8_t *)&numChannels, 2);
  file.write((uint8_t *)&sampleRate, 4);
  file.write((uint8_t *)&byteRate, 4);
  file.write((uint8_t *)&blockAlign, 2);
  file.write((uint8_t *)&bitsPerSample, 2);
  file.write((const uint8_t *)"data", 4);
  file.write((uint8_t *)&dataSize, 4);
}

void recordAudioChunk() {
  int sample = analogRead(MIC_PIN);  // 0-4095
  sample = (sample - 2048) << 4;     // Center to 0, scale to 16-bit

  audioBuffer[bufferIndex++] = (int16_t)sample;

  if (bufferIndex >= BUFFER_SIZE) {
    audioFile.write((uint8_t *)audioBuffer, BUFFER_SIZE * sizeof(int16_t));
    samplesWritten += BUFFER_SIZE;
    bufferIndex = 0;
  }
}

void startRecording() {
  
  // Create file and write empty header
  filename = "/rec_" + String(millis()) + ".wav";
  audioFile = SD.open(filename, FILE_WRITE);
  for(int i=0; i<44; i++) audioFile.write((uint8_t)0);
  samplesWritten = 0;
  bufferIndex = 0;
  Serial.println("🎙️ Recording started");
}

void stopRecording() {
  if (bufferIndex > 0) {
    audioFile.write((uint8_t *)audioBuffer, bufferIndex * sizeof(int16_t));
    samplesWritten += bufferIndex;
  }

  writeWavHeader(audioFile, SAMPLE_RATE, samplesWritten);
  audioFile.close();
  Serial.println("💾 Recording saved as: " + filename);
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(MIC_PIN, INPUT);

  if (!SD.begin()) {
    Serial.println("❌ SD card init failed!");
    while (true);
  }
  Serial.println("✅ SD card initialized");
}

void loop() {
  static bool buttonPressed = false;
  static unsigned long lastDebounceTime = 0;

  int buttonState = digitalRead(BUTTON_PIN);

  if (buttonState == LOW && !buttonPressed && millis() - lastDebounceTime > debounceDelay) {
    buttonPressed = true;
    lastDebounceTime = millis();

    // Toggle recording state
    isRecording = !isRecording;

    if (isRecording) {
      Serial.println("🎙️ Start recording");
      startRecording();
      delay(10); //Debounce
    } else {
      Serial.println("🛑 Stop recording");
      stopRecording();
      delay(10); //debounce
      //playLastRecording();  // optional
    }
  }

  // Reset button press flag after button is released
  if (buttonState == HIGH && buttonPressed) {
    buttonPressed = false;
    lastDebounceTime = millis();
  }

  if (isRecording) {
    recordAudioChunk();
    delayMicroseconds(1000000 / SAMPLE_RATE);  // Simulate sampling rate
  }
}