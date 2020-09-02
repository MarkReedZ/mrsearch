
import words
import random

common_words = words.common_words

def getRandomSentence():
  return random.choice(words.sens)

sens = []
for x in range(100):
  sens.append( getRandomSentence() )

