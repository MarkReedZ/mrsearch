
import words
import random
import mrpacker
import time

common_words = words.common_words

start = time.time()
docs = []
for x in range(100000):
  s = words.getWord() + " "
  for y in range(random.randrange(200)):
    s += words.getWord() + " "
  docs.append(s)
end = time.time()
print("gen took", end-start)

f = open("insert_data.mrp","wb")
f.write( mrpacker.pack(docs) )
f.close()

