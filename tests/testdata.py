
import words
import random

common_words = words.common_words

docs = []
for x in range(100000):
  s = words.getWord() + " "
  for y in range(random.randrange(200)):
    s += words.getWord() + " "
  docs.append(s)

