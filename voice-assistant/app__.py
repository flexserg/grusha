import functions as myf
from flask import Flask, render_template, request, jsonify, session, send_file, make_response
import speech_recognition as sr
import io
import tempfile
import os
from datetime import datetime
from pydub import AudioSegment
from gtts import gTTS
import pygame
from playsound import playsound
import time

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

@app.route("/")
def home():
    # Initialize conversation history if it doesn't exist
    if 'conversation' not in session:
        session['conversation'] = []
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
        
        # Recognize speech
        r = sr.Recognizer()
        with sr.AudioFile(wav_path) as source:
            audio_data = r.record(source)
            user_text = r.recognize_google(audio_data, language="ru-RU")
        
        reply_text, intent, intent_ml, confidence = myf.bot(user_text)

        # Create conversation entry
        timestamp = datetime.now().strftime("%H:%M:%S")
        user_entry = f"[{timestamp}] USER: {user_text}"
        assistant_entry = f"[{timestamp}] ASSISTANT: {reply_text} (Intent: {intent}, Intent_ml: {intent_ml}, Confidence: {confidence*100:.1f}%)"
        
        # Update conversation history in session
        conversation = session.get('conversation', [])
        conversation.insert(0, assistant_entry)
        conversation.insert(0, user_entry)
        session['conversation'] = conversation
        
        # Clean up temporary files
        os.unlink(temp_path)
        os.unlink(wav_path)
        
        return jsonify({
            "status": "success",
            "user_text": user_text,
            "reply_text": reply_text,
            "conversation": conversation
        })
    
    except Exception as e:
        return jsonify({"status": "error", "message": str(e)})

@app.route('/generate_speech', methods=['POST'])
def generate_speech():
    text = request.json.get('text', '')
    tts = gTTS(text=text, lang='ru')
    
    # Create in-memory file
    mp3_fp = io.BytesIO()
    tts.write_to_fp(mp3_fp)
    mp3_fp.seek(0)
    
    pygame.mixer.init()
    pygame.mixer.music.load(mp3_fp)
    pygame.mixer.music.play()
    # Wait for playback to finish
    while pygame.mixer.music.get_busy():
      time.sleep(0.1)
    pygame.mixer.quit()

    return ('', 204)

    #filename = 'assistant.mp3'
    #filepath = os.path.join('recordings', filename)
    #tts.save(filepath)

    #return send_file(
    #    mp3_fp,
    #    mimetype='audio/mpeg',
    #    as_attachment=False
    #)

if __name__ == "__main__":
    app.run(debug=True)
