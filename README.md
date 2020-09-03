# mrsearch

Mrsearch is a full text distributed search engine.  To index a document you provide a 64 bit id and the text.  Searching returns a list of ids with the most recent first. 

# Dependencies

* Linux kernel version 5.4+
* [Mrloop an io_uring based C event loop](https://github.com/MarkReedZ/mrloop)
* [The python asyncio client](https://github.com/MarkReedZ/asyncmrsearch)

# Usage

To build and run an in memory search cache with 128mb of memory

```
./bld
./mrsearch -m 128
```

You may also specify the port and enable disk

```
Mrsearch Version 0.1
    -h, --help                    This help
    -p, --port=<num>              TCP port to listen on (default: 7011)
    -m, --max-memory=<mb>         Maximum amount of memory in mb (default: 256)
    -d, --max-disk=<gb>           Maximum amount of disk in gb (default: 1)
```

# Clients

[Python asyncio](https://github.com/MarkReedZ/asyncmrsearch)

```
  # pip install asyncmrcache

  rc = await asyncmrsearch.create_client( [("localhost",7011)], loop)

  await rc.add(1,"this is a cat")
  await rc.add(2,"cat is great")
  await rc.add(3,"my cats are awesome")
  await rc.add(4,"my dog is not a cat")

  print(await rc.get(["cat"]))

  # Outputs
  # [1, 2, 4]
```



