Welcome to the fun world of opensource.

To help you get started do the following before starting:

Assuming your using tcsh/csh do the following before compiling.
#Set this to wherever you have extracted the source.
setenv NANBLENDERHOME `pwd`
setenv MAKEFLAGS "-w -I$NANBLENDERHOME/source"

set NANBLENDERHOME=`pwd`
export NANBLENDERHOME
set MAKEFLAGS="-w -I$NANBLENDERHOME/source"
export MAKEFLAGS

Then edit source/nan_definitions.mk to fit your environment.


