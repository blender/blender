
.. Keep this document short & concise,
   linking to external resources instead of including content in-line.
   See 'release/text/readme.html' for the end user read-me.


Resona, adapted from Blender
=======
**Build for Linux, Windows and Mac required. Help us, build and share the packages to support our effort...**

Blender is the free and open source 3D creation suite. It supports the entirety of the 3D pipeline-modeling, rigging, animation, simulation, rendering, compositing,
motion tracking and video editing.

For the last few years, the Video Sequencer Editor (VSE) of Blender has seen slow development, and there have been many requests from various artists to add exciting new features and also to make necessary modifications. RESONA is intended to be an Independent Project aiming at delivering a refreshing Video Editing Capability for Artists and Users, along with a Production-Quality Audio Editing Setup.

.. figure:: https://aldrinmathew.com/wp-content/uploads/2020/07/splash.jpg
   :scale: 50 %
   :align: center


I am currently linking to the Official Blender Website, as this project is yet to have an independent domain, community and documentation. Please feel Free to contribute to the Project. Honestly, we need a lot of help.

Goals
-----

**COMPLETED** - Changed Startup Screen, Added **Color Grading Workspace**, Brand New **"Resona Dark" Theme** for Video Editing, Modifying Startup File

#5 - **Change Icon** - Change the Icon of the software to a new one. The icon design is finished. **ONGOING - Adapting to various Build OS**

#6 - **Change Software Name** - Change the name of the software from "Blender" to "Resona"

#7 - **Surround Sound** - Setting up Surround Sound by adding Camera Objects into the 3D View. The Setup should be scalable, and importing of animation data and speakers should be compatible.

#8 - **VSE Multithreading** - So far, Blender's Video Sequencer Editor (VSE) does not support Multithreaded Video Rendering. This would be a huge milestone and will help a lot of Video Editors and Artists out there. This step will make the Workflow much easier for people with Multiple Core CPUs. According to the current data, most people involved in Video/Audio related workflows have at least Quad Core CPUs. The idea is to find the number of threads of CPU, and assign Consecutive frames to corresponding Threads. The exported Video or Image Sequence file should have the Frames arranged properly (it is a challenge, because not all frames finish rendering at the same time, and waiting for a frame to finish rendering process is Counter-productive).

#9 - **File Format** - A Unique and separate file format for Resona Projects. My suggestions are ".resona" and ".sonar". Existing File Extensions should be compared and checked before making changes.

#10 - **Audio Editing & SFX** - Add Audio Editing Capabilities and SFX Feature. This would kickstart the program as a Video and Audio Editing Software.

#11 - **Scraping Off Features** - As you understand, Resona is not a replacement for Blender, but instead an Extension to the program in terms of Video and Audio Editing. So if Resona is supposed to be Independent, it should no longer have features that is unnecessary to Video and Audio Editing. We should not dismiss the possibility of incorporating Motion Graphics, 3D Audio, Audio Object Animation and SFX into the program in the future. So every step of removing a feature should be careful. Most of the features of Blender will be useful in this program, when we factor in 3D Audio and Audio Object Animation.

#12 - **Open 3D Audio Format** - This would be a huge milestone considering the scale of implementation. It will also require heavy development and planning. The goal is to provide an Open Source and Free Alternative to the current expensive options for 3D Audio. This should be tailored for Cinema, Filmmaking, Animation, Video Editing...

Project Pages
-------------

- `Project Page (Temporary) <https://aldrinmathew.com/resona>`__
- `Main Website <http://www.blender.org>`__
- `Reference Manual <https://docs.blender.org/manual/en/latest/index.html>`__
- `User Community <https://www.blender.org/community/>`__

Development
-----------

- `Build Instructions <https://wiki.blender.org/wiki/Building_Blender>`__
- `Code Review & Bug Tracker <https://developer.blender.org>`__
- `Developer Forum <https://devtalk.blender.org>`__
- `Developer Documentation <https://wiki.blender.org>`__


License
-------

Blender as a whole is licensed under the GNU Public License, Version 3.
Individual files may have a different, but compatible license.

See `blender.org/about/license <https://www.blender.org/about/license>`__ for details.
