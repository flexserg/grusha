import json
from sklearn.feature_extraction.text import CountVectorizer
from sklearn.ensemble import RandomForestClassifier
import joblib
import re #regexp

def filter_text(text):
  text = text.lower() # lowercase
  text = text.strip() # пробелы в начале и конце
  pattern = r'[^\w\s]' # все что ни слово и не пробел
  text = re.sub(pattern, "", text) # вырезаем все, что не слово и не пробел
  return text

#  Взять файл и сделать с ним
with open("big_bot_config.json", "r") as config_file:
  data = json.load(config_file)

INTENTS = data["intents"]

# Классификация текстов - определение к какому классу (интенту) относится текст
# Скормим модели обучающую выборку (Фраза + Интент)
# Обучающая выборка состоит из входных X данных (фраз) и выходных y данных (интентов)
X = []
y = []

for intent in INTENTS:
  examples = INTENTS[intent]["examples"]
  for example in examples:
    example = filter_text(example)
    if len(example) < 3:
      continue # пропускаем короткий текст
    X.append(example)
    y.append(intent)

vectorizer = CountVectorizer() #  настройки в скобочках
# ДЗ: попробовать ngram_range, analyzer

vectorizer.fit(X) # обучаем векторайзер
vecX = vectorizer.transform(X) # преобразуем тексты в вектора

# Обучаем модель классификации
# Выбираем алгоритм/модель (для этого нужно экспериментировать, иметь опыт)

model = RandomForestClassifier() # Настройки (n_estimators = ?, max_dept = ?)
model.fit(vecX, y) # Обучение модели

joblib.dump(model, 'intent_model.pkl')
joblib.dump(vectorizer, 'vectorizer.pkl')

print("Model and vectorizer saved!")
