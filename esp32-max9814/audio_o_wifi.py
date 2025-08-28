import socket
import wave
import struct
import signal
#import sys

# === SETTINGS ===
OUTPUT_FILE = "wifi_recording.wav"
PORT = 12345            # Should match ESP32 sender port
SAMPLE_RATE = 8000      # Hz — match ESP32 sample rate
CHANNELS = 1            # Mono
SAMPLE_WIDTH = 2        # Bytes (16-bit audio)
CHUNK_SIZE = 1024       # Bytes per read

# === BUFFER TO STORE AUDIO ===
audio_buffer = bytearray()
running = True

# === SIGNAL HANDLER TO STOP RECORDING ===
def signal_handler(sig, frame):
    global running
    print("\nStopping recording and saving file...")
    running = False

signal.signal(signal.SIGINT, signal_handler)

# === SOCKET SETUP ===
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)  # Use SOCK_STREAM for TCP
sock.bind(("", PORT))
sock.settimeout(1.0)  # allow graceful shutdown

print(f"Listening for audio on port {PORT}... Press Ctrl+C to stop.")

while running:
    try:
        data, addr = sock.recvfrom(CHUNK_SIZE)
        if data:
            # Convert from unsigned 12-bit ESP32 ADC to signed 16-bit PCM
            for i in range(0, len(data), 2):
                val = data[i] | (data[i + 1] << 8)
                val = val - 2048  # remove 12-bit DC offset
                val <<= 4         # scale to 16-bit signed
                audio_buffer.extend(struct.pack('<h', val))
    except socket.timeout:
        continue
    except Exception as e:
        print(f"Error: {e}")
        break

sock.close()

# === SAVE TO WAV FILE ===
if audio_buffer:
    with wave.open(OUTPUT_FILE, 'wb') as wf:
        wf.setnchannels(CHANNELS)
        wf.setsampwidth(SAMPLE_WIDTH)
        wf.setframerate(SAMPLE_RATE)
        wf.writeframes(audio_buffer)
    print(f"Saved {len(audio_buffer) // SAMPLE_WIDTH} samples to {OUTPUT_FILE}")
else:
    print("No audio data received.")
