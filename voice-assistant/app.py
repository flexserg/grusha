import functions as myf
from flask import Flask, render_template, request, jsonify, session, send_file, make_response
from flask_cors import CORS
import speech_recognition as sr
import io
import tempfile
import os
from datetime import datetime
from pydub import AudioSegment
import pygame
from playsound import playsound
import time
from gtts import gTTS

#AudioSegment.converter = '/usr/bin/ffmpeg'
'''
# Explicitly set FFmpeg paths (try all common locations)
ffmpeg_paths = [
    "/usr/bin/ffmpeg",
    "/usr/local/bin/ffmpeg",
    os.getenv("FFMPEG_PATH", "")  # Fallback to environment variable
]
for path in ffmpeg_paths:
    if os.path.exists(path):
        AudioSegment.converter = path
        AudioSegment.ffmpeg = path
        AudioSegment.ffprobe = path
        break
else:
    raise RuntimeError("FFmpeg not found! Tried paths: " + ", ".join(ffmpeg_paths))
'''

app = Flask(__name__)
CORS(app)
app.secret_key = 'your_secret_key_here'  # Needed for session

######### ffmpeg problemssss
#@app.before_first_request
#def check_ffmpeg():
#    try:
#        test = AudioSegment.silent(duration=1000)  # Simple test
#        test.export("/tmp/test.mp3", format="mp3")
#        app.logger.info("FFmpeg is working correctly!")
#    except Exception as e:
#        app.logger.error(f"FFmpeg test failed: {str(e)}")
##########

#needed for http
'''
@app.route("/")
def home():
    # Initialize conversation history if it doesn't exist
    if 'conversation' not in session:
        session['conversation'] = []
    return render_template("index.html")
'''

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
        
        # Recognize speech
        r = sr.Recognizer()
        with sr.AudioFile(wav_path) as source:
            audio_data = r.record(source)
            user_text = r.recognize_google(audio_data, language="ru-RU")
        
        reply_text, intent, intent_ml, confidence = myf.bot(user_text)

        tts = gTTS(text=reply_text, lang='ru')
    
        # Create in-memory file
        mp3_fp = io.BytesIO()
        tts.write_to_fp(mp3_fp)
        mp3_fp.seek(0)

        return send_file(
         mp3_fp,
         mimetype='audio/mpeg',
         as_attachment=False
        )
    
    except Exception as e:
        return jsonify({"status": "error", "message": str(e)})

if __name__ == "__main__":
    app.run(host='127.0.0.1', port=5000, debug=True)
