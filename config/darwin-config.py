#
# Note : if you want to alter this file
# copy it as a whole in the upper folder
# as user-config.py
# dont create a new file with only some
# vars changed.

import commands

# IMPORTANT NOTE : OFFICIAL BUILDS SHOULD BE DONE WITH SDKs
USE_SDK=True

#############################################################################
###################     Cocoa & architecture settings      ##################
#############################################################################
WITH_GHOST_COCOA=True
MACOSX_ARCHITECTURE = 'i386' # valid archs: ppc, i386, ppc64, x86_64


cmd = 'uname -p'
MAC_PROC=commands.getoutput(cmd) 
cmd = 'uname -r'
cmd_res=commands.getoutput(cmd) 
if cmd_res[0]=='7':
	MAC_CUR_VER='10.3'
elif cmd_res[0]=='8':
	MAC_CUR_VER='10.4'
elif cmd_res[0]=='9':
	MAC_CUR_VER='10.5'
elif cmd_res[0]=='10':
	MAC_CUR_VER='10.6'

if MACOSX_ARCHITECTURE == 'x86_64' or MACOSX_ARCHITECTURE == 'ppc64':
	USE_QTKIT=True # Carbon quicktime is not available for 64bit


# Default target OSX settings per architecture
# Can be customized

if MACOSX_ARCHITECTURE == 'ppc':
# ppc release are now made for 10.4
#	MAC_MIN_VERS = '10.3'
#	MACOSX_SDK='/Developer/SDKs/MacOSX10.3.9.sdk'
#	LCGDIR = '#../lib/darwin-6.1-powerpc'
#	CC = 'gcc-3.3'
#	CXX = 'g++-3.3'
	MAC_MIN_VERS = '10.4'
	MACOSX_DEPLOYMENT_TARGET = '10.4'
	MACOSX_SDK='/Developer/SDKs/MacOSX10.4u.sdk'
	LCGDIR = '#../lib/darwin-8.0.0-powerpc'
	CC = 'gcc-4.0'
	CXX = 'g++-4.0'
elif MACOSX_ARCHITECTURE == 'i386':
	MAC_MIN_VERS = '10.4'
	MACOSX_DEPLOYMENT_TARGET = '10.4'
	MACOSX_SDK='/Developer/SDKs/MacOSX10.4u.sdk'
	LCGDIR = '#../lib/darwin-8.x.i386'
	CC = 'gcc-4.0'
	CXX = 'g++-4.0'
else :
	MAC_MIN_VERS = '10.5'
	MACOSX_DEPLOYMENT_TARGET = '10.5'
	MACOSX_SDK='/Developer/SDKs/MacOSX10.5.sdk'
	LCGDIR = '#../lib/darwin-9.x.universal'
	CC = 'gcc-4.2'
	CXX = 'g++-4.2'

LIBDIR = '${LCGDIR}'

#############################################################################
###################          Dependency settings           ##################
#############################################################################

#Defaults openMP to true if compiler (currently only gcc 4.2) handles it
if CC == 'gcc-4.2':
    WITH_BF_OPENMP = True  # multithreading for fluids, cloth and smoke
else:
    WITH_BF_OPENMP = False

# enable ffmpeg  support
WITH_BF_FFMPEG = True  # -DWITH_FFMPEG
BF_FFMPEG = LIBDIR + '/ffmpeg'
BF_FFMPEG_INC = "${BF_FFMPEG}/include"
BF_FFMPEG_LIBPATH='${BF_FFMPEG}/lib'
BF_FFMPEG_LIB = 'avcodec avdevice avformat avutil mp3lame swscale x264 xvidcore theora theoradec theoraenc vorbis vorbisenc vorbisfile ogg bz2'
#bz2 is a standard osx dynlib

# python 3.1 uses precompiled libraries in bf svn /lib by default
BF_PYTHON_VERSION = '3.1'
BF_PYTHON = LIBDIR + '/python'
BF_PYTHON_INC = '${BF_PYTHON}/include/python${BF_PYTHON_VERSION}'
# BF_PYTHON_BINARY = '${BF_PYTHON}/bin/python${BF_PYTHON_VERSION}'
BF_PYTHON_LIB = 'python${BF_PYTHON_VERSION}'
BF_PYTHON_LIBPATH = '${BF_PYTHON}/lib/python${BF_PYTHON_VERSION}'
# BF_PYTHON_LINKFLAGS = ['-u', '_PyMac_Error', '-framework', 'System']
	
WITH_BF_OPENAL = True
#different lib must be used  following version of gcc
# for gcc 3.3
#BF_OPENAL = LIBDIR + '/openal'
# for gcc 3.4 and ulterior
if MAC_PROC == 'powerpc':
	BF_OPENAL = '#../lib/darwin-8.0.0-powerpc/openal'
else :
	BF_OPENAL = LIBDIR + '/openal'

WITH_BF_STATICOPENAL = False
BF_OPENAL_INC = '${BF_OPENAL}/include' # only headers from libdir needed for proper use of framework !!!!
#BF_OPENAL_LIB = 'openal'
#BF_OPENAL_LIBPATH = '${BF_OPENAL}/lib'
# Warning, this static lib configuration is untested! users of this OS please confirm.
#BF_OPENAL_LIB_STATIC = '${BF_OPENAL}/lib/libopenal.a'

# Warning, this static lib configuration is untested! users of this OS please confirm.
BF_CXX = '/usr'
WITH_BF_STATICCXX = False
BF_CXX_LIB_STATIC = '${BF_CXX}/lib/libstdc++.a'

BF_LIBSAMPLERATE = LIBDIR + '/samplerate'
BF_LIBSAMPLERATE_INC = '${BF_LIBSAMPLERATE}/include'
BF_LIBSAMPLERATE_LIB = 'samplerate'
BF_LIBSAMPLERATE_LIBPATH = '${BF_LIBSAMPLERATE}/lib'

# TODO - set proper paths here (add precompiled to lib/ ? )
WITH_BF_JACK = False
BF_JACK = '/usr'
BF_JACK_INC = '${BF_JACK}/include/jack'
BF_JACK_LIB = 'jack'
BF_JACK_LIBPATH = '${BF_JACK}/lib'

WITH_BF_SNDFILE = True
BF_SNDFILE = LIBDIR + '/sndfile'
BF_SNDFILE_INC = '${BF_SNDFILE}/include'
BF_SNDFILE_LIB = 'sndfile FLAC ogg vorbis vorbisenc'
BF_SNDFILE_LIBPATH = '${BF_SNDFILE}/lib ${BF_FFMPEG}/lib' #ogg libs are stored in ffmpeg dir

WITH_BF_SDL = True
BF_SDL = LIBDIR + '/sdl' #$(shell sdl-config --prefix)
BF_SDL_INC = '${BF_SDL}/include' #$(shell $(BF_SDL)/bin/sdl-config --cflags)
BF_SDL_LIB = 'SDL' #BF_SDL #$(shell $(BF_SDL)/bin/sdl-config --libs) -lSDL_mixer
BF_SDL_LIBPATH = '${BF_SDL}/lib'

WITH_BF_OPENEXR = True
WITH_BF_STATICOPENEXR = False
BF_OPENEXR = '${LCGDIR}/openexr'
BF_OPENEXR_INC = '${BF_OPENEXR}/include ${BF_OPENEXR}/include/OpenEXR'
BF_OPENEXR_LIB = ' Iex Half IlmImf Imath IlmThread'
BF_OPENEXR_LIBPATH = '${BF_OPENEXR}/lib'
# Warning, this static lib configuration is untested! users of this OS please confirm.
BF_OPENEXR_LIB_STATIC = '${BF_OPENEXR}/lib/libHalf.a ${BF_OPENEXR}/lib/libIlmImf.a ${BF_OPENEXR}/lib/libIex.a ${BF_OPENEXR}/lib/libImath.a ${BF_OPENEXR}/lib/libIlmThread.a'

WITH_BF_DDS = True

WITH_BF_JPEG = True
BF_JPEG = LIBDIR + '/jpeg'
BF_JPEG_INC = '${BF_JPEG}/include'
BF_JPEG_LIB = 'jpeg'
BF_JPEG_LIBPATH = '${BF_JPEG}/lib'

WITH_BF_PNG = True
BF_PNG = LIBDIR + '/png'
BF_PNG_INC = '${BF_PNG}/include'
BF_PNG_LIB = 'png'
BF_PNG_LIBPATH = '${BF_PNG}/lib'

BF_TIFF = LIBDIR + '/tiff'
BF_TIFF_INC = '${BF_TIFF}/include'

WITH_BF_ZLIB = True
BF_ZLIB = '/usr'
BF_ZLIB_INC = '${BF_ZLIB}/include'
BF_ZLIB_LIB = 'z'

WITH_BF_INTERNATIONAL = True

BF_GETTEXT = LIBDIR + '/gettext'
BF_GETTEXT_INC = '${BF_GETTEXT}/include'
BF_GETTEXT_LIB = 'intl'
BF_GETTEXT_LIBPATH = '${BF_GETTEXT}/lib'

WITH_BF_GAMEENGINE=True
WITH_BF_PLAYER = False

WITH_BF_BULLET = True
BF_BULLET = '#extern/bullet2/src'
BF_BULLET_INC = '${BF_BULLET}'
BF_BULLET_LIB = 'extern_bullet'

WITH_BF_FFTW3 = True
BF_FFTW3 = LIBDIR + '/fftw3'
BF_FFTW3_INC = '${BF_FFTW3}/include'
BF_FFTW3_LIB = 'libfftw3'
BF_FFTW3_LIBPATH = '${BF_FFTW3}/lib'

#WITH_BF_NSPR = True
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
#BF_PARANOID = True

# enable freetype2 support for text objects
BF_FREETYPE = LIBDIR + '/freetype'
BF_FREETYPE_INC = '${BF_FREETYPE}/include ${BF_FREETYPE}/include/freetype2'
BF_FREETYPE_LIB = 'freetype'
BF_FREETYPE_LIBPATH = '${BF_FREETYPE}/lib'

WITH_BF_QUICKTIME = True

WITH_BF_ICONV = True
BF_ICONV = '/usr'
BF_ICONV_INC = '${BF_ICONV}/include'
BF_ICONV_LIB = 'iconv'
#BF_ICONV_LIBPATH = '${BF_ICONV}/lib'

# Mesa Libs should go here if your using them as well....
WITH_BF_STATICOPENGL = True
BF_OPENGL_LIB = 'GL GLU'
BF_OPENGL_LIBPATH = '/System/Library/Frameworks/OpenGL.framework/Libraries'
BF_OPENGL_LINKFLAGS = ['-framework', 'OpenGL']

#OpenCollada flags
WITH_BF_COLLADA = False
BF_COLLADA = '#source/blender/collada'
BF_COLLADA_INC = '${BF_COLLADA}'
BF_COLLADA_LIB = 'bf_collada'
BF_OPENCOLLADA = LIBDIR + '/opencollada'
BF_OPENCOLLADA_INC = '${BF_OPENCOLLADA}/include'
BF_OPENCOLLADA_LIB = 'OpenCOLLADASaxFrameworkLoader OpenCOLLADAFramework OpenCOLLADABaseUtils OpenCOLLADAStreamWriter MathMLSolver GeneratedSaxParser UTF xml2 buffer ftoa'
BF_OPENCOLLADA_LIBPATH = LIBDIR + '/opencollada'
BF_PCRE = LIBDIR + '/opencollada'
BF_PCRE_LIB = 'pcre'
BF_PCRE_LIBPATH = '${BF_PCRE}/lib'
#BF_EXPAT = '/usr'
#BF_EXPAT_LIB = 'expat'
#BF_EXPAT_LIBPATH = '/usr/lib'

#Ray trace optimization
if MACOSX_ARCHITECTURE == 'x86_64' or MACOSX_ARCHITECTURE == 'i386':
    WITH_BF_RAYOPTIMIZATION = True
else:
    WITH_BF_RAYOPTIMIZATION = False
if MACOSX_ARCHITECTURE == 'i386':
    BF_RAYOPTIMIZATION_SSE_FLAGS = ['-msse']
elif MACOSX_ARCHITECTURE == 'x86_64':
    BF_RAYOPTIMIZATION_SSE_FLAGS = ['-msse','-msse2']
    

#############################################################################
###################  various compile settings and flags    ##################
#############################################################################

BF_QUIET = '1' # suppress verbose output

if MACOSX_ARCHITECTURE == 'x86_64' or MACOSX_ARCHITECTURE == 'ppc64':
	ARCH_FLAGS = ['-m64']
else:
	ARCH_FLAGS = ['-m32']

CFLAGS = ['-pipe','-funsigned-char']+ARCH_FLAGS

CPPFLAGS = ['-fpascal-strings']+ARCH_FLAGS
CCFLAGS = ['-pipe','-funsigned-char','-fpascal-strings']+ARCH_FLAGS
CXXFLAGS = ['-pipe','-funsigned-char', '-fpascal-strings']+ARCH_FLAGS

if WITH_GHOST_COCOA==True:
	PLATFORM_LINKFLAGS = ['-fexceptions','-framework','CoreServices','-framework','Foundation','-framework','IOKit','-framework','AppKit','-framework','Cocoa','-framework','Carbon','-framework','AudioUnit','-framework','AudioToolbox','-framework','CoreAudio','-framework','OpenAL']+ARCH_FLAGS
else:
	PLATFORM_LINKFLAGS = ['-fexceptions','-framework','CoreServices','-framework','Foundation','-framework','IOKit','-framework','AppKit','-framework','Carbon','-framework','AGL','-framework','AudioUnit','-framework','AudioToolbox','-framework','CoreAudio','-framework','OpenAL']+ARCH_FLAGS

if WITH_BF_QUICKTIME == True:
	if USE_QTKIT == True:
		PLATFORM_LINKFLAGS = PLATFORM_LINKFLAGS+['-framework','QTKit']
	else:
		PLATFORM_LINKFLAGS = PLATFORM_LINKFLAGS+['-framework','QuickTime']

#note to build succesfully on 10.3.9 SDK you need to patch  10.3.9 by adding the SystemStubs.a lib from 10.4
LLIBS = ['stdc++', 'SystemStubs']

# some flags shuffling for different Os versions
if MAC_MIN_VERS == '10.3':
	CFLAGS = ['-fuse-cxa-atexit']+CFLAGS
	CXXFLAGS = ['-fuse-cxa-atexit']+CXXFLAGS
	PLATFORM_LINKFLAGS = ['-fuse-cxa-atexit']+PLATFORM_LINKFLAGS
	LLIBS.append('crt3.o')
	
if USE_SDK==True:
	SDK_FLAGS=['-isysroot', MACOSX_SDK,'-mmacosx-version-min='+MAC_MIN_VERS,'-arch',MACOSX_ARCHITECTURE]	
	PLATFORM_LINKFLAGS = ['-mmacosx-version-min='+MAC_MIN_VERS,'-Wl','-isysroot',MACOSX_SDK,'-arch',MACOSX_ARCHITECTURE]+PLATFORM_LINKFLAGS
	CCFLAGS=SDK_FLAGS+CCFLAGS
	CXXFLAGS=SDK_FLAGS+CXXFLAGS

#Intel Macs are CoreDuo and Up	
if MACOSX_ARCHITECTURE == 'i386' or MACOSX_ARCHITECTURE == 'x86_64':
	REL_CFLAGS = ['-O2','-ftree-vectorize','-msse','-msse2','-msse3','-mfpmath=sse']
	REL_CCFLAGS = ['-O2','-ftree-vectorize','-msse','-msse2','-msse3','-mfpmath=sse']
else:
	CFLAGS = CFLAGS+['-fno-strict-aliasing']
	CCFLAGS =  CCFLAGS+['-fno-strict-aliasing']
	CXXFLAGS = CXXFLAGS+['-fno-strict-aliasing']
	REL_CFLAGS = ['-O2']
	REL_CCFLAGS = ['-O2']

# Intel 64bit Macs are Core2Duo and up
if MACOSX_ARCHITECTURE == 'x86_64':
	REL_CFLAGS = REL_CFLAGS+['-march=core2','-mssse3','-with-tune=core2','-enable-threads']
	REL_CCFLAGS = REL_CCFLAGS+['-march=core2','-mssse3','-with-tune=core2','-enable-threads']

CC_WARN = ['-Wall']
C_WARN = ['-Wno-char-subscripts', '-Wpointer-arith', '-Wcast-align', '-Wdeclaration-after-statement', '-Wno-unknown-pragmas']
CXX_WARN = ['-Wno-invalid-offsetof', '-Wno-sign-compare']

##FIX_STUBS_WARNINGS = -Wno-unused

##LOPTS = --dynamic
##DYNLDFLAGS = -shared $(LDFLAGS)

BF_PROFILE_CCFLAGS = ['-pg', '-g ']
BF_PROFILE_LINKFLAGS = ['-pg']
BF_PROFILE = False

BF_DEBUG = False
BF_DEBUG_CCFLAGS = ['-g']

#############################################################################
###################           Output directories           ##################
#############################################################################

BF_BUILDDIR='../build/darwin'
BF_INSTALLDIR='../install/darwin'
