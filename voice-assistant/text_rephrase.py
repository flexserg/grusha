from transformers import pipeline

paraphraser = pipeline("text2text-generation", model="t5-small", device="cpu")  # Explicitly use CPU
result = paraphraser("The quick brown fox jumps over the lazy dog.")
print(result[0]['generated_text'])