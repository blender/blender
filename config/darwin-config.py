LCGDIR = '#../lib/darwin-6.1-powerpc'
BF_PYTHON = '/System/Library/Frameworks/Python.framework/Versions/'
BF_PYTHON_VERSION = '2.3'
BF_PYTHON_INC = '${BF_PYTHON}${BF_PYTHON_VERSION}/include/python${BF_PYTHON_VERSION}'
BF_PYTHON_BINARY = '${BF_PYTHON}${BF_PYTHON_VERSION}/bin/python${BF_PYTHON_VERSION}'
BF_PYTHON_LIB = ''
BF_PYTHON_LIBPATH = '${BF_PYTHON}${BF_PYTHON_VERSION}/lib/python${BF_PYTHON_VERSION}/config'
BF_PYTHON_LINKFLAGS = '-u __dummy -u _PyMac_Error -framework System -framework Python'

WITH_BF_OPENAL = 'true'
BF_OPENAL = LCGDIR + '/openal'
BF_OPENAL_INC = '${BF_OPENAL}/include'
BF_OPENAL_LIB = 'openal'
BF_OPENAL_LIBPATH = '${BF_OPENAL}/lib'

WITH_BF_SDL = 'true'
BF_SDL = LCGDIR + '/sdl' #$(shell sdl-config --prefix)
BF_SDL_INC = '${BF_SDL}/include' #$(shell $(BF_SDL)/bin/sdl-config --cflags)
BF_SDL_LIB = 'SDL' #BF_SDL #$(shell $(BF_SDL)/bin/sdl-config --libs) -lSDL_mixer
BF_SDL_LIBPATH = '${BF_SDL}/lib'

WITH_BF_FMOD = 'false'
BF_FMOD = LCGDIR + '/fmod'

WITH_BF_OPENEXR = 'true'
BF_OPENEXR = '/usr/local'
BF_OPENEXR_INC = '${BF_OPENEXR}/include/OpenEXR'
BF_OPENEXR_LIB = ' Iex Half IlmImf Imath '


WITH_BF_JPEG = 'true'
BF_JPEG = LCGDIR + '/jpeg'
BF_JPEG_INC = '${BF_JPEG}/include'
BF_JPEG_LIB = 'jpeg'
BF_JPEG_LIBPATH = '${BF_JPEG}/lib'

WITH_BF_PNG = 'true'
BF_PNG = LCGDIR + '/png'
BF_PNG_INC = '${BF_PNG}/include'
BF_PNG_LIB = 'png'
BF_PNG_LIBPATH = '${BF_PNG}/lib'

BF_TIFF = LCGDIR + '/tiff'
BF_TIFF_INC = '${BF_TIFF}/include'

WITH_BF_ZLIB = 'true'
BF_ZLIB = '/usr'
BF_ZLIB_INC = '${BF_ZLIB}/include'
BF_ZLIB_LIB = 'z'

WITH_BF_INTERNATIONAL = 'true'

BF_GETTEXT = LCGDIR + '/gettext'
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
BF_ODE = LCGDIR + '/ode'
BF_ODE_INC = '${BF_ODE}/include'
BF_ODE_LIB = '${BF_ODE}/lib/libode.a'

WITH_BF_BULLET = 'true'
BF_BULLET = '#extern/bullet'
BF_BULLET_INC = '${BF_BULLET}/LinearMath ${BF_BULLET}/BulletDynamics ${BF_BULLET}/Bullet'
BF_BULLET_LIB = 'extern_bullet'

BF_SOLID = '#extern/solid'
BF_SOLID_INC = '${BF_SOLID}'
BF_SOLID_LIB = 'extern_solid'

#WITH_BF_NSPR = 'true'
#BF_NSPR = $(LCGDIR)/nspr
#BF_NSPR_INC = -I$(BF_NSPR)/include -I$(BF_NSPR)/include/nspr
#BF_NSPR_LIB = 

# Uncomment the following line to use Mozilla inplace of netscape
#CPPFLAGS += -DMOZ_NOT_NET
# Location of MOZILLA/Netscape header files...
#BF_MOZILLA = $(LCGDIR)/mozilla
#BF_MOZILLA_INC = -I$(BF_MOZILLA)/include/mozilla/nspr -I$(BF_MOZILLA)/include/mozilla -I$(BF_MOZILLA)/include/mozilla/xpcom -I$(BF_MOZILLA)/include/mozilla/idl
#BF_MOZILLA_LIB =
# Will fall back to look in BF_MOZILLA_INC/nspr and BF_MOZILLA_LIB
# if this is not set.
#
# Be paranoid regarding library creation (do not update archives)
#BF_PARANOID = 'true'

# enable freetype2 support for text objects
BF_FREETYPE = LCGDIR + '/freetype'
BF_FREETYPE_INC = '${BF_FREETYPE}/include ${BF_FREETYPE}/include/freetype2'
BF_FREETYPE_LIB = 'freetype'
BF_FREETYPE_LIBPATH = '${BF_FREETYPE}/lib'

WITH_BF_QUICKTIME = 'true' # -DWITH_QUICKTIME

# Mesa Libs should go here if your using them as well....
WITH_BF_STATICOPENGL = 'true'
BF_OPENGL_LIB = 'GL GLU'
BF_OPENGL_LIBPATH = '/System/Library/Frameworks/OpenGL.framework/Libraries'
BF_OPENGL_LINKFLAGS = '-framework OpenGL'

##
##CC = gcc
##CCC = g++
##ifeq ($CPU),alpha)
##   CFLAGS += -pipe -fPIC -funsigned-char -fno-strict-aliasing -mieee

CFLAGS = ['-pipe','-fPIC','-funsigned-char','-fno-strict-aliasing']

CPPFLAGS = []
CCFLAGS = ['-pipe','-fPIC','-funsigned-char','-fno-strict-aliasing']
CXXFLAGS = ['-pipe','-fPIC','-funsigned-char','-fno-strict-aliasing']
PLATFORM_LINKFLAGS = '-fexceptions -framework CoreServices -framework Foundation -framework IOKit -framework AppKit -framework Carbon -framework AGL -framework AudioUnit -framework AudioToolbox -framework CoreAudio -framework QuickTime'
REL_CFLAGS = ['-O2']
REL_CCFLAGS = ['-O2']
##BF_DEPEND = 'true'
##
##AR = ar
##ARFLAGS = ruv
##ARFLAGSQUIET = ru
##
C_WARN = ' -Wall  -Wno-long-double'

CC_WARN = ' -Wall  -Wno-long-double'

##FIX_STUBS_WARNINGS = -Wno-unused

LLIBS = 'stdc++ SystemStubs'
##LOPTS = --dynamic
##DYNLDFLAGS = -shared $(LDFLAGS)

BF_PROFILE_FLAGS = ' -pg -g '
BF_PROFILE = 'false'

BF_DEBUG = 'false'
BF_DEBUG_FLAGS = '-g'

BF_BUILDDIR='../build/darwin'
BF_INSTALLDIR='../install/darwin'
