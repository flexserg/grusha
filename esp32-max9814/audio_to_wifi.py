import socket
import wave
import time

ESP32_IP = "192.168.31.118"  # Change to your ESP32 IP
UDP_PORT = 12345
CHUNK_SIZE = 512

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

#wf = wave.open("test_audio.wav", "rb")
#print("Channels:", wf.getnchannels())
#print("Sample width:", wf.getsampwidth())
#print("Frame rate:", wf.getframerate())

with wave.open("wifi_recording_inmp441.wav", "rb") as wf:
    if wf.getsampwidth() != 2 or wf.getnchannels() != 1:
        raise ValueError("Audio must be 16-bit mono WAV")

    print("Sending audio...")
    data = wf.readframes(CHUNK_SIZE)

    while data:
        sock.sendto(data, (ESP32_IP, UDP_PORT))
        #time.sleep(0.005)
        time.sleep(CHUNK_SIZE / wf.getframerate())  # pacing to avoid buffer overrun
        data = wf.readframes(CHUNK_SIZE)

print("Done.")
