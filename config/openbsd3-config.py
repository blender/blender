LCGDIR = '../lib/openbsd3'
LIBDIR = '${LCGDIR}'

BF_PYTHON = '/usr/local'
BF_PYTHON_VERSION = '2.5'
BF_PYTHON_INC = '${BF_PYTHON}/include/python${BF_PYTHON_VERSION}'
BF_PYTHON_BINARY = '${BF_PYTHON}/bin/python${BF_PYTHON_VERSION}'
BF_PYTHON_LIB = 'python${BF_PYTHON_VERSION}'
BF_PYTHON_LIBPATH = '${BF_PYTHON}/lib/python${BF_PYTHON_VERSION}/config'

WITH_BF_OPENAL = False
# WITH_BF_STATICOPENAL = False
#BF_OPENAL = LIBDIR + '/openal'
#BF_OPENAL_INC = '${BF_OPENAL}/include'
#BF_OPENAL_LIB = 'openal'
#BF_OPENAL_LIBPATH = '${BF_OPENAL}/lib'
#BF_OPENAL_LIB_STATIC = '${BF_OPENAL}/lib/libopenal.a'

BF_LIBSAMPLERATE = '/usr/local'
BF_LIBSAMPLERATE_INC = '${BF_LIBSAMPLERATE}/include'
BF_LIBSAMPLERATE_LIB = 'samplerate'
BF_LIBSAMPLERATE_LIBPATH = '${BF_LIBSAMPLERATE}/lib'

WITH_BF_SDL = True
BF_SDL = '/usr/local' #$(shell sdl-config --prefix)
BF_SDL_INC = '${BF_SDL}/include/SDL' #$(shell $(BF_SDL)/bin/sdl-config --cflags)
BF_SDL_LIB = 'SDL' #BF_SDL #$(shell $(BF_SDL)/bin/sdl-config --libs) -lSDL_mixer
BF_SDL_LIBPATH = '${BF_SDL}/lib'

WITH_BF_OPENEXR = False
WITH_BF_STATICOPENEXR = False
BF_OPENEXR = '/usr/local'
BF_OPENEXR_INC = '${BF_OPENEXR}/include/OpenEXR'
BF_OPENEXR_LIB = 'Half IlmImf Iex Imath '
# Warning, this static lib configuration is untested! users of this OS please confirm.
BF_OPENEXR_LIB_STATIC = '${BF_OPENEXR}/lib/libHalf.a ${BF_OPENEXR}/lib/libIlmImf.a ${BF_OPENEXR}/lib/libIex.a ${BF_OPENEXR}/lib/libImath.a ${BF_OPENEXR}/lib/libIlmThread.a'

WITH_BF_DDS = True

WITH_BF_JPEG = True
BF_JPEG = '/usr/local'
BF_JPEG_INC = '${BF_JPEG}/include'
BF_JPEG_LIB = 'jpeg'
BF_JPEG_LIBPATH = '${BF_JPEG}/lib'

WITH_BF_PNG = True
BF_PNG = '/usr/local'
BF_PNG_INC = '${BF_PNG}/include/libpng'
BF_PNG_LIB = 'png'
BF_PNG_LIBPATH = '${BF_PNG}/lib'

BF_TIFF = '/usr/local'
BF_TIFF_INC = '${BF_TIFF}/include'

WITH_BF_ZLIB = True
BF_ZLIB = '/usr/local'
BF_ZLIB_INC = '${BF_ZLIB}/include'
BF_ZLIB_LIB = 'z'

WITH_BF_INTERNATIONAL = True

BF_GETTEXT = '/usr/local'
BF_GETTEXT_INC = '${BF_GETTEXT}/include'
BF_GETTEXT_LIB = 'intl iconv'
BF_GETTEXT_LIBPATH = '${BF_GETTEXT}/lib'

WITH_BF_GAMEENGINE=False
WITH_BF_PLAYER = False

WITH_BF_BULLET = True
BF_BULLET = '#extern/bullet2/src'
BF_BULLET_INC = '${BF_BULLET}'
BF_BULLET_LIB = 'extern_bullet'

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
BF_FREETYPE = '/usr/X11R6'
BF_FREETYPE_INC = '${BF_FREETYPE}/include ${BF_FREETYPE}/include/freetype2'
BF_FREETYPE_LIB = 'freetype'
BF_FREETYPE_LIBPATH = '${BF_FREETYPE}/lib'

WITH_BF_QUICKTIME = False # -DWITH_QUICKTIME

WITH_BF_ICONV = False
BF_ICONV = LIBDIR + "/iconv"
BF_ICONV_INC = '${BF_ICONV}/include'
BF_ICONV_LIB = 'iconv'
BF_ICONV_LIBPATH = '${BF_ICONV}/lib'

# Mesa Libs should go here if your using them as well....
WITH_BF_STATICOPENGL = True
BF_OPENGL = '/usr/X11R6'
BF_OPENGL_INC = '${BF_OPENGL}/include'
BF_OPENGL_LIB = 'GL GLU X11 Xi'
BF_OPENGL_LIBPATH = '${BF_OPENGL}/lib'
BF_OPENGL_LIB_STATIC = '${BF_OPENGL_LIBPATH}/libGL.a ${BF_OPENGL_LIBPATH}/libGLU.a ${BF_OPENGL_LIBPATH}/libXxf86vm.a ${BF_OPENGL_LIBPATH}/libX11.a ${BF_OPENGL_LIBPATH}/libXi.a ${BF_OPENGL_LIBPATH}/libXext.a ${BF_OPENGL_LIBPATH}/libXxf86vm.a'

##
##CC = gcc
##CCC = g++
##ifeq ($CPU),alpha)
##   CFLAGS += -pipe -fPIC -funsigned-char -fno-strict-aliasing -mieee

CFLAGS = ['-pipe','-fPIC','-funsigned-char','-fno-strict-aliasing']

CPPFLAGS = []
CCFLAGS = ['-pipe','-fPIC','-funsigned-char','-fno-strict-aliasing']
CXXFLAGS = ['-pipe','-fPIC','-funsigned-char','-fno-strict-aliasing']
REL_CFLAGS = ['-O2']
REL_CCFLAGS = ['-O2']
##BF_DEPEND = True
##
##AR = ar
##ARFLAGS = ruv
##ARFLAGSQUIET = ru
##
CC = 'gcc'
CXX = 'g++'
C_WARN = ['-Wdeclaration-after-statement']

CC_WARN = ['-Wall']

##FIX_STUBS_WARNINGS = -Wno-unused

LLIBS = ['m', 'stdc++', 'pthread', 'util']
##LOPTS = --dynamic
##DYNLDFLAGS = -shared $(LDFLAGS)

BF_PROFILE = False
BF_PROFILE_CCFLAGS = ['-pg','-g']
BF_PROFILE_LINKFLAGS = ['-pg']

BF_DEBUG = False
BF_DEBUG_CCFLAGS = ['-g']

BF_BUILDDIR='../build/openbsd3'
BF_INSTALLDIR='../install/openbsd3'
BF_DOCDIR='../install/doc'
