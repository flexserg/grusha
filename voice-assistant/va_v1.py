from flask import Flask, render_template, request, jsonify
import speech_recognition as sr
import io
from pydub import AudioSegment
import tempfile
import os
import datetime

app = Flask(__name__)

@app.route("/")
def home():
    return render_template("index.html")

@app.route("/process_audio", methods=["POST"])
def process_audio():
    try:
        # Get audio file from request
        audio_file = request.files["audio_data"]
        
        # Save to temporary file
        with tempfile.NamedTemporaryFile(suffix=".webm", delete=False) as tmp:
            audio_file.save(tmp.name)
            temp_path = tmp.name
        
        # Convert to WAV format using pydub
        audio = AudioSegment.from_file(temp_path)
        wav_path = temp_path.replace(".webm", ".wav")
        audio.export(wav_path, format="wav")

        # Save file
        save_path = f"recordings/{datetime.datetime.now().strftime('%Y%m%d_%H%M%S')}.wav"
        os.makedirs("recordings", exist_ok=True)
        audio.export(save_path, format="wav")
        
        # Recognize speech
        r = sr.Recognizer()
        with sr.AudioFile(wav_path) as source:
            audio_data = r.record(source)
            text = r.recognize_google(audio_data, language="ru-RU")

        # Clean up temporary files
        os.unlink(temp_path)
        os.unlink(wav_path)
        
        return jsonify({"status": "success", "response": "Hello"})
    
    except Exception as e:
        return jsonify({"status": "error", "message": str(e)})

if __name__ == "__main__":
    app.run(debug=True)