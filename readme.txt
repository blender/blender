Welcome to the fun world of open-source.

This file is to help you get started using the source and will hopefully
answer most questions.

Here are some links to external packages you may or maynot need:

openssl:  http://www.openssl.org
python:  http://www.python.org
nspr:  ftp://ftp.mozilla.org/pub/nspr/releases
libjpeg:  http://www.ijg.org/
libpng:  http://www.libpng.org/pub/png/
zlib:   http://www.gzip.org/zlib/
openal:  http://www.openal.org/home/		(for linux/windows)
	sdl: http://www.libsdl.org/index.php (for openal)
	smpeg: http://www.lokigames.com/development/smpeg.php3 (for openal)
fmod: http://www.fmod.org/

If you do not have GL you will also need mesa:
http://www.mesa3d.org

--------------Basic Makefile TIPS---------------------------------------
Assuming you are using tcsh/csh do the following before compiling.
#Set this to wherever you have extracted the source.
setenv NANBLENDERHOME `pwd`
setenv MAKEFLAGS "-w -I$NANBLENDERHOME/source"

Or for bash/sh do this:
NANBLENDERHOME=`pwd`
export NANBLENDERHOME
MAKEFLAGS="-w -I$NANBLENDERHOME/source"
export MAKEFLAGS

Then edit source/nan_definitions.mk to fit you're environment.
(You'll want to change things like NAN_OPENSSL,NAN_JPEG, NAN_PNG etc.. 
to point to where you have it installed)

If you tried to just have a go at making stuff you might wind up with
an empty file /tmp/.nanguess
You need to remove the empty file and re run it after you have
setup the NANBLENDERHOME variable.

After that cd $NANBLENDERHOME/intern 
make
make install

Then cd $NANBLENDERHOME/intern/python/freeze 
make
cd $NANBLENDERHOME/source/blender/bpython/frozen
make -f Makefile.freeze

Then cd $NANBLENDERHOME/source
make

If you have any problems with the above post a message to the Forums on
www.blender.org
----------------------WINDOWS TIPS--------------------------------------
If you have any problems with the above post a message to the Forums on
www.blender.org


----------------------Mac OSX TIPS--------------------------------------
Now before you go to the source directory, make sure you have installed the
external libraries that Blender depends upon. Here is a description of the 
things you need.

FINK:
Use fink (http://fink.sourceforge.net/) to install the following libraries that
Blender depends on:
1. openssl (fink install openssl)
2. jpeg    (fink install jpeg)
3. png     (fink install png)

PYTHON:
Mac OSX 10.2 (Jaguar) now comes with Python (2.2.1) pre-installed. This is fine
for producing the "frozen" Python code found in the intern directory. However,
the installation does not contain the python library to link against (at least
I could not find it). You could use fink to install Python but that Python 
installation depends on X being installed and that is a large installation.

If you prefer the easy way: download Python 2.2.2 from http://www.python.org.
Follow the instructions to in the documentation to install it on your box. If
you run OSX 10.2 it should install just fine. Basically a configure and a 
"make" will do the job. The result is a Python library that should be copied to
the library tree together with the associated header files.

Create the following  directories:
$NANBLENDERHOME/lib/darwin-6.1-powerpc/python/include/python2.2
$NANBLENDERHOME/lib/darwin-6.1-powerpc/python/lib/python2.2/config
Now copy the include files and the libpython2.2.a library to those locations.

FMOD:
Download FMOD from http://www.fmod.org/ and unpack with StuffIt Expander. The
archive contains header files and a library. Copy those to these directories
(that you need to create first):
$NANBLENDERHOME/lib/darwin-6.1-powerpc/fmod/include
$NANBLENDERHOME/lib/darwin-6.1-powerpc/fmod/lib

RANLIB:
Although the make files run ranlib on the libraries built, the gcc linker 
complains about ranlib not being run. Until there is a solution, you will need
to run ranlib by hand once in a while when the make breaks. Luckily, the error
message lists the full path of the file to run ranlib on... Anybody out there 
with a real solution? I guess the problem arises from copying the files from 
one location to the other...

Now wait, don't type make yet! You'll have to edit a config file of ODE first.
go to $NANBLENDERHOME/source/ode/config and edit the file "user-settings" so 
that platform is equal to osx (PLATFORM=osx).

Success!
