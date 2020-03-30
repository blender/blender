Tutorials
=========

Introduction
------------

The C and Python binding for audaspace were designed with simplicity in mind.
This means however that to use the full capabilities of audaspace,
there is no way around the C++ library.

Simple Demo
-----------

The **simple.py** example program contains all the basic
building blocks for an application using audaspace.
These building blocks are basically the classes :class:`aud.Device`,
:class:`aud.Sound` and :class:`aud.Handle`.

We start with importing :mod:`aud` and :mod:`time`
as the modules we need for our simple example.

.. code-block:: python

   #!/usr/bin/python
   import aud, time

The first step now is to open an output device and this
can simply be done by allocating a :class:`aud.Device` object.

.. code-block:: python

   device = aud.Device()

To create a sound we can choose to load one from a :func:`aud.Sound.file`,
or we use one of our signal generators. We decide to do the latter
and create a :func:`aud.Sound.sine` signal with a frequency of 440 Hz.

.. code-block:: python

   sine = aud.Sound.sine(440)

.. note:: At this point nothing is playing back yet,
:class:`aud.Sound` objects are just descriptions of sounds.

However instead of a sine wave, we would like to have a square wave
to produce a more retro gaming sound. We could of course use the
:func:`aud.Sound.square` generator instead of sine,
but we want to show how to apply effects,
so we apply a :func:`aud.Sound.threshold`
which makes a square wave out of our sine too,
even if less efficient than directly generating the square wave.

.. code-block:: python

   square = sine.threshold()

.. note:: The :class:`aud.Sound` class offers generator and effect functions.

The we can play our sound by calling the
:func:`aud.Device.play` method of our device.
This method returns a :class:`aud.Handle`
which is used to control the playback of the sound.

.. code-block:: python

   handle = device.play(square)

Now if we do nothing else anymore the application will quit immediately,
so we won't hear much of our square wave,
so we decide to wait for three seconds before
quitting the application by calling :func:`time.sleep`.

.. code-block:: python

   time.sleep(3)

Audioplayer
-----------

Now that we know the basics of audaspace,
we can build our own music player easily
by just slightly changing the previous program.
The **player.py** example does exactly that,
let's have a short look at the differences:

Instead of creating a sine signal and thresholding it,
we in fact use the :func:`aud.Sound.file` function to load a sound from a file.
The filename we pass is the first command line argument our application got.

.. code-block:: python

   sound = aud.Sound.file(sys.argv[1])

When the sound gets played back we now want to wait until
the whole file has been played, so we use the :data:`aud.Handle.status`
property to determine whether the sound finished playing.

.. code-block:: python

   while handle.status:
   	time.sleep(0.1)

We don't make any error checks if the user actually added a command
line argument. As an exercise you could extend this program to play
any number of command line supplied files in sequence.

Siren
-----

Let's get a little bit more complex. The **siren.py** example
plays a generated siren sound that circles around your head.
Depending on how many speakers you have and if the output
device used supports the speaker setup, you will hear this effect.
With stereo speakers you should at least hear some left-right-panning.

We start off again with importing the modules we need and
we also define some properties of our siren sound.
We want it to consist of two sine sounds with different frequencies.
We define a length for the sine sounds and how long a fade in/out should take.
We also know already how to open a device.

.. code-block:: python

   #!/usr/bin/python
   import aud, math, time
   length = 0.5
   fadelength = 0.05

   device = aud.Device()

The next thing to do is to define our sine waves and apply all the required effects.
As each of the effect functions returns the corresponding sound,
we can easily chain those calls together.

.. code-block:: python

   high = aud.Sound.sine(880).limit(0, length).fadein(0, fadelength).fadeout(length - fadelength, length)
   low = aud.Sound.sine(700).limit(0, length).fadein(0, fadelength).fadeout(length - fadelength, length).volume(0.6)

The next step is to connect the two sines,
which we do using the :func:`aud.Sound.join` function.

.. code-block:: python

   sound = high.join(low)

The generated siren sound can now be played back and what we also do is to loop it.
Therefore we set the :data:`aud.Handle.loop_count` to a negative value to loop forever.

.. code-block:: python

   handle = device.play(sound)
   handle.loop_count = -1

Now we use some timing code to make sure our demo runs for 10 seconds,
but we also use the time to update the location of our playing sound,
with the :data:`aud.Handle.location` property, which is a three dimensional vector.
The trigonometic calculation based on the running time of the program keeps
the sound on the XZ plane letting it follow a circle around us.

.. code-block:: python

   start = time.time()

   while time.time() - start < 10:
   	angle = time.time() - start

   	handle.location = [math.sin(angle), 0, -math.cos(angle)]

As an exercise you could try to let the sound come from the far left
and go to the far right and a little bit in front of you within the
10 second runtime of the program. With this change you should be able
to hear the volume of the sound change, depending on how far it is away from you.
Updating the :data:`aud.Handle.velocity` property properly also enables the doppler effect.
Compare your solution to the **siren2.py** demo.

Tetris
------

The **tetris.py** demo application shows an even more
complex application which generates retro tetris music.
Looking at the source code there should be nothing new here,
again the functions used from audaspace are the same as in the previous examples.
In the :func:`parseNote` function all single notes get joined which leads
to a very long chain of sounds. If you think of :func:`aud.Sound.join`
as a function that creates a binary tree with the two joined sounds as
leaves then the :func:`parseNote` function creates a very unbalanced tree.

Insted we could rewrite the code to use two other classes:
:class:`aud.Sequence` and :class:`aud.SequenceEntry` to sequence the notes.
The **tetris2.py** application does exactly that.
Before the while loop we add a variable that stores the current position
in the score and create a new :class:`aud.Sequence` object.

.. code-block:: python

   position = 0
   sequence = aud.Sequence()

Then in the loop we can create the note simply by chaining the
:func:`aud.Sound.square` generator and :func:`aud.Sound.fadein`
and :func:`aud.Sound.fadeout` effects.

.. code-block:: python

   note = aud.Sound.square(freq, rate).fadein(0, fadelength).fadeout(length - fadelength, fadelength)

Now instead of using :func:`aud.Sound.limit` and :func:`aud.Sound.join`
we simply add the sound to the sequence.

.. code-block:: python

   entry = sequence.add(note, position, position + length, 0)

The entry returned from the :func:`aud.Sequence.add`
function is an object of the :class:`aud.SequenceEntry` class.
We can use this entry to mute the note in case it's actually a pause.

.. code-block:: python

   if char == 'p':
   	entry.muted = True

Lastly we have to update our position variable.

.. code-block:: python

   position += length

Now in **tetris2.py** we used the :data:`aud.SequenceEntry.muted`
property to show how the :class:`aud.SequenceEntry` class can be used,
but it would actually be smarter to not even create a note for pauses and just skip them.
You can try to implement this as an exercise and then check out the solution in **tetris3.py**.

Conclusion
----------

We introduced all five currently available classes in the audaspace Python API.
Of course all classes offer a lot more functions than have been used in these demo applications,
check out the specific class documentation for more details.
