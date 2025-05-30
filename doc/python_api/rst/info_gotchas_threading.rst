********************************
Python Threads are Not Supported
********************************

In short: Python threads cause Blender to crash in hard to diagnose ways. For
example, a crash can occur while rendering with Cycles, with Python drivers,
while a background thread is used to download some file.

So far, no work has been done to make Blender's Python integration thread safe,
so until it's properly supported, it's best not make use of this.

Note that some modules in the Python standard library may use threads as well.
An example is the `multiprocessing.Queue <https://docs.python.org/3/library/multiprocessing.html#multiprocessing.Queue>`_
class.

Python threading with Blender only works properly when the threads finish up
before the script does, for example by using ``threading.join()``. In other
words, they can only be used while the main Blender thread is blocked from
running.


Alternative Approaches
======================

For running Python code independently of Blender, it is recommended to use the
`multiprocessing <https://docs.python.org/3/library/multiprocessing.html>`_ module.


Code Examples
=============

Here is an example of threading supported by Blender:

.. code-block:: python

   import threading
   import requests

   urls = [
      "http://localhost:8000/file-1.blend",
      "http://localhost:8000/file-2.blend",
      "http://localhost:8000/file-3.blend",
   ]


   def download(url: str) -> None:
      name = threading.current_thread().name
      print("{}: Starting".format(name))
      response = requests.get(url)
      print("{}: Request status code {}".format(name, response.status_code))


   threads = [
      threading.Thread(
         name="thread-{}".format(index),
         target=lambda: download(url),
      )
      for index, url in enumerate(urls)
   ]

   print("Starting threads...")
   for t in threads:
      t.start()
   print("Waiting for threads to finish...")
   for t in threads:
      t.join()
   print("Threads all done, now Blender can continue")

This an example of an **unsupported** case, where a timer which runs many times
a second:

.. code-block:: python

   from threading import Timer

   def my_timer():
         t = Timer(0.1, my_timer)
         t.setDaemon(True)
         t.start()
         print("Running...")

   my_timer()

Use cases like the one above, which leave the thread running once the script
finishes, may seem to work for a while, but end up causing random crashes or
errors in Blender's own drawing code.
