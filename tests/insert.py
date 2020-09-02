
import asyncio, time
import asyncmrsearch
import time

import mrpacker

f = open("insert_data.mrp","rb")
docs = mrpacker.unpack(f.read())
f.close()

import uvloop
asyncio.set_event_loop_policy(uvloop.EventLoopPolicy())

def lcb(client):
  print("conn lost")

async def run(loop):

  rc = await asyncmrsearch.create_client( [("localhost",7011)], loop, lost_cb=lcb)

  start = time.time()
  x = 0
  l = 0
  for doc in docs:
    await rc.add(x,doc)
    l += len(doc)
    x += 1
  end = time.time()
  t = end-start
  lmb = l/1024/1024
  mb_per_second = lmb/t
  gb_per_hour = mb_per_second*60*60/1024
  print(t, lmb/t, gb_per_hour)
    
  await rc.close()    
  #ids = await rc.get(["this"])
  #print(ids)

  return


if __name__ == '__main__':
  loop = asyncio.get_event_loop()
  loop.run_until_complete(run(loop))
  loop.close()
  print("DONE")


