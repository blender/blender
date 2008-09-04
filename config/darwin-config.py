#
# Note : if you want to alter this file
# copy it as a whole in the upper folder
# as user-config.py
# dont create a new file with only some
# vars changed.

import commands

# IMPORTANT NOTE : OFFICIAL BUILDS SHOULD BE DONE WITH SDKs
USE_SDK='true'

BF_PYTHON_VERSION = '2.3'

cmd = 'uname -p'
MAC_PROC=commands.getoutput(cmd) 
cmd = 'uname -r'
cmd_res=commands.getoutput(cmd) 
if cmd_res[0]=='7':
	MAC_CUR_VER='10.3'
elif cmd_res[0]=='8':
	MAC_CUR_VER='10.4'
else:
	MAC_CUR_VER='10.5'

if MAC_PROC == 'powerpc':
	LCGDIR = '#../lib/darwin-6.1-powerpc'
else :
	LCGDIR = '#../lib/darwin-8.x.i386'
LIBDIR = '${LCGDIR}'

if MAC_PROC== 'powerpc' and BF_PYTHON_VERSION == '2.3':
	MAC_MIN_VERS = '10.3'
	MACOSX_SDK='/Developer/SDKs/MacOSX10.3.9.sdk'
else:
	MAC_MIN_VERS = '10.4'
	MACOSX_SDK='/Developer/SDKs/MacOSX10.4u.sdk'


# enable ffmpeg  support
WITH_BF_FFMPEG = 'true'  # -DWITH_FFMPEG
BF_FFMPEG = "#extern/ffmpeg"
BF_FFMPEG_INC = '${BF_FFMPEG}/include'
if USE_SDK=='true':
	BF_FFMPEG_EXTRA = '-isysroot '+MACOSX_SDK+' -mmacosx-version-min='+MAC_MIN_VERS
#BF_FFMPEG_LIBPATH='${BF_FFMPEG}/lib'
#BF_FFMPEG_LIB = 'avformat.a avcodec.a avutil.a'

WITH_BF_VERSE = 'false'
BF_VERSE = "#extern/verse/dist"
BF_VERSE_LIBPATH = "${BF_BUILDDIR}/extern/verse/dist"
BF_VERSE_INCLUDE = BF_VERSE
BF_VERSE_LIBS = "libverse"

# python.org libs install in /library we want to use that for 2.5 
#
# if you want py2.5 on leopard without installing
# change value to BF_PYTHON = '/Library/Frameworks/Python.framework/Versions/'
# BEWARE: in that case it will work only on leopard

if BF_PYTHON_VERSION=='2.3':
	BF_PYTHON = '/System/Library/Frameworks/Python.framework/Versions/'
else:
	BF_PYTHON = '/Library/Frameworks/Python.framework/Versions/'

BF_PYTHON_INC = '${BF_PYTHON}${BF_PYTHON_VERSION}/include/python${BF_PYTHON_VERSION}'
BF_PYTHON_BINARY = '${BF_PYTHON}${BF_PYTHON_VERSION}/bin/python${BF_PYTHON_VERSION}'
BF_PYTHON_LIB = ''
BF_PYTHON_LIBPATH = '${BF_PYTHON}${BF_PYTHON_VERSION}/lib/python${BF_PYTHON_VERSION}/config'
BF_PYTHON_LINKFLAGS = '-u _PyMac_Error -framework System -framework Python'
if MAC_CUR_VER=='10.3' or  MAC_CUR_VER=='10.4':
	BF_PYTHON_LINKFLAGS ='-u __dummy '+BF_PYTHON_LINKFLAGS
	
BF_QUIET = '1'
WITH_BF_OPENMP = '0'

# Note : should be true, but openal simply dont work on intel
if MAC_PROC == 'i386':
	WITH_BF_OPENAL = 'false'
else:
	WITH_BF_OPENAL = 'true'
#different lib must be used  following version of gcc
# for gcc 3.3
#BF_OPENAL = LIBDIR + '/openal'
# for gcc 3.4 and ulterior
if MAC_PROC == 'powerpc':
	BF_OPENAL = '#../lib/darwin-8.0.0-powerpc/openal'
else :
	BF_OPENAL = LIBDIR + '/openal'
	
BF_OPENAL_INC = '${BF_OPENAL}/include'
BF_OPENAL_LIB = 'openal'
BF_OPENAL_LIBPATH = '${BF_OPENAL}/lib'

WITH_BF_SDL = 'true'
BF_SDL = LIBDIR + '/sdl' #$(shell sdl-config --prefix)
BF_SDL_INC = '${BF_SDL}/include' #$(shell $(BF_SDL)/bin/sdl-config --cflags)
BF_SDL_LIB = 'SDL' #BF_SDL #$(shell $(BF_SDL)/bin/sdl-config --libs) -lSDL_mixer
BF_SDL_LIBPATH = '${BF_SDL}/lib'

WITH_BF_FMOD = 'false'
BF_FMOD = LIBDIR + '/fmod'

WITH_BF_OPENEXR = 'true'
BF_OPENEXR = '${LCGDIR}/openexr'
BF_OPENEXR_INC = '${BF_OPENEXR}/include ${BF_OPENEXR}/include/OpenEXR'
BF_OPENEXR_LIB = ' Iex Half IlmImf Imath IlmThread'
BF_OPENEXR_LIBPATH = '${BF_OPENEXR}/lib'

WITH_BF_DDS = 'true'

WITH_BF_JPEG = 'true'
BF_JPEG = LIBDIR + '/jpeg'
BF_JPEG_INC = '${BF_JPEG}/include'
BF_JPEG_LIB = 'jpeg'
BF_JPEG_LIBPATH = '${BF_JPEG}/lib'

WITH_BF_PNG = 'true'
BF_PNG = LIBDIR + '/png'
BF_PNG_INC = '${BF_PNG}/include'
BF_PNG_LIB = 'png'
BF_PNG_LIBPATH = '${BF_PNG}/lib'

BF_TIFF = LIBDIR + '/tiff'
BF_TIFF_INC = '${BF_TIFF}/include'

WITH_BF_ZLIB = 'true'
BF_ZLIB = '/usr'
BF_ZLIB_INC = '${BF_ZLIB}/include'
BF_ZLIB_LIB = 'z'

WITH_BF_INTERNATIONAL = 'true'

BF_GETTEXT = LIBDIR + '/gettext'
BF_GETTEXT_INC = '${BF_GETTEXT}/include'
BF_GETTEXT_LIB = 'intl'
BF_GETTEXT_LIBPATH = '${BF_GETTEXT}/lib'

WITH_BF_FTGL = 'true'
BF_FTGL = '#extern/bFTGL'
BF_FTGL_INC = '${BF_FTGL}/include'
BF_FTGL_LIB = 'extern_ftgl'

WITH_BF_GAMEENGINE='true'
WITH_BF_PLAYER='true'

WITH_BF_ODE = 'false'
BF_ODE = LIBDIR + '/ode'
BF_ODE_INC = '${BF_ODE}/include'
BF_ODE_LIB = '${BF_ODE}/lib/libode.a'

WITH_BF_BULLET = 'true'
BF_BULLET = '#extern/bullet2/src'
BF_BULLET_INC = '${BF_BULLET}'
BF_BULLET_LIB = 'extern_bullet'

BF_SOLID = '#extern/solid'
BF_SOLID_INC = '${BF_SOLID}'
BF_SOLID_LIB = 'extern_solid'

WITH_BF_YAFRAY = 'true'

#WITH_BF_NSPR = 'true'
#BF_NSPR = $(LIBDIR)/nspr
#BF_NSPR_INC = -I$(BF_NSPR)/include -I$(BF_NSPR)/include/nspr
#BF_NSPR_LIB = 

# Uncomment the following line to use Mozilla inplace of netscape
#CPPFLAGS += -DMOZ_NOT_NET
# Location of MOZILLA/Netscape header files...
#BF_MOZILLA = $(LIBDIR)/mozilla
#BF_MOZILLA_INC = -I$(BF_MOZILLA)/include/mozilla/nspr -I$(BF_MOZILLA)/include/mozilla -I$(BF_MOZILLA)/include/mozilla/xpcom -I$(BF_MOZILLA)/include/mozilla/idl
#BF_MOZILLA_LIB =
# Will fall back to look in BF_MOZILLA_INC/nspr and BF_MOZILLA_LIB
# if this is not set.
#
# Be paranoid regarding library creation (do not update archives)
#BF_PARANOID = 'true'

# enable freetype2 support for text objects
BF_FREETYPE = LIBDIR + '/freetype'
BF_FREETYPE_INC = '${BF_FREETYPE}/include ${BF_FREETYPE}/include/freetype2'
BF_FREETYPE_LIB = 'freetype'
BF_FREETYPE_LIBPATH = '${BF_FREETYPE}/lib'

WITH_BF_QUICKTIME = 'true' # -DWITH_QUICKTIME

WITH_BF_ICONV = 'true'
BF_ICONV = LIBDIR + "/iconv"
BF_ICONV_INC = '${BF_ICONV}/include'
BF_ICONV_LIB = 'iconv'
#BF_ICONV_LIBPATH = '${BF_ICONV}/lib'

# Mesa Libs should go here if your using them as well....
WITH_BF_STATICOPENGL = 'true'
BF_OPENGL_LIB = 'GL GLU'
BF_OPENGL_LIBPATH = '/System/Library/Frameworks/OpenGL.framework/Libraries'
BF_OPENGL_LINKFLAGS = '-framework OpenGL'

CFLAGS = ['-pipe','-fPIC','-funsigned-char']

CPPFLAGS = ['-fpascal-strings']
CCFLAGS = ['-pipe','-fPIC','-funsigned-char','-fpascal-strings']
CXXFLAGS = [ '-pipe','-fPIC','-funsigned-char', '-fpascal-strings']
PLATFORM_LINKFLAGS = '-fexceptions -framework CoreServices -framework Foundation -framework IOKit -framework AppKit -framework Carbon -framework AGL -framework AudioUnit -framework AudioToolbox -framework CoreAudio -framework QuickTime'

#note to build succesfully on 10.3.9 SDK you need to patch  10.3.9 by adding the SystemStubs.a lib from 10.4
LLIBS = 'stdc++ SystemStubs'

# some flags shuffling for different Os versions
if MAC_MIN_VERS == '10.3':
	CFLAGS = ['-fuse-cxa-atexit']+CFLAGS
	CXXFLAGS = ['-fuse-cxa-atexit']+CXXFLAGS
	PLATFORM_LINKFLAGS = '-fuse-cxa-atexit '+PLATFORM_LINKFLAGS
	LLIBS = LLIBS + ' crt3.o'
	
if USE_SDK=='true':
	SDK_FLAGS=['-isysroot', MACOSX_SDK,'-mmacosx-version-min='+MAC_MIN_VERS]	
	PLATFORM_LINKFLAGS = '-mmacosx-version-min='+MAC_MIN_VERS+ ' -Wl,-syslibroot,' + MACOSX_SDK+" "+PLATFORM_LINKFLAGS
	CCFLAGS=SDK_FLAGS+CCFLAGS
	CXXFLAGS=SDK_FLAGS+CXXFLAGS
	
# you can add -mssse3 if gcc >= 4.2
if MAC_PROC == 'i386':
	REL_CFLAGS = ['-O2','-ftree-vectorize','-msse','-msse2','-msse3']
	REL_CCFLAGS = ['-O2','-ftree-vectorize','-msse','-msse2','-msse3']
else:
	CFLAGS = CFLAGS+['-fno-strict-aliasing']
	CCFLAGS =  CCFLAGS+['-fno-strict-aliasing']
	CXXFLAGS = CXXFLAGS+['-fno-strict-aliasing']
	
	REL_CFLAGS = ['-O2']
	REL_CCFLAGS = ['-O2']

##BF_DEPEND = 'true'
##
##AR = ar
##ARFLAGS = ruv
##ARFLAGSQUIET = ru
##
CC = 'gcc'
CXX = 'g++'
C_WARN = ' -Wall  -Wno-long-double -Wdeclaration-after-statement '

CC_WARN = ' -Wall  -Wno-long-double'

##FIX_STUBS_WARNINGS = -Wno-unused

##LOPTS = --dynamic
##DYNLDFLAGS = -shared $(LDFLAGS)

BF_PROFILE_FLAGS = ' -pg -g '
BF_PROFILE = 'false'

BF_DEBUG = 'false'
BF_DEBUG_FLAGS = '-g'

BF_BUILDDIR='../build/darwin'
BF_INSTALLDIR='../install/darwin'
