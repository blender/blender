audaspace
=========

Audaspace (pronounced "outer space") is a high level audio library written in C++ with language bindings for Python for example. It started out as the audio engine of the 3D modelling application Blender and is now released as a standalone library.

Documentation and Community
---------------------------

The documentation including guides for building and installing, demos, tutorials as well as the API reference for C++, C and python can be found on https://audaspace.github.io.

Bug reports and feature requests should go to the [issue tracker](https://github.com/audaspace/audaspace/issues).

For any other discussions about audaspace there is a [mailing list](https://groups.google.com/forum/#!forum/audaspace) and there is also the IRC channel #audaspace on irc.freenode.net.

Features
--------

The following (probably incomplete) features are supported by audaspace:

* input/output devices
 * input from microphones, line in, etc.
 * output devices including 3D audio support
* file reading/writing
* filters like low-/highpass and effects like delay, reverse or fading
* generators for simple waveforms like silence, sine and triangle
* respecification - this term is used for changing stream parameters which are
 * channel count - channel remapping
 * sample format - the library internally uses 32 bit floats
 * sample rate - resampling
* simple (superposition, joining and ping-pong aka forward-reverse) and more complex (non-linear audio editing) sequencing of sounds

License
-------

> Copyright © 2009-2023 Jörg Müller. All rights reserved.
>
> Licensed under the Apache License, Version 2.0 (the "License");
> you may not use this file except in compliance with the License.
> You may obtain a copy of the License at
>
>   http://www.apache.org/licenses/LICENSE-2.0
>
> Unless required by applicable law or agreed to in writing, software
> distributed under the License is distributed on an "AS IS" BASIS,
> WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
> See the License for the specific language governing permissions and
> limitations under the License.
