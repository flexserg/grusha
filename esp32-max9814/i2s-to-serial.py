import serial
import wave
import time

# === CONFIG ===
PORT = '/dev/tty.usbserial-0001'           # Change this to your ESP32 serial port (e.g., '/dev/ttyUSB0' on Linux)
BAUD_RATE = 115200
SAMPLE_RATE = 16000     # Must match what your ESP32 is using
RECORD_SECONDS = 5      # Duration to record

OUTPUT_WAV = 'esp32_audio.wav'

# === SETUP SERIAL ===
ser = serial.Serial(PORT, BAUD_RATE, timeout=1)
time.sleep(2)  # Wait for ESP32 to reboot and start streaming

print(f"🎙️ Recording {RECORD_SECONDS} seconds of audio...")
raw_audio = bytearray()

start_time = time.time()
while time.time() - start_time < RECORD_SECONDS:
    data = ser.read(512)  # Read 512 bytes at a time
    if data:
        raw_audio.extend(data)

ser.close()
print(f"✅ Finished reading {len(raw_audio)} bytes from ESP32")

# === SAVE TO WAV ===
with wave.open(OUTPUT_WAV, 'wb') as wf:
    wf.setnchannels(1)              # Mono
    wf.setsampwidth(2)              # 16-bit samples
    wf.setframerate(SAMPLE_RATE)
    wf.writeframes(raw_audio)

print(f"💾 Audio saved as {OUTPUT_WAV}")
