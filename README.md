# Груша
---
- [Груша](#груша)
  - [Схема подключения и параметры конфигурации](#схема-подключения-и-параметры-конфигурации)
    - [Микрофон INMP441](#микрофон-inmp441)
    - [Усилитель MAX98357A (для подключения динамика)](#усилитель-max98357a-для-подключения-динамика)
    - [Модуль SD карты памяти](#модуль-sd-карты-памяти)
    - [Кнопки](#кнопки)
    - [Светодиод](#светодиод)
  - [Что умеет](#что-умеет)
    - [Передача аудио с микрофона через интернет](#передача-аудио-с-микрофона-через-интернет)
    - [Проигрывание аудио файла WAV, полученного через интернет, из динамика](#проигрывание-аудио-файла-wav-полученного-через-интернет-из-динамика)
    - [Проигрывание аудио из файла на SD карте из динамика](#проигрывание-аудио-из-файла-на-sd-карте-из-динамика)
    - [Запись аудио с микрофона и сохранение в файл на SD карте](#запись-аудио-с-микрофона-и-сохранение-в-файл-на-sd-карте)

---
![Alt text](IMG_4153.HEIC "Grusha")
## Схема подключения и параметры конфигурации
### Микрофон INMP441
|INMP441 pin    |ESP32 pin    |
|---------------|-------------|
|L/R            |3V3          |
|WS             |D25          |
|SCK            |D32          |
|SD             |D33          |
|VDD            |3V3          |
|GND            |GND          |
```
//====CONFIG INMP441====
#define  I2S_WS  25
#define  I2S_SD  33
#define  I2S_SCK  32
#define  I2S_PORT_MIC I2S_NUM_0
```
Для инциализации необходимо указывать следующие параметры:
```
const  i2s_config_t i2s_config_mic = {
  .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
  .sample_rate = SAMPLE_RATE, //44100,
  .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
  .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
  .communication_format = I2S_COMM_FORMAT_I2S_MSB,
  .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1, // default interrupt priority
  .dma_buf_count = 4,
  .dma_buf_len = SAMPLE_BUFFER_SIZE / 4,
  .use_apll = false, //true
  .tx_desc_auto_clear = false,
  .fixed_mclk = 0
}
...
i2s_driver_install(I2S_PORT_MIC, &i2s_config_mic, 0, NULL);
...
const  i2s_pin_config_t pin_config_mic = {
  .bck_io_num = I2S_SCK,
  .ws_io_num = I2S_WS,
  .data_out_num = I2S_PIN_NO_CHANGE, //-1,
  .data_in_num = I2S_SD
};
...
i2s_set_pin(I2S_PORT_MIC, &pin_config_mic);
```
### Усилитель MAX98357A (для подключения динамика)
|MAX98357A pin  |ESP32 pin    |
|---------------|-------------|
|LRC            |D13          |
|BCLK           |D12          |
|DIN            |D32          |
|GAIN           |D33          |
|SD             |D33          |
|GND            |GND          |
|VIN            |3V3          |
```
//i2s_speaker_config
#define  I2S_DOUT  14
#define  I2S_BCLK  12
#define  I2S_LRC  13
#define  I2S_PORT_DAC I2S_NUM_1
```
Для инциализации необходимо указывать следующие параметры:
```
const  i2s_config_t i2s_config_dac = {
  .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_TX),
  .sample_rate = 16000, // Match WAV file sample rate (or 44100)
  .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
  .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT, // Mono
  .communication_format = I2S_COMM_FORMAT_I2S_MSB,
  .intr_alloc_flags = 0,
  .dma_buf_count = 8,
  .dma_buf_len = 512,
  .use_apll = false,
  .tx_desc_auto_clear = true,
  .fixed_mclk = 0
};
...
i2s_driver_install(I2S_PORT_DAC, &i2s_config_dac, 0, NULL);
...
const  i2s_pin_config_t pin_config_dac = {
  .bck_io_num = I2S_BCLK,
  .ws_io_num = I2S_LRC,
  .data_out_num = I2S_DOUT,
  .data_in_num = I2S_PIN_NO_CHANGE
};
...
i2s_set_pin(I2S_PORT_DAC, &pin_config_dac);
```
### Модуль SD карты памяти
|SD-card module pin  |ESP32 pin    |
|--------------------|-------------|
|3V3                 |3V3          |
|CS                  |D5           |
|MOSI                |D23          |
|CLK                 |D18          |
|MISO                |D19          |
|GND                 |GND          |
```
//sd-card_config
#define  SD_CS  5
#define  SD_MOSI  23
#define  SD_SCK  18
#define  SD_MISO  19
```
Красивая инциализация происходит следующим образом:
```
Serial.print("Initializing SD card...");

SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

if (!SD.begin(SD_CS, SPI)) {
  Serial.println(" failed!");
  Serial.println("🔍 Diagnostics:");
  Serial.print("➤ CS pin: ");
  Serial.println(SD_CS);
  Serial.println("⚠️ Possible causes:");
  Serial.println(" - Wrong CS pin");
  Serial.println(" - Incorrect wiring (MISO/MOSI/SCK)");
  Serial.println(" - Card not inserted or bad");
  Serial.println(" - Incompatible SD card (try FAT32 formatted)");
  Serial.println(" - Power issue (check 3.3V for logic-level SD modules)");
  while (true); // Halt execution
}

Serial.println(" done.");
Serial.println("SD card detected.");

Serial.print("Card type: ");
switch (SD.cardType()) {
  case CARD_MMC: Serial.println("MMC"); break;
  case CARD_SD: Serial.println("SDSC"); break;
  case CARD_SDHC: Serial.println("SDHC"); break;
  default: Serial.println("Unknown / No card"); break;
}

uint64_t cardSize = SD.cardSize() / (1024 * 1024);
Serial.print("Card size: ");
Serial.print(cardSize);
Serial.println(" MB");
```
### Кнопки
|Button pin          |ESP32 pin    |
|--------------------|-------------|
|1                   |GND          |
|2                   |D26          |

Внутренняя кнопка ESP32 модуля подключена к D0 и к GND.
```
//button_config
#define  BUTTON_PIN  0
#define  EXT_BUTTON_PIN  26
```
Поскольку обе кнопки подключены к GND, устанавливается режим PULLUP:
```
void setup() {
...
  pinMode(BUTTON_PIN, INPUT_PULLUP); // set ESP32 Internal Button pin to input pull-up mode
  pinMode(EXT_BUTTON_PIN, INPUT_PULLUP); // set External Button pin to output mode  
...
}
```
При такой конфигурации, когда кнопка нажата на порт поступает сигнал LOW, когда отпущена - HIGH.
### Светодиод
|LED pin.            |ESP32 pin                 |
|--------------------|--------------------------|
|1                   |GND                       |
|2                   |D22 (через резистор 220)  |

Внутренняя кнопка ESP32 модуля подключена к D0 и к GND.
```
//led_config
#define  LED_PIN  22
```
Поскольку обе кнопки подключены к GND, устанавливается режим PULLUP:
```
int led_state = LOW;
```
Значение светодиода устанавливается командой:
```
digitalWrite(LED_PIN, led_state);
```
## Что умеет
### Передача аудио с микрофона через интернет
[voice_over_wifi.ino](arduino/voice_over_wifi/voice_over_wifi.ino)

Необходимо указать следующие параметры:
```
//====WIFI conifg====
const char* ssid = "XXXX";
const char* password = "XXXX";
const char* receiverIP = "XXXX";  // Replace with listening server IP
const uint16_t receiverPort = XXXX; // Replace with listening server UDP port
```
Пример кода, сохраняющего аудио в WAV файл, на принимающей стороне: [inmp441_o_wifi.py](esp32-max9814/inmp441_o_wifi.py)

Необходимо указать следующие параметры:
```
OUTPUT_FILE = "wifi_recording_inmp441.wav"
PORT = 12345            # Should match ESP32 sender port
```
### Проигрывание аудио файла WAV, полученного через интернет, из динамика
[play_audio_over_wifi.ino](arduino/voice_over_wifi/play_audio_over_wifi/play_audio_over_wifi.ino)

Необходимо указать следующие параметры:
```
const char* ssid = "XXXX";
const char* password = "XXXX";
WiFiUDP udp;
const int localPort = XXXX; // Replace with UDP port to listen on
```
Пример кода на передающей стороне: [audio_to_wifi.py](esp32-max9814/audio_to_wifi.py)

Необходимо указать следующие параметры:
```
ESP32_IP = "192.168.31.117"  # Change to your ESP32 IP
UDP_PORT = XXXX
...
with wave.open("wifi_recording_inmp441.wav", "rb") as wf:
```
### Проигрывание аудио из файла на SD карте из динамика
[play_from_sd.ino](esp32-max9814/play_from_sd/play_from_sd.ino)

Необходимо указать следующие параметры:
```
#define WAV_FILENAME    "/wifi_recording_inmp441.wav"
```
### Запись аудио с микрофона и сохранение в файл на SD карте
[audio_to_sd_notworking.ino](arduino/audio_to_sd_notworking/audio_to_sd_notworking.ino)

> **Внимание:** Пока работает криво, т.к. не получается нормально управлять началом и окончанием записи кнопками. 

Тут же предпринята попытка сразу проиграть записанное аудио.
