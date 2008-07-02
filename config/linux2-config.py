LCGDIR = '../lib/linux2'
LIBDIR = "${LCGDIR}"

WITH_BF_VERSE = 'false'
BF_VERSE_INCLUDE = "#extern/verse/dist"

BF_PYTHON = '/usr'
BF_PYTHON_VERSION = '2.5'
BF_PYTHON_INC = '${BF_PYTHON}/include/python${BF_PYTHON_VERSION}'
BF_PYTHON_BINARY = '${BF_PYTHON}/bin/python${BF_PYTHON_VERSION}'
BF_PYTHON_LIB = 'python${BF_PYTHON_VERSION}' #BF_PYTHON+'/lib/python'+BF_PYTHON_VERSION+'/config/libpython'+BF_PYTHON_VERSION+'.a'
BF_PYTHON_LINKFLAGS = ['-Xlinker', '-export-dynamic']

WITH_BF_OPENAL = 'true'
BF_OPENAL = '/usr'
BF_OPENAL_INC = '${BF_OPENAL}/include'
BF_OPENAL_LIB = 'openal'
# some distros have a separate libalut
# if you get linker complaints, you need to uncomment the line below
# BF_OPENAL_LIB = 'openal alut'  

WITH_BF_SDL = 'true'
BF_SDL = '/usr' #$(shell sdl-config --prefix)
BF_SDL_INC = '${BF_SDL}/include/SDL' #$(shell $(BF_SDL)/bin/sdl-config --cflags)
BF_SDL_LIB = 'SDL' #BF_SDL #$(shell $(BF_SDL)/bin/sdl-config --libs) -lSDL_mixer

WITH_BF_FMOD = 'false'
BF_FMOD = LIBDIR + '/fmod'

WITH_BF_OPENEXR = 'true'
BF_OPENEXR = '/usr'
# when compiling with your own openexr lib you might need to set...
# BF_OPENEXR_INC = '${BF_OPENEXR}/include/OpenEXR ${BF_OPENEXR}/include'

BF_OPENEXR_INC = '${BF_OPENEXR}/include/OpenEXR'
BF_OPENEXR_LIB = 'Half IlmImf Iex Imath '
# BF_OPENEXR_LIBPATH = '${BF_OPENEXR}/lib'

WITH_BF_DDS = 'true'

WITH_BF_JPEG = 'true'
BF_JPEG = '/usr'
BF_JPEG_INC = '${BF_JPEG}/include'
BF_JPEG_LIB = 'jpeg'

WITH_BF_PNG = 'true'
BF_PNG = '/usr'
BF_PNG_INC = '${BF_PNG}/include'
BF_PNG_LIB = 'png'

BF_TIFF = '/usr'
BF_TIFF_INC = '${BF_TIFF}/include'

WITH_BF_ZLIB = 'true'
BF_ZLIB = '/usr'
BF_ZLIB_INC = '${BF_ZLIB}/include'
BF_ZLIB_LIB = 'z'

WITH_BF_INTERNATIONAL = 'true'

BF_GETTEXT = '/usr'
BF_GETTEXT_INC = '${BF_GETTEXT}/include'
BF_GETTEXT_LIB = 'gettextlib'
BF_GETTEXT_LIBPATH = '${BF_GETTEXT}/lib'

WITH_BF_FTGL = 'true'
BF_FTGL = '#extern/bFTGL'
BF_FTGL_INC = '${BF_FTGL}/include'
BF_FTGL_LIB = 'extern_ftgl'

WITH_BF_GAMEENGINE='false'

WITH_BF_ODE = 'false'
BF_ODE = LIBDIR + '/ode'
BF_ODE_INC = BF_ODE + '/include'
BF_ODE_LIB = BF_ODE + '/lib/libode.a'

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
BF_FREETYPE = '/usr'
BF_FREETYPE_INC = '${BF_FREETYPE}/include ${BF_FREETYPE}/include/freetype2'
BF_FREETYPE_LIB = 'freetype'

WITH_BF_QUICKTIME = 'false' # -DWITH_QUICKTIME
BF_QUICKTIME = '/usr/local'
BF_QUICKTIME_INC = '${BF_QUICKTIME}/include'

WITH_BF_ICONV = 'false'
BF_ICONV = LIBDIR + "/iconv"
BF_ICONV_INC = '${BF_ICONV}/include'
BF_ICONV_LIB = 'iconv'
BF_ICONV_LIBPATH = '${BF_ICONV}/lib'

WITH_BF_BINRELOC = 'true'

# enable ffmpeg  support
WITH_BF_FFMPEG = 'true'  # -DWITH_FFMPEG
BF_FFMPEG = '#extern/ffmpeg'
BF_FFMPEG_LIB = ''
# Uncomment the following two lines to use system's ffmpeg
# BF_FFMPEG = '/usr'
# BF_FFMPEG_LIB = 'avformat avcodec swscale avutil'
BF_FFMPEG_INC = '${BF_FFMPEG}/include'
BF_FFMPEG_LIBPATH='${BF_FFMPEG}/lib'

WITH_BF_OPENJPEG = 'true' 
BF_OPENJPEG = '#extern/libopenjpeg'
BF_OPENJPEG_LIB = ''
# Uncomment the following two lines to use system's ffmpeg
# BF_FFMPEG = '/usr'
# BF_FFMPEG_LIB = 'avformat avcodec swscale avutil'
BF_OPENJPEG_INC = '${BF_OPENJPEG}/include'
BF_OPENJPEG_LIBPATH='${BF_OPENJPEG}/lib'

WITH_BF_REDCODE = 'true'  
BF_REDCODE = '#extern/libredcode'
BF_REDCODE_LIB = ''
# Uncomment the following two lines to use system's ffmpeg
# BF_FFMPEG = '/usr'
# BF_FFMPEG_LIB = 'avformat avcodec swscale avutil'
BF_REDCODE_INC = '${BF_REDCODE}/include'
BF_REDCODE_LIBPATH='${BF_REDCODE}/lib'

# Mesa Libs should go here if your using them as well....
WITH_BF_STATICOPENGL = 'false'
BF_OPENGL = '/usr'
BF_OPENGL_INC = '${BF_OPENGL}/include'
BF_OPENGL_LIB = 'GL GLU X11 Xi'
BF_OPENGL_LIBPATH = '/usr/X11R6/lib'
BF_OPENGL_LIB_STATIC = '${BF_OPENGL}/libGL.a ${BF_OPENGL}/libGLU.a ${BF_OPENGL}/libXxf86vm.a ${BF_OPENGL}/libX11.a ${BF_OPENGL}/libXi.a ${BF_OPENGL}/libXext.a ${BF_OPENGL}/libXxf86vm.a'

##
CC = 'gcc'
CXX = 'g++'
##ifeq ($CPU),alpha)
##   CFLAGS += -pipe -fPIC -funsigned-char -fno-strict-aliasing -mieee

CCFLAGS = ['-pipe','-fPIC','-funsigned-char','-fno-strict-aliasing']

CPPFLAGS = ['-DXP_UNIX']
CXXFLAGS = ['-pipe','-fPIC','-funsigned-char','-fno-strict-aliasing']
REL_CFLAGS = ['-O2']
REL_CCFLAGS = ['-O2']
##BF_DEPEND = 'true'
##
##AR = ar
##ARFLAGS = ruv
##ARFLAGSQUIET = ru
##
C_WARN = '-Wall -Wno-char-subscripts -Wdeclaration-after-statement'

CC_WARN = '-Wall'

##FIX_STUBS_WARNINGS = -Wno-unused

LLIBS = 'util c m dl pthread stdc++'
##LOPTS = --dynamic
##DYNLDFLAGS = -shared $(LDFLAGS)

BF_PROFILE_FLAGS = ['-pg','-g']
BF_PROFILE = 'false'

BF_DEBUG = 'false'
BF_DEBUG_FLAGS = '-g'

BF_BUILDDIR = '../build/linux2'
BF_INSTALLDIR='../install/linux2'


#Link against pthread
PLATFORM_LINKFLAGS = ['-pthread']

