import serial
import wave
import time
import struct

# ---- Settings ----
PORT = '/dev/tty.usbserial-0001'              # Change to your ESP32 port
BAUDRATE = 1000000         # Match the ESP32 code
SAMPLE_RATE = 8000         # Hz
BITS_PER_SAMPLE = 16       # We're saving as 16-bit PCM
CHANNELS = 1
DURATION_SECONDS = None    # Optional: limit recording time (e.g., 10 seconds)
#RAW_FILE = "audio.raw"
WAV_FILE = "audio.wav"

# ---- Open Serial Port ----
ser = serial.Serial(PORT, BAUDRATE)
print("Recording... Press Ctrl+C to stop.")

# ---- Record ----
raw_data = bytearray()

try:
    start_time = time.time()
    while True:
        if DURATION_SECONDS and (time.time() - start_time > DURATION_SECONDS):
            break
        data = ser.read(2)  # 2 bytes per sample
        raw_data.extend(data)

except KeyboardInterrupt:
    print("\nRecording stopped.")

ser.close()

# ---- Save as .wav ----
print(f"Saving to {WAV_FILE}...")

with wave.open(WAV_FILE, 'wb') as wav_file:
    wav_file.setnchannels(CHANNELS)
    wav_file.setsampwidth(BITS_PER_SAMPLE // 8)
    wav_file.setframerate(SAMPLE_RATE)

    # Convert raw bytes to signed 16-bit PCM (assuming ESP sends unsigned 12-bit as 16-bit little-endian)
    samples = []
    for i in range(0, len(raw_data), 2):
        val = raw_data[i] | (raw_data[i + 1] << 8)
        val = val - 2048  # Remove DC offset
        val = max(min(val, 2047), -2048)  # Clamp to 12-bit range
        val <<= 4  # Scale to use full 16-bit range
        samples.append(struct.pack('<h', val))  # little-endian signed 16-bit

    wav_file.writeframes(b''.join(samples))

print("Done!")
