#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <FS.h>
#include <driver/i2s.h>
#include <ezButton.h>

//====CONFIG INMP441====
#define I2S_WS 25
#define I2S_SD 33
#define I2S_SCK 32
#define I2S_PORT_MIC I2S_NUM_0

//i2s_speaker_config
#define I2S_DOUT 14
#define I2S_BCLK 12
#define I2S_LRC  13
#define I2S_PORT_DAC I2S_NUM_1

//i2s_config
#define SAMPLE_RATE     16000
#define BITS_PER_SAMPLE 16
#define CHANNELS        1

//i2s_read
#define SAMPLE_BUFFER_SIZE  4096
#define BUFFER_SIZE  1024

//VAD_config
//#define VAD_THRESHOLD       300  // Adjust experimentally
//#define VAD_SILENCE_DURATION 1000  // in ms

//sd-card_config
#define SD_CS   5
#define SD_MOSI   23
#define SD_SCK   18
#define SD_MISO   19

//button_config
#define BUTTON_PIN 0
#define EXT_BUTTON_PIN 26
#define DEBOUNCE_TIME  100 // the debounce time in millisecond, increase this time if it still chatters
ezButton button_int(BUTTON_PIN); // create ezButton object that attach to pin GPIO0
ezButton button_ext(EXT_BUTTON_PIN); // create ezButton object that attach to pin GPIO26

//led_config
#define LED_PIN     22 // ESP32 pin GPIO22, which connected to led
int led_state = LOW;   // the current state of LED

bool isRecording = false;

// --- GLOBALS ---
File audioFile;
String filename = "";
uint32_t samplesWritten = 0;

esp_err_t i2s_install_mic()
{
  const i2s_config_t i2s_config_mic = {
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

  return i2s_driver_install(I2S_PORT_MIC, &i2s_config_mic, 0, NULL);
}

esp_err_t i2s_setpin_mic()
{
  const i2s_pin_config_t pin_config_mic = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE, //-1,
    .data_in_num = I2S_SD
    };

  return i2s_set_pin(I2S_PORT_MIC, &pin_config_mic);
}

void setupI2S_dac() {
  const i2s_config_t i2s_config_dac = {
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

  const i2s_pin_config_t pin_config_dac = {
    .bck_io_num = I2S_BCLK,
    .ws_io_num = I2S_LRC,
    .data_out_num = I2S_DOUT,
    .data_in_num = I2S_PIN_NO_CHANGE
  };

  i2s_driver_install(I2S_PORT_DAC, &i2s_config_dac, 0, NULL);
  i2s_set_pin(I2S_PORT_DAC, &pin_config_dac);
}

void writeWavHeader(File file, uint32_t sampleRate, uint32_t totalSamples) { 
  uint16_t audioFormat = 1;
  uint16_t numChannels = CHANNELS;
  uint16_t bitsPerSample = 16;

  uint32_t byteRate = sampleRate * numChannels * (bitsPerSample / 8);
  uint16_t blockAlign = numChannels * (bitsPerSample / 8);

  uint32_t dataSize = totalSamples * numChannels * (bitsPerSample / 8); 
  uint32_t chunkSize = dataSize + 36;
  
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

void startRecording() {
  filename = "/rec-" + String(millis()) + ".wav";
  audioFile = SD.open(filename.c_str(), FILE_WRITE);
  if (!audioFile) {
    Serial.println("❌ Failed to open file for writing");
    return;
  }
  Serial.println("🎙️ Recording to: " + filename);
  for (int i = 0; i < 44; i++) audioFile.write((uint8_t)0); // reserve WAV header
  samplesWritten = 0;
  //i2s_driver_install(I2S_NUM_0, &i2sMicConfig, 0, NULL);
  i2s_install_mic();
  //i2s_set_pin(I2S_NUM_0, &i2sMicPins);
  i2s_setpin_mic();
  //i2s_zero_dma_buffer(I2S_NUM_0);
}

void stopRecording() {
  i2s_driver_uninstall(I2S_PORT_MIC);
  writeWavHeader(audioFile, SAMPLE_RATE, samplesWritten);
  audioFile.close();
  Serial.println("💾 Recording saved as: " + filename);
}

void recordAudioChunk() {
  int32_t buffer32[SAMPLE_BUFFER_SIZE / 4];  // raw 32-bit mic samples
  int16_t buffer16[SAMPLE_BUFFER_SIZE / 4];  // converted 16-bit samples
  size_t bytesRead;

  // Read from mic
  i2s_read(I2S_PORT_MIC, (char*)buffer32, sizeof(buffer32), &bytesRead, portMAX_DELAY);

  // Convert to 16-bit PCM
  int samplesRead = bytesRead / 4;  
  for (int i = 0; i < samplesRead; i++) {
    buffer16[i] = buffer32[i] >> 14;  // downscale 32→16
  }

  // Write entire block at once
  audioFile.write((uint8_t*)buffer16, samplesRead * 2);

  // Update sample count
  samplesWritten += samplesRead;
}

/*
void recordAudioChunk() {
  int32_t buffer32[SAMPLE_BUFFER_SIZE / 4];  // 32-bit samples
  size_t bytesRead;

  i2s_read(I2S_PORT_MIC, (char*)buffer32, sizeof(buffer32), &bytesRead, portMAX_DELAY);

  // Convert to 16-bit PCM
  int samplesRead = bytesRead / 4;  // 4 bytes per 32-bit sample
  for (int i = 0; i < samplesRead; i++) {
    int16_t sample16 = buffer32[i] >> 14;  // scale down 32→16 bits
    audioFile.write((uint8_t*)&sample16, 2);
  }

  // Count how many 16-bit samples were written
  samplesWritten += samplesRead;
}
*/

void playLastRecording() {
  File file = SD.open(filename.c_str());
  if (!file) {
    Serial.println("❌ Couldn't open file for playback");
    return;
  }

  //i2s_driver_install(I2S_NUM_1, &i2sDacConfig, 0, NULL);
  //i2s_set_pin(I2S_NUM_1, &i2sDacPins);
  setupI2S_dac();
  //i2s_zero_dma_buffer(I2S_NUM_1); 
  file.seek(44); // skip WAV header

  uint8_t buffer[BUFFER_SIZE];
  size_t bytesRead;
  while ((bytesRead = file.read(buffer, BUFFER_SIZE)) > 0) {
    size_t bytesWritten;
    i2s_write(I2S_PORT_DAC, buffer, bytesRead, &bytesWritten, portMAX_DELAY);
  }

  file.close();

  i2s_driver_uninstall(I2S_NUM_1);
  Serial.println("🔊 Playback finished");
} 

void setup() {
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);   // set ESP32 pin to output mode
  button_int.setDebounceTime(DEBOUNCE_TIME); // set debounce time to 50 milliseconds
  button_ext.setDebounceTime(DEBOUNCE_TIME); // set debounce time to 50 milliseconds

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

} 

void loop() {
//  button_int.loop(); // MUST call the loop() function first
  button_ext.loop(); // MUST call the loop() function first
/*
  if (button_int.isPressed())
    Serial.println("The button INT is pressed");

  if (button_int.isReleased())
    Serial.println("The button INT is released");
*/
  if (button_ext.isPressed()) {
    Serial.println("The button EXT is pressed");
    // toggle state of LED
    led_state = !led_state;

    // control LED arccoding to the toggleed sate
    digitalWrite(LED_PIN, led_state);
    /*
    if (!isRecording) {
      Serial.println("🎙️ Start recording");
      startRecording();
      isRecording = true;
    } else {
      Serial.println("🛑 Stop recording");
      stopRecording();
      isRecording = false;
      // playLastRecording(); // optional
    }
    */
  }
    
  if (button_ext.isReleased())
    Serial.println("The button EXT is released");

  // Only record audio while recording is active
  if (isRecording) {
    recordAudioChunk();
  }
}

/*

void loop() {
  static int lastButtonState = HIGH;  // previous stable state
  int buttonState = digitalRead(BUTTON_PIN);
  unsigned long now = millis();

  // Debounce: only act if state changed and stable
  if (buttonState != lastButtonState && (now - lastDebounceTime > debounceDelay)) {
    lastDebounceTime = now;

    // Detect falling edge (HIGH -> LOW = press)
    if (buttonState == LOW) {
      if (!isRecording) {
        Serial.println("🎙️ Start recording");
        startRecording();
        isRecording = true;
      } else {
        Serial.println("🛑 Stop recording");
        stopRecording();
        isRecording = false;
        // playLastRecording(); // optional
      }
    }
  }

  lastButtonState = buttonState;

  // Only record audio while recording is active
  if (isRecording) {
    recordAudioChunk();
  }
}
*/

/*
void loop() {
  static bool buttonPressed = false;

  int buttonState = digitalRead(BUTTON_PIN);
  unsigned long now = millis();

  uint32_t lastDebounceTime = 0;

  // Detect falling edge (button press, INPUT_PULLUP so LOW = pressed)
  if (buttonState == LOW && !buttonPressed && (now - lastDebounceTime > debounceDelay)) {
    buttonPressed = true;
    lastDebounceTime = now;

    if (!isRecording) {
      Serial.println("🎙️ Start recording");
      //startRecording();
      isRecording = true;
    } else {
      Serial.println("🛑 Stop recording");
      //stopRecording();
      isRecording = false;
      // playLastRecording();  // optional
    }
    Serial.println(buttonPressed);
    Serial.println(buttonState);
    Serial.println(now - lastDebounceTime);
  }

  // Reset press state on release
  if (buttonState == HIGH && buttonPressed && (now - lastDebounceTime > debounceDelay)) {
    buttonPressed = false;
    lastDebounceTime = now;
  }

  // Only record audio while recording is active
  if (isRecording) {
    //recordAudioChunk();
  }
}


void loop() {
  static bool buttonPressed = false;
  static unsigned long lastDebounceTime = 0;

  int buttonState = digitalRead(BUTTON_PIN);
  unsigned long now = millis();

  // Handle button press (LOW = pressed with INPUT_PULLUP)
  if (buttonState == LOW && !buttonPressed && (now - lastDebounceTime > debounceDelay)) {
    buttonPressed = true;
    lastDebounceTime = now;

    isRecording = !isRecording;

    if (isRecording) {
      Serial.println("🎙️ Start recording");
      startRecording();
    } else {
      Serial.println("🛑 Stop recording");
      stopRecording();
      //playLastRecording();  // optional
    }
    
    Serial.println(buttonPressed);
    Serial.println(buttonState);
    Serial.println(now - lastDebounceTime);
  }

  // Handle button release
  if (buttonState == HIGH && buttonPressed && (now - lastDebounceTime > debounceDelay)) {
    buttonPressed = false;
    lastDebounceTime = now;
  }

  // Only record audio **after** button state handled
  if (isRecording) {
    recordAudioChunk();
  }
}

*/

//old
/*
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

  Serial.println("Setup I2S - Done!");
  Serial.println("Ready. Speak to record.");
}

void loop() {
  static uint8_t buffer[SAMPLE_BUFFER_SIZE];
  static uint32_t bytesWritten = 0;

  size_t bytesRead;
  i2s_read(I2S_NUM_0, buffer, SAMPLE_BUFFER_SIZE, &bytesRead, portMAX_DELAY);

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
*/