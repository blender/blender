import os

LCGDIR = os.getcwd()+"/../lib/aix-4.3-ppc"
LIBDIR = LCGDIR
print LCGDIR

WITH_BF_VERSE = 'false'
BF_VERSE_INCLUDE = "#extern/verse/dist"

BF_PYTHON = LCGDIR+'/python'
BF_PYTHON_VERSION = '3.1'
WITH_BF_STATICPYTHON = 'true'
BF_PYTHON_INC = '${BF_PYTHON}/include/python${BF_PYTHON_VERSION}'
BF_PYTHON_BINARY = '${BF_PYTHON}/bin/python${BF_PYTHON_VERSION}'
BF_PYTHON_LIB = 'python${BF_PYTHON_VERSION}' #BF_PYTHON+'/lib/python'+BF_PYTHON_VERSION+'/config/libpython'+BF_PYTHON_VERSION+'.a'
BF_PYTHON_LINKFLAGS = ['-Xlinker', '-export-dynamic']
BF_PYTHON_LIB_STATIC = '${BF_PYTHON}/lib/python${BF_PYTHON_VERSION}/config/libpython${BF_PYTHON_VERSION}.a'

WITH_BF_OPENAL = 'false'
WITH_BF_STATICOPENAL = 'false'
BF_OPENAL = LCGDIR+'/openal'
BF_OPENAL_INC = '${BF_OPENAL}/include'
BF_OPENAL_LIB = 'openal'
BF_OPENAL_LIB_STATIC = '${BF_OPENAL}/lib/libopenal.a'
BF_OPENAL_LIBPATH = LIBDIR + '/lib'

BF_CXX = '/usr'
WITH_BF_STATICCXX = 'false'
BF_CXX_LIB_STATIC = '${BF_CXX}/lib/libstdc++.a'

WITH_BF_SDL = 'false'
BF_SDL = LCGDIR+'/sdl' #$(shell sdl-config --prefix)
BF_SDL_INC = '${BF_SDL}/include/SDL' #$(shell $(BF_SDL)/bin/sdl-config --cflags)
BF_SDL_LIB = 'SDL audio iconv charset' #BF_SDL #$(shell $(BF_SDL)/bin/sdl-config --libs) -lSDL_mixer
BF_SDL_LIBPATH = '${BF_SDL}/lib'

WITH_BF_OPENEXR = 'false'
WITH_BF_STATICOPENEXR = 'false'
BF_OPENEXR = '/usr'
# when compiling with your own openexr lib you might need to set...
# BF_OPENEXR_INC = '${BF_OPENEXR}/include/OpenEXR ${BF_OPENEXR}/include'

BF_OPENEXR_INC = '${BF_OPENEXR}/include/OpenEXR'
BF_OPENEXR_LIB = 'Half IlmImf Iex Imath '
BF_OPENEXR_LIB_STATIC = '${BF_OPENEXR}/lib/libHalf.a ${BF_OPENEXR}/lib/libIlmImf.a ${BF_OPENEXR}/lib/libIex.a ${BF_OPENEXR}/lib/libImath.a ${BF_OPENEXR}/lib/libIlmThread.a'
# BF_OPENEXR_LIBPATH = '${BF_OPENEXR}/lib'


WITH_BF_DDS = 'false'

WITH_BF_JPEG = 'false'
BF_JPEG = LCGDIR+'/jpeg'
BF_JPEG_INC = '${BF_JPEG}/include'
BF_JPEG_LIB = 'jpeg'
BF_JPEG_LIBPATH = '${BF_JPEG}/lib'

WITH_BF_PNG = 'false'
BF_PNG = LCGDIR+"/png"
BF_PNG_INC = '${BF_PNG}/include'
BF_PNG_LIB = 'png'
BF_PNG_LIBPATH = '${BF_PNG}/lib'

BF_TIFF = '/usr/nekoware'
BF_TIFF_INC = '${BF_TIFF}/include'

WITH_BF_ZLIB = 'true'
BF_ZLIB = LCGDIR+"/zlib"
BF_ZLIB_INC = '${BF_ZLIB}/include'
BF_ZLIB_LIB = 'z'
BF_ZLIB_LIBPATH = '${BF_ZLIB}/lib'

WITH_BF_INTERNATIONAL = 'false'

BF_GETTEXT = LCGDIR+'/gettext'
BF_GETTEXT_INC = '${BF_GETTEXT}/include'
BF_GETTEXT_LIB = 'gettextpo intl'
BF_GETTEXT_LIBPATH = '${BF_GETTEXT}/lib'

WITH_BF_FTGL = 'false'
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
BF_FREETYPE = LCGDIR+'/freetype'
BF_FREETYPE_INC = '${BF_FREETYPE}/include ${BF_FREETYPE}/include/freetype2'
BF_FREETYPE_LIB = 'freetype'
BF_FREETYPE_LIBPATH = '${BF_FREETYPE}/lib'

WITH_BF_QUICKTIME = 'false' # -DWITH_QUICKTIME
BF_QUICKTIME = '/usr/local'
BF_QUICKTIME_INC = '${BF_QUICKTIME}/include'

WITH_BF_ICONV = 'false'
BF_ICONV = LIBDIR + "/iconv"
BF_ICONV_INC = '${BF_ICONV}/include'
BF_ICONV_LIB = 'iconv charset'
BF_ICONV_LIBPATH = '${BF_ICONV}/lib'

WITH_BF_BINRELOC = 'false'

# enable ffmpeg  support
WITH_BF_FFMPEG = 'false'  # -DWITH_FFMPEG
# Uncomment the following two lines to use system's ffmpeg
BF_FFMPEG = LCGDIR+'/ffmpeg'
BF_FFMPEG_LIB = 'avformat avcodec swscale avutil avdevice faad faac vorbis x264 ogg mp3lame z'
BF_FFMPEG_INC = '${BF_FFMPEG}/include'
BF_FFMPEG_LIBPATH='${BF_FFMPEG}/lib'

# enable ogg, vorbis and theora in ffmpeg
WITH_BF_OGG = 'false'  # -DWITH_OGG 
BF_OGG = '/usr'
BF_OGG_INC = '${BF_OGG}/include'
BF_OGG_LIB = 'ogg vorbis theoraenc theoradec'

WITH_BF_OPENJPEG = 'false' 
BF_OPENJPEG = '#extern/libopenjpeg'
BF_OPENJPEG_LIB = ''
BF_OPENJPEG_INC = '${BF_OPENJPEG}'
BF_OPENJPEG_LIBPATH='${BF_OPENJPEG}/lib'

WITH_BF_REDCODE = 'false'  
BF_REDCODE = '#extern/libredcode'
BF_REDCODE_LIB = ''
BF_REDCODE_INC = '${BF_REDCODE}/include'
BF_REDCODE_LIBPATH='${BF_REDCODE}/lib'

# Mesa Libs should go here if your using them as well....
WITH_BF_STATICOPENGL = 'false'
BF_OPENGL = '/usr'
BF_OPENGL_INC = '${BF_OPENGL}/include'
BF_OPENGL_LIB = 'GL GLU X11 Xi Xext'
BF_OPENGL_LIBPATH = '/usr/X11R6/lib'
BF_OPENGL_LIB_STATIC = '${BF_OPENGL}/libGL.a ${BF_OPENGL}/libGLU.a ${BF_OPENGL}/libXxf86vm.a ${BF_OPENGL}/libX11.a ${BF_OPENGL}/libXi.a ${BF_OPENGL}/libXext.a ${BF_OPENGL}/libXxf86vm.a'


CC = 'gcc'
CXX = 'g++'

CCFLAGS = [ '-pipe', '-funsigned-char', '-fno-strict-aliasing' ]

CPPFLAGS = [ '-DXP_UNIX', '-DWIN32', '-DFREE_WINDOWS' ]
CXXFLAGS = ['-pipe', '-funsigned-char', '-fno-strict-aliasing' ]
REL_CFLAGS = [ '-O2' ]
REL_CCFLAGS = [ '-O2' ]
C_WARN = [ '-Wall' , '-Wno-char-subscripts', '-Wdeclaration-after-statement' ]

CC_WARN = [ '-Wall' ]



##BF_DEPEND = 'true'
##
##AR = ar
##ARFLAGS = ruv
##ARFLAGSQUIET = ru
##

##FIX_STUBS_WARNINGS = -Wno-unused

LLIBS = 'c m dl pthread dmedia movie'
##LOPTS = --dynamic
##DYNLDFLAGS = -shared $(LDFLAGS)

BF_PROFILE_FLAGS = ['-pg','-g']
BF_PROFILE = 'false'

BF_DEBUG = 'false'
BF_DEBUG_FLAGS = '-g'

BF_BUILDDIR = '../build/aix4'
BF_INSTALLDIR='../install/aix4'
BF_DOCDIR='../install/doc'

#Link against pthread
LDIRS = []
LDIRS.append(BF_FREETYPE_LIBPATH)
LDIRS.append(BF_PNG_LIBPATH)
LDIRS.append(BF_ZLIB_LIBPATH)
LDIRS.append(BF_SDL_LIBPATH)
LDIRS.append(BF_OPENAL_LIBPATH)
LDIRS.append(BF_ICONV_LIBPATH)

PLATFORM_LINKFLAGS = []
for x in LDIRS:
    PLATFORM_LINKFLAGS.append("-L"+x)
    
PLATFORM_LINKFLAGS += ['-L${LCGDIR}/jpeg/lib' , '-L/usr/lib32',  '-n32', '-v', '-no_prelink']
print PLATFORM_LINKFLAGS
LINKFLAGS= PLATFORM_LINKFLAGS
