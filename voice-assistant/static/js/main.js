        const startButton = document.getElementById('startButton');
        const statusDiv = document.getElementById('status');
        const conversationDiv = document.getElementById('conversation');
        const soundLevelBar = document.getElementById('soundLevelBar');
        const soundLevelText = document.getElementById('soundLevelText');
        
        let mediaRecorder;
        let audioChunks = [];
        let audioContext;
        let analyser;
        let microphone;
        let silenceStartTime;
        let animationId;
        
        const SILENCE_THRESHOLD = 16; // dB level considered "silence"
        const SILENCE_TIMEOUT = 2000; // 2 seconds of silence to stop
        //const MAX_DB = -20; // Maximum expected dB level for visualization
        //const MIN_DB = -80; // Minimum expected dB level for visualization
        const MIN_DB = 10;    // Minimum expected dB level (quiet room)
        const MAX_DB = 40;   // Maximum expected dB level (loud speech)
        const CLIP_DB = 120;  // Absolute maximum before clipping

        startButton.addEventListener('click', toggleRecording);

        async function toggleRecording() {
            if (mediaRecorder && mediaRecorder.state === 'recording') {
                stopRecording();
            } else {
                await startRecording();
            }
        }

        async function startRecording() {
            statusDiv.textContent = "Listening... Speak now";
            startButton.textContent = "Stop Listening";
            startButton.style.backgroundColor = "#f44336";
            
            try {
                const stream = await navigator.mediaDevices.getUserMedia({ audio: true });
                audioContext = new (window.AudioContext || window.webkitAudioContext)();
                analyser = audioContext.createAnalyser();
                analyser.fftSize = 2048;
                microphone = audioContext.createMediaStreamSource(stream);
                microphone.connect(analyser);
                
                mediaRecorder = new MediaRecorder(stream);
                audioChunks = [];
                silenceStartTime = null;
                
                mediaRecorder.start();
                
                // Start analyzing sound level
                updateSoundLevel();
                
                mediaRecorder.ondataavailable = event => {
                    audioChunks.push(event.data);
                };
                
/*                // Maximum recording time
                setTimeout(() => {
                    if (mediaRecorder.state === 'recording') {
                        stopRecording();
                    }
                }, 10000);
*/                
            } catch (err) {
                statusDiv.textContent = "Error: " + err.message;
                resetUI();
            }
        }

        async function playServerSpeech(text) {
            const response = await fetch('/generate_speech', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ text })
            });

            //const audio = new Audio();
            //audio.src = URL.createObjectURL(await response.blob());
            //audio.play();
        }

        function updateSoundLevel() {
            if (!analyser) return;
            
            const dataArray = new Uint8Array(analyser.fftSize);
            analyser.getByteTimeDomainData(dataArray);
            
            // Calculate RMS (root mean square) for volume
            let sum = 0;
            for (const amplitude of dataArray) {
                const normalized = (amplitude - 128) / 128; // Convert to -1 to 1 range
                sum += normalized * normalized;
            }
            const rms = Math.sqrt(sum / dataArray.length);
            
            // Convert to dB scale (more accurate for real-world levels)
            let dB;
            if (rms > 0) {
                dB = 20 * Math.log10(rms);
                // Adjust to match your microphone's baseline
                dB += 60; // This compensates for typical mic levels
                dB = Math.max(MIN_DB, Math.min(CLIP_DB, dB)); // Clamp to range
            } else {
                dB = MIN_DB;
            }
            
            // Update display with proper scaling
            const displayLevel = Math.min(100, Math.max(0, 
                ((dB - MIN_DB) / (MAX_DB - MIN_DB)) * 100
            ));
            
            soundLevelBar.style.width = `${displayLevel}%`;
            soundLevelText.textContent = `Mic level: ${dB.toFixed(1)} dB (${displayLevel.toFixed(0)}%)`;
            
            // Check for silence
            if (dB < SILENCE_THRESHOLD) {
                if (!silenceStartTime) {
                    silenceStartTime = Date.now();
                } else if (Date.now() - silenceStartTime > SILENCE_TIMEOUT) {
                    stopRecording();
                    return;
                }
            } else {
                silenceStartTime = null;
            }

            // Continue animation if still recording
            if (mediaRecorder?.state === 'recording') {
                requestAnimationFrame(updateSoundLevel);
            }
        }

/*        function updateSoundLevel() {
            if (!analyser) {
                console.error("Analyser not initialized");
                return;
            }

            const dataArray = new Uint8Array(analyser.fftSize);
            analyser.getByteTimeDomainData(dataArray);
            console.log("Raw audio data:", dataArray.slice(0, 10)); // Log first 10 samples
            
            // Calculate RMS (root mean square) for volume
            let sum = 0;
            for (const amplitude of dataArray) {
                sum += amplitude * amplitude;
            }
            const rms = Math.sqrt(sum / dataArray.length);
            const dB = 20 * Math.log10(rms / 255);
            
            // Update sound level display
            const normalizedLevel = Math.min(1, Math.max(0, (dB - MIN_DB) / (MAX_DB - MIN_DB)));
            soundLevelBar.style.width = `${normalizedLevel * 100}%`;
            soundLevelText.textContent = `Microphone level: ${dB.toFixed(1)} dB`;
            
            // Check for silence
            if (dB < SILENCE_THRESHOLD) {
                if (!silenceStartTime) {
                    silenceStartTime = Date.now();
                } else if (Date.now() - silenceStartTime > SILENCE_TIMEOUT) {
                    stopRecording();
                    return;
                }
            } else {
                silenceStartTime = null;
            }
            
            // Continue animation if still recording
            if (mediaRecorder && mediaRecorder.state === 'recording') {
                animationId = requestAnimationFrame(updateSoundLevel);
            }
        }
*/
        function stopRecording() {
            if (mediaRecorder && mediaRecorder.state !== 'inactive') {
                cancelAnimationFrame(animationId);
                mediaRecorder.stop();
                
                // Disconnect audio nodes
                if (microphone) microphone.disconnect();
                if (analyser) analyser.disconnect();
                if (audioContext) audioContext.close();
                
                mediaRecorder.onstop = () => {
                    const audioBlob = new Blob(audioChunks, { type: 'audio/webm' });
                    sendAudioToServer(audioBlob);
                    
                    // Stop all tracks
                    mediaRecorder.stream.getTracks().forEach(track => track.stop());
                };
            }
            
            resetUI();
        }

        function resetUI() {
            startButton.textContent = "Start Listening";
            startButton.style.backgroundColor = "#4CAF50";
            soundLevelBar.style.width = "0%";
            soundLevelText.textContent = "Microphone level: 0 dB";
        }

        function sendAudioToServer(audioBlob) {
            statusDiv.textContent = "Processing...";

            const formData = new FormData();
            formData.append('audio_data', audioBlob, 'recording.webm');

            fetch('/process_audio', {
                method: 'POST',
                body: formData
            })
            .then(response => response.json())
            .then(data => {
                if (data.status === 'success') {
                    updateConversation(data.conversation);
                    statusDiv.textContent = "Ready";
                    playServerSpeech(data.reply_text);
                } else {
                    statusDiv.textContent = "Error: " + data.message;
                }             
            })
            .catch(error => {
                statusDiv.textContent = "Error: " + error;
            });
        }
        
        function updateConversation(conversation) {
            const conversationDiv = document.getElementById('conversation');
            conversationDiv.innerHTML = '';
            
            conversation.forEach(entry => {
                const messageDiv = document.createElement('div');
                
                if (entry.includes('USER:')) {
                    messageDiv.className = 'user-message';
                    messageDiv.textContent = entry.replace('USER:', 'You:');
                } else {
                    messageDiv.className = 'assistant-message';
                    
                    // Extract both intents and confidence
                    const intentMatch = entry.match(/\(Intent: (.+?), Intent_ml: (.+?), Confidence: (.+?)%\)/);
                    const baseMessage = entry.split(' (Intent:')[0];
                    
                    messageDiv.innerHTML = `
                        <div class="assistant-text">${baseMessage.replace('ASSISTANT:', 'Assistant:')}</div>
                        ${intentMatch ? `
                        <div class="intent-line">
                            <span class="intent-pair">
                                <span class="intent-tag">${intentMatch[1]}</span>
                                <span class="intent-tag">${intentMatch[2]}</span>
                            </span>
                            <span class="confidence-pill">${intentMatch[3]}%</span>
                        </div>
                        ` : ''}
                    `;
                }
                
                conversationDiv.appendChild(messageDiv);
            });
        }

/*        function updateConversation(conversation) {
            // Clear existing messages
            conversationDiv.innerHTML = '';
            
            // Add messages in reverse order (newest first)
            conversation.forEach(entry => {
                const messageDiv = document.createElement('div');
                if (entry.includes('USER:')) {
                    messageDiv.className = 'user-message';
                } else {
                    messageDiv.className = 'assistant-message';
                }
                messageDiv.textContent = entry;
                conversationDiv.append(messageDiv); // Use prepend instead of appendChild
            });
            
            // Scroll to top to see latest message
            conversationDiv.scrollTop = 0;
        }


        */

