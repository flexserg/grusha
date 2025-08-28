from flask import Flask, render_template, request, jsonify, session
import speech_recognition as sr
import io
from pydub import AudioSegment
import tempfile
import os
from datetime import datetime

app = Flask(__name__)
app.secret_key = 'your_secret_key_here'  # Needed for session

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
        
        # Create conversation entry
        timestamp = datetime.now().strftime("%H:%M:%S")
        user_entry = f"[{timestamp}] USER: {user_text}"
        assistant_entry = f"[{timestamp}] ASSISTANT: Hello"
        
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
            "conversation": conversation
        })
    
    except Exception as e:
        return jsonify({"status": "error", "message": str(e)})

if __name__ == "__main__":
    app.run(debug=True)