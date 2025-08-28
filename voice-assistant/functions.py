import re
import nltk
import json
import joblib
import random

def filter_text(text):
  text = text.lower() # lowercase
  text = text.strip() # пробелы в начале и конце
  pattern = r'[^\w\s]' # все что ни слово и не пробел
  text = re.sub(pattern, "", text) # вырезаем все, что не слово и не пробел
  return text

# True, ели тексты похожи (промпт и контрольная фраза)
# False, если не похожи
def text_match(user_text, example):
  user_text = filter_text(user_text)
  example = filter_text(example)
  #print(user_text, example)
  if user_text == example: # если полностью сопадают
    return True
  if user_text.find(example) != -1: # если фраза входит в промпт
    return True

  distance = nltk.edit_distance(user_text, example)
  # print(distance)
  # отношение кол-ва ошибок к длине слова
  ratio = distance / len(example)
  #print(ratio)
  if ratio < 0.4:
    return True

#  Взять файл и сделать с ним
with open("big_bot_config.json", "r") as config_file:
  data = json.load(config_file)

INTENTS = data["intents"]

def get_intent(user_text):
  for intent in INTENTS:
    # intent - hello, bye, how are you, ...
    examples = INTENTS[intent]["examples"]
    # для кажджого примера в интенте
    for example in examples:
      if len(filter_text(example)) < 3:
        continue
      if text_match(user_text, example):
        return intent # найденное намерение подходит к промпту
  return None # ничего не найдено

# Load artifacts
model = joblib.load('intent_model.pkl')
vectorizer = joblib.load('vectorizer.pkl')

def get_intent_ml(user_text):
  user_text = filter_text(user_text)
  vec_text = vectorizer.transform([user_text])
  probas = model.predict_proba(vec_text)[0]  # [0.1, 0.8, 0.1] etc.
  intent = model.predict(vec_text)[0]
  confidence = max(probas)  # Highest probability  
  return intent, confidence

def get_random_response(intent):
  return random.choice(INTENTS[intent]["responses"])

def bot(user_text):
  # Confidence threshold check
  CONFIDENCE_THRESHOLD = 0.4  # Adjust as needed
  
  intent_ml, confidence = get_intent_ml(user_text)
  if confidence >= CONFIDENCE_THRESHOLD:
    reply = get_random_response(intent_ml)
    return reply, 'n/a', intent_ml, confidence
  
  intent = get_intent(user_text)
  if intent:
    reply = get_random_response(intent)
  else:
     reply = 'Не знаю, что и сказать. Ха-ха.'

  return reply, intent, intent_ml, confidence