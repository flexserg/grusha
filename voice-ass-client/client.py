import sounddevice as sd
import numpy as np
import requests
import io
import queue
import threading
import time
import tkinter as tk
from tkinter import ttk
from pydub import AudioSegment
from pydub.playback import play
import webrtcvad
import noisereduce as nr

# Configuration
SAMPLE_RATE = 16000
CHUNK_SIZE = 480  # 30ms chunks for VAD
BUFFER_SECONDS = 5  # Buffer for speech before/after detection
API_URL = "http://127.0.0.1:5000/process_audio"
FORMAT = np.int16
CHANNELS = 1

class VoiceAssistantClient:
    def __init__(self, root):
        self.root = root
        self.root.title("Voice Assistant Client")
        
        # Audio processing
        self.vad = webrtcvad.Vad(2)  # Aggressiveness level 0-3
        self.audio_buffer = queue.Queue()
        self.is_recording = False
        self.speech_detected = False
        self.silence_frames = 0
        self.speech_frames = []
        self.silence_threshold = 15  # Number of silent chunks to end speech
        
        # GUI Setup
        self.setup_gui()
        
        # Start audio monitoring thread
        self.monitor_thread = threading.Thread(target=self.monitor_microphone, daemon=True)
        self.monitor_thread.start()
        
        # Status variables
        self.status = "Ready"
        self.last_response = ""
    
    def setup_gui(self):
        # Status Frame
        status_frame = ttk.LabelFrame(self.root, text="Status", padding=10)
        status_frame.pack(fill=tk.X, padx=10, pady=5)
        
        self.status_label = ttk.Label(status_frame, text="Status: Ready")
        self.status_label.pack(anchor=tk.W)
        
        self.mic_status = ttk.Label(status_frame, text="Microphone: Active")
        self.mic_status.pack(anchor=tk.W)
        
        self.processing_status = ttk.Label(status_frame, text="Processing: Idle")
        self.processing_status.pack(anchor=tk.W)
        
        # Response Frame
        response_frame = ttk.LabelFrame(self.root, text="Last Response", padding=10)
        response_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=5)
        
        self.response_text = tk.Text(response_frame, height=10, wrap=tk.WORD)
        self.response_text.pack(fill=tk.BOTH, expand=True)
        self.response_text.insert(tk.END, "No responses yet...")
        self.response_text.config(state=tk.DISABLED)
        
        # Control Frame
        control_frame = ttk.Frame(self.root)
        control_frame.pack(fill=tk.X, padx=10, pady=5)
        
        self.action_button = ttk.Button(control_frame, text="Trigger Action", command=self.trigger_action)
        self.action_button.pack(side=tk.RIGHT)
        
        # Visual Indicator
        self.indicator = tk.Canvas(self.root, width=30, height=30, bg="gray")
        self.indicator.pack(pady=10)
    
    def update_status(self, status):
        self.status = status
        self.status_label.config(text=f"Status: {status}")
        self.root.update()
    
    def update_mic_status(self, status):
        self.mic_status.config(text=f"Microphone: {status}")
        self.root.update()
    
    def update_processing_status(self, status):
        self.processing_status.config(text=f"Processing: {status}")
        self.root.update()
    
    def update_response(self, text):
        self.last_response = text
        self.response_text.config(state=tk.NORMAL)
        self.response_text.delete(1.0, tk.END)
        self.response_text.insert(tk.END, text)
        self.response_text.config(state=tk.DISABLED)
        self.root.update()
    
    def update_indicator(self, color):
        self.indicator.delete("all")
        self.indicator.create_oval(5, 5, 25, 25, fill=color, outline="")
        self.root.update()
    
    def monitor_microphone(self):
        def callback(indata, frames, time, status):
            if status:
                print(status)
            self.audio_buffer.put(indata.copy())
        
        with sd.InputStream(samplerate=SAMPLE_RATE, channels=CHANNELS, 
                           dtype=FORMAT, callback=callback, blocksize=CHUNK_SIZE):
            self.update_status("Listening...")
            self.update_indicator("green")
            
            speech_buffer = []
            silence_counter = 0
            recording = False
            
            while True:
                try:
                    data = self.audio_buffer.get_nowait()
                    
                    # Convert to bytes for VAD
                    audio_bytes = data.tobytes()
                    
                    # Voice Activity Detection
                    is_speech = self.vad.is_speech(audio_bytes, SAMPLE_RATE)
                    
                    if is_speech:
                        silence_counter = 0
                        if not recording:
                            recording = True
                            self.update_status("Speech detected")
                            self.update_indicator("red")
                            speech_buffer = []  # Clear buffer
                        
                        speech_buffer.append(data)
                    else:
                        if recording:
                            silence_counter += 1
                            speech_buffer.append(data)  # Keep adding silence to buffer
                            
                            if silence_counter >= self.silence_threshold:
                                # End of speech detected
                                self.process_speech(speech_buffer)
                                recording = False
                                silence_counter = 0
                                speech_buffer = []
                                self.update_status("Listening...")
                                self.update_indicator("green")
                    
                except queue.Empty:
                    time.sleep(0.01)
    
    def process_speech(self, audio_chunks):
        self.update_processing_status("Processing speech")
        
        try:
            # Convert chunks to single numpy array
            audio_data = np.concatenate(audio_chunks)
            
            # Noise reduction
            audio_data = nr.reduce_noise(y=audio_data, sr=SAMPLE_RATE)
            
            # Convert to bytes for API
            audio_segment = AudioSegment(
                audio_data.tobytes(),
                frame_rate=SAMPLE_RATE,
                sample_width=audio_data.dtype.itemsize,
                channels=CHANNELS
            )
            
            # Export as WAV in memory
            wav_buffer = io.BytesIO()
            audio_segment.export(wav_buffer, format="wav")
            wav_data = wav_buffer.getvalue()
            
            # Send to API
            self.update_processing_status("Sending to server")
            files = {'audio': ('speech.wav', wav_data, 'audio/wav')}
            response = requests.post(API_URL, files=files)
            
            if response.status_code == 200:
                result = response.json()
                self.update_response(result.get("response", "No response text"))
                
                # Play audio response if available
                if 'audio_response' in result and result['audio_response']:
                    audio_response = io.BytesIO(result['audio_response'])
                    audio = AudioSegment.from_file(audio_response)
                    threading.Thread(target=play, args=(audio,), daemon=True).start()
            
            self.update_processing_status("Idle")
            
        except Exception as e:
            self.update_response(f"Error: {str(e)}")
            self.update_processing_status("Error")
            self.update_indicator("yellow")
    
    def trigger_action(self):
        # Placeholder for your custom action
        self.update_response("Action button pressed - implement your custom action here")
        # You can add your custom code here
    
    def run(self):
        self.root.mainloop()

if __name__ == "__main__":
    root = tk.Tk()
    client = VoiceAssistantClient(root)
    client.run()