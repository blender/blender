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
Then cd $NANBLENDERHOME/intern/python and follow the instructions 
Then cd $NANBLENDERHOME/source
make

If you have any problems with the above post a message to the Forums on
www.blender.org
----------------------WINDOWS TIPS--------------------------------------
If you have any problems with the above post a message to the Forums on
www.blender.org
