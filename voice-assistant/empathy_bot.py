import re
import random
from pymorphy3 import MorphAnalyzer

morph = MorphAnalyzer()

def normalize_grammar(text):
    # Исправляем "что" → "с тем что"Vjz
    text = re.sub(r'(сталкиваться|столкнуться|смириться) с что', r'\1 с тем что', text)
    
    # Согласование глаголов с подлежащим
    words = text.split()
    for i in range(len(words)-1):
        if words[i] in {'бесит', 'раздражает'}:
            parsed = morph.parse(words[i])[0]
            if morph.parse(words[i-1])[0].tag.gender == 'femn':
                words[i] = parsed.inflect({'femn'}).word
    return ' '.join(words)

def rephrase_with_empathy(phrase):
    patterns = {
        r'меня бесит (.+)': [
            ('Знаю, как сложно сталкиваться с тем, что {gramm_fixed}', True),
            ('{gramm_fixed} — это правда раздражает', False),
            ('Понимаю, когда {gramm_fixed} — это выводит из себя', True)
        ],
        r'я не могу (.+)': [
            ('Обидно осознавать, что {gramm_fixed} не получается', True),
            ('{gramm_fixed} — сложная задача', False)
        ]
    }

    for pattern, responses in patterns.items():
        if match := re.search(pattern, phrase.lower()):
            core = match.group(1)
            template, needs_grammar = random.choice(responses)
            
            if needs_grammar:
                core = core.replace(' что ', ' ')  # Убираем лишние "что"
                gramm_fixed = normalize_grammar(core)
            else:
                gramm_fixed = core
                
            return template.format(gramm_fixed=gramm_fixed)
    
    return normalize_grammar(phrase)

def empathy_bot():
    intros = [
        "Понимаю твои чувства...",
        "Это действительно тяжело...",
        "Мне жаль это слышать..."
    ]

    print("Грамматически правильный бот запущен (Ctrl+C для выхода)")
    while True:
        try:
            user_phrase = input("Твоя фраза: ").strip()
            if not user_phrase:
                continue

            response = f"{random.choice(intros)} {rephrase_with_empathy(user_phrase)}"
            print(response)
        except KeyboardInterrupt:
            print("\nЗавершаю работу...")
            break

if __name__ == "__main__":
    # Проверяем наличие pymorphy2
    try:
        MorphAnalyzer()
    except ImportError:
        print("Ошибка: сначала установите pymorphy2!")
        print("Выполните: pip install pymorphy2")
        exit(1)
    
    empathy_bot()