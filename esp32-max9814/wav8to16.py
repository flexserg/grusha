import numpy as np
import wave
from scipy.signal import resample_poly

# Input and output file names
input_file = "wifi_recording.wav"
output_file = "wifi_recording_16k.wav"

# Open input file
with wave.open(input_file, 'rb') as wf_in:
    nchannels = wf_in.getnchannels()
    sampwidth = wf_in.getsampwidth()
    framerate = wf_in.getframerate()
    nframes = wf_in.getnframes()
    audio_data = wf_in.readframes(nframes)

# Convert byte data to numpy array
audio_array = np.frombuffer(audio_data, dtype=np.int16)

# Resample from 8kHz to 16kHz (up factor 2, down factor 1)
resampled = resample_poly(audio_array, up=2, down=1)

# Convert back to bytes
resampled_bytes = resampled.astype(np.int16).tobytes()

# Write output WAV file
with wave.open(output_file, 'wb') as wf_out:
    wf_out.setnchannels(nchannels)
    wf_out.setsampwidth(sampwidth)
    wf_out.setframerate(16000)  # Set new sample rate
    wf_out.writeframes(resampled_bytes)

print(f"Done: {output_file}")
