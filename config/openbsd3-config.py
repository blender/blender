LCGDIR = '../lib/openbsd3'
LIBDIR = '${LCGDIR}'

BF_PYTHON = '/usr/local'
BF_PYTHON_VERSION = '2.5'
BF_PYTHON_INC = '${BF_PYTHON}/include/python${BF_PYTHON_VERSION}'
BF_PYTHON_BINARY = '${BF_PYTHON}/bin/python${BF_PYTHON_VERSION}'
BF_PYTHON_LIB = 'python${BF_PYTHON_VERSION}'
BF_PYTHON_LIBPATH = '${BF_PYTHON}/lib/python${BF_PYTHON_VERSION}/config'

WITH_BF_OPENAL = 'false'
#BF_OPENAL = LIBDIR + '/openal'
#BF_OPENAL_INC = '${BF_OPENAL}/include'
#BF_OPENAL_LIB = 'openal'
#BF_OPENAL_LIBPATH = '${BF_OPENAL}/lib'

WITH_BF_SDL = 'true'
BF_SDL = '/usr/local' #$(shell sdl-config --prefix)
BF_SDL_INC = '${BF_SDL}/include/SDL' #$(shell $(BF_SDL)/bin/sdl-config --cflags)
BF_SDL_LIB = 'SDL' #BF_SDL #$(shell $(BF_SDL)/bin/sdl-config --libs) -lSDL_mixer
BF_SDL_LIBPATH = '${BF_SDL}/lib'

WITH_BF_FMOD = 'false'
BF_FMOD = LIBDIR + '/fmod'

WITH_BF_OPENEXR = 'false'
BF_OPENEXR = '/usr/local'
BF_OPENEXR_INC = '${BF_OPENEXR}/include/OpenEXR'
BF_OPENEXR_LIB = 'Half IlmImf Iex Imath '

WITH_BF_DDS = 'true'

WITH_BF_JPEG = 'true'
BF_JPEG = '/usr/local'
BF_JPEG_INC = '${BF_JPEG}/include'
BF_JPEG_LIB = 'jpeg'
BF_JPEG_LIBPATH = '${BF_JPEG}/lib'

WITH_BF_PNG = 'true'
BF_PNG = '/usr/local'
BF_PNG_INC = '${BF_PNG}/include/libpng'
BF_PNG_LIB = 'png'
BF_PNG_LIBPATH = '${BF_PNG}/lib'

BF_TIFF = '/usr/local'
BF_TIFF_INC = '${BF_TIFF}/include'

WITH_BF_ZLIB = 'true'
BF_ZLIB = '/usr/local'
BF_ZLIB_INC = '${BF_ZLIB}/include'
BF_ZLIB_LIB = 'z'

WITH_BF_INTERNATIONAL = 'true'

BF_GETTEXT = '/usr/local'
BF_GETTEXT_INC = '${BF_GETTEXT}/include'
BF_GETTEXT_LIB = 'intl iconv'
BF_GETTEXT_LIBPATH = '${BF_GETTEXT}/lib'

WITH_BF_FTGL = 'true'
BF_FTGL = '#extern/bFTGL'
BF_FTGL_INC = '${BF_FTGL}/include'
BF_FTGL_LIB = 'extern_ftgl'

WITH_BF_GAMEENGINE='false'

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
BF_FREETYPE = '/usr/X11R6'
BF_FREETYPE_INC = '${BF_FREETYPE}/include ${BF_FREETYPE}/include/freetype2'
BF_FREETYPE_LIB = 'freetype'
BF_FREETYPE_LIBPATH = '${BF_FREETYPE}/lib'

WITH_BF_QUICKTIME = 'false' # -DWITH_QUICKTIME

WITH_BF_ICONV = 'false'
BF_ICONV = LIBDIR + "/iconv"
BF_ICONV_INC = '${BF_ICONV}/include'
BF_ICONV_LIB = 'iconv'
BF_ICONV_LIBPATH = '${BF_ICONV}/lib'

# Mesa Libs should go here if your using them as well....
WITH_BF_STATICOPENGL = 'true'
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
##BF_DEPEND = 'true'
##
##AR = ar
##ARFLAGS = ruv
##ARFLAGSQUIET = ru
##
CC = 'gcc'
CXX = 'g++'
C_WARN = '-Wall -Wdeclaration-after-statement'

CC_WARN = '-Wall'

##FIX_STUBS_WARNINGS = -Wno-unused

LLIBS = 'm stdc++ pthread util'
##LOPTS = --dynamic
##DYNLDFLAGS = -shared $(LDFLAGS)

BF_PROFILE_FLAGS = ' -pg -g '
BF_PROFILE = 'false'

BF_DEBUG = 'false'
BF_DEBUG_FLAGS = '-g'

BF_BUILDDIR='../build/openbsd3'
BF_INSTALLDIR='../install/openbsd3'
