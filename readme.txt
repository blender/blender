Welcome to the fun world of opensource.

To help you get started do the following before starting:

--------------UNIX TIPS---------------------------------------
Assuming your using tcsh/csh do the following before compiling.
#Set this to wherever you have extracted the source.
setenv NANBLENDERHOME `pwd`
setenv MAKEFLAGS "-w -I$NANBLENDERHOME/source"

set NANBLENDERHOME=`pwd`
export NANBLENDERHOME
set MAKEFLAGS="-w -I$NANBLENDERHOME/source"
export MAKEFLAGS

Then edit source/nan_definitions.mk to fit your environment.

After that cd $NANBLENDERHOME/intern 
make
make install
Then cd $NANBLENDERHOME/intern/python and follow the instructions in README
Then cd $NANBLENDERHOME/source
make

If you have any problems with the above post a message to the Forums on
www.blender.org
----------------------WINDOWS TIPS--------------------------------------
If you have any problems with the above post a message to the Forums on
www.blender.org


----------------------Mac OSX TIPS--------------------------------------

Build the intern libraries according to the description above. Instead of going
to the $NANBLENDERHOME/intern/python directory and reading the instructions, 
you can better directly go to the $NANBLENDERHOME/intern/python/freeze 
directory and make there. You can of course read the README.NaN but it should
not be necessary (unless you want to know about the process of "freezing" 
Python code).

Now before you go to the source directory, make sure you have installed the
external libaries that Blender depends upon. Here is a description of the 
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
Will be added later.

RANLIB:
Although the make files run ranlib on the libraries built, the gcc linker 
complains about ranlib not being run. Until there is a solution, you will need
to run ranlib by hand once in a while when the make breaks. Luckily, the error
message lists the full path of the file to run ranlib on... Anybody ot there 
with a real solution?

Success!
