LCGDIR = '#../lib/windows'
LIBDIR = '${LCGDIR}'

WITH_BF_VERSE = 'false'
BF_VERSE_INCLUDE = "#extern/verse/dist"

# enable ffmpeg  support
WITH_BF_FFMPEG = 'false'  # -DWITH_FFMPEG
BF_FFMPEG = LIBDIR +'/ffmpeg'
BF_FFMPEG_INC = '${BF_FFMPEG}/include'
BF_FFMPEG_LIBPATH='${BF_FFMPEG}/lib'
BF_FFMPEG_LIB = 'avformat-52.lib avcodec-51.lib avdevice-52.lib avutil-49.lib swscale-0.lib'

BF_PYTHON = LIBDIR + '/python'
BF_PYTHON_VERSION = '2.5'
BF_PYTHON_INC = '${BF_PYTHON}/include/python${BF_PYTHON_VERSION}'
BF_PYTHON_BINARY = 'python'
BF_PYTHON_LIB = 'python25'
BF_PYTHON_LIBPATH = '${BF_PYTHON}/lib'

WITH_BF_OPENAL = 'true'
BF_OPENAL = LIBDIR + '/openal'
BF_OPENAL_INC = '${BF_OPENAL}/include ${BF_OPENAL}/include/AL '
BF_OPENAL_LIB = 'dxguid openal_static'
BF_OPENAL_LIBPATH = '${BF_OPENAL}/lib'

WITH_BF_ICONV = 'true'
BF_ICONV = LIBDIR + '/iconv'
BF_ICONV_INC = '${BF_ICONV}/include'
BF_ICONV_LIB = 'iconv'
BF_ICONV_LIBPATH = '${BF_ICONV}/lib'

WITH_BF_SDL = 'true'
BF_SDL = LIBDIR + '/sdl'
BF_SDL_INC = '${BF_SDL}/include'
BF_SDL_LIB = 'SDL.lib'
BF_SDL_LIBPATH = '${BF_SDL}/lib'

BF_PTHREADS = LIBDIR + '/pthreads'
BF_PTHREADS_INC = '${BF_PTHREADS}/include'
BF_PTHREADS_LIB = 'pthreadVC2'
BF_PTHREADS_LIBPATH = '${BF_PTHREADS}/lib'

WITH_BF_FMOD = 'false'
BF_FMOD = LIBDIR + '/fmod'

WITH_BF_OPENEXR = 'true'
BF_OPENEXR = LIBDIR + '/openexr'
BF_OPENEXR_INC = '${BF_OPENEXR}/include ${BF_OPENEXR}/include/IlmImf ${BF_OPENEXR}/include/Iex ${BF_OPENEXR}/include/Imath '
BF_OPENEXR_LIB = ' Iex Half IlmImf Imath IlmThread '
BF_OPENEXR_LIBPATH = '${BF_OPENEXR}/lib_msvc'

WITH_BF_DDS = 'true'

WITH_BF_JPEG = 'true'
BF_JPEG = LIBDIR + '/jpeg'
BF_JPEG_INC = '${BF_JPEG}/include'
BF_JPEG_LIB = 'libjpeg'
BF_JPEG_LIBPATH = '${BF_JPEG}/lib'

WITH_BF_PNG = 'true'
BF_PNG = LIBDIR + '/png'
BF_PNG_INC = '${BF_PNG}/include'
BF_PNG_LIB = 'libpng_st'
BF_PNG_LIBPATH = '${BF_PNG}/lib'

BF_TIFF = LIBDIR + '/tiff'
BF_TIFF_INC = '${BF_TIFF}/include'

WITH_BF_ZLIB = 'true'
BF_ZLIB = LIBDIR + '/zlib'
BF_ZLIB_INC = '${BF_ZLIB}/include'
BF_ZLIB_LIB = 'libz'
BF_ZLIB_LIBPATH = '${BF_ZLIB}/lib'

WITH_BF_INTERNATIONAL = 'true'

BF_GETTEXT = LIBDIR + '/gettext'
BF_GETTEXT_INC = '${BF_GETTEXT}/include'
BF_GETTEXT_LIB = 'gnu_gettext'
BF_GETTEXT_LIBPATH = '${BF_GETTEXT}/lib'

WITH_BF_FTGL = 'true'
BF_FTGL = LIBDIR + '/ftgl'
BF_FTGL_INC = '${BF_FTGL}/include'
BF_FTGL_LIB = 'extern_ftgl'

WITH_BF_GAMEENGINE = 'false'

WITH_BF_ODE = 'true'
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

BF_WINTAB = LIBDIR + '/wintab'
BF_WINTAB_INC = '${BF_WINTAB}/INCLUDE'

WITH_BF_YAFRAY = 'true'

WITH_BF_BINRELOC = 'false'

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
BF_FREETYPE_LIB = 'freetype2ST'
BF_FREETYPE_LIBPATH = '${BF_FREETYPE}/lib'

WITH_BF_QUICKTIME = 'true' # -DWITH_QUICKTIME
BF_QUICKTIME = LIBDIR + '/QTDevWin'
BF_QUICKTIME_INC = '${BF_QUICKTIME}/CIncludes'
BF_QUICKTIME_LIB = 'qtmlClient'
BF_QUICKTIME_LIBPATH = '${BF_QUICKTIME}/Libraries'

WITH_BF_STATICOPENGL = 'false'
BF_OPENGL_INC = '${BF_OPENGL}/include'
BF_OPENGL_LIBINC = '${BF_OPENGL}/lib'
BF_OPENGL_LIB = 'opengl32 glu32'
BF_OPENGL_LIB_STATIC = [ '${BF_OPENGL}/lib/libGL.a', '${BF_OPENGL}/lib/libGLU.a',
                         '${BF_OPENGL}/lib/libXmu.a', '${BF_OPENGL}/lib/libXext.a',
                         '${BF_OPENGL}/lib/libX11.a', '${BF_OPENGL}/lib/libXi.a' ]
CC = 'cl.exe'
CXX = 'cl.exe'

CCFLAGS = ['/nologo', '/Og', '/Ot', '/Ob1', '/Op', '/G6','/EHsc', '/J', '/W3', '/Gd', '/MT']

BF_DEBUG_FLAGS = ['/Zi', '/FR${TARGET}.sbr']

CPPFLAGS = ['-DWIN32','-D_CONSOLE', '-D_LIB', '-DFTGL_LIBRARY_STATIC', '-D_CRT_SECURE_NO_DEPRECATE']
REL_CFLAGS = ['-O2', '-DNDEBUG']
REL_CCFLAGS = ['-O2', '-DNDEBUG']
C_WARN = []
CC_WARN = []

LLIBS = 'ws2_32 vfw32 winmm kernel32 user32 gdi32 comdlg32 advapi32 shell32 ole32 oleaut32 uuid'

PLATFORM_LINKFLAGS = '''
                        /SUBSYSTEM:CONSOLE 
                        /MACHINE:IX86 
                        /ENTRY:mainCRTStartup 
                        /INCREMENTAL:NO 
                        /NODEFAULTLIB:"msvcprt.lib" 
                        /NODEFAULTLIB:"glut32.lib" 
                        /NODEFAULTLIB:"libc.lib"
                        /NODEFAULTLIB:"libcd.lib"
                        /NODEFAULTLIB:"libcpd.lib" 
                        /NODEFAULTLIB:"libcp.lib" 
                        /NODEFAULTLIB:"libcmtd.lib"
                    '''
                        
BF_BUILDDIR = '..\\build\\win32-vc'
BF_INSTALLDIR='..\\install\\win32-vc'
