import socket
import wave
import numpy as np
import signal
import sys

# === SETTINGS ===
OUTPUT_FILE = "wifi_recording_inmp441.wav"
PORT = 12345            # Should match ESP32 sender port
SAMPLE_RATE = 16000     # Hz — match ESP32 sample rate
CHANNELS = 1            # Mono
SAMPLE_WIDTH = 2        # Bytes (16-bit audio)
CHUNK_SIZE = 4096       # Bytes per read

# === BUFFER TO STORE AUDIO ===
audio_data = []
running = True

# === SOCKET SETUP ===
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)  # Use SOCK_STREAM for TCP
sock.bind(("", PORT))
sock.settimeout(1.0)  # allow graceful shutdown

print(f"Listening for audio on port {PORT}... Press Ctrl+C to stop.")


# === SIGNAL HANDLER TO STOP RECORDING ===
def signal_handler(sig, frame):
    global running
    print("\nStopping recording and saving file...")
    running = False

signal.signal(signal.SIGINT, signal_handler)

while running:
    try:
        data, addr = sock.recvfrom(CHUNK_SIZE)
        if data:
            audio_data.append(data)

    except socket.timeout:
        continue
    except Exception as e:
        print(f"Error: {e}")
        break

sock.close()

# === SAVE TO WAV FILE ===
if audio_data:
    audio_np = np.frombuffer(b''.join(audio_data), dtype=np.int32)
    audio_np = (audio_np >> 14).astype(np.int16)  # Convert 32-bit to 16-bit PCM

    with wave.open(OUTPUT_FILE, 'wb') as wf:
        wf.setnchannels(CHANNELS)
        wf.setsampwidth(SAMPLE_WIDTH)  # 2 bytes = 16 bits
        wf.setframerate(SAMPLE_RATE)
        wf.writeframes(audio_np.tobytes())

    print(f"Saved to {OUTPUT_FILE}")
    sys.exit(0)

else:
    print("No audio data received.")
