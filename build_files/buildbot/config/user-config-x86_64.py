BF_BUILDDIR = '../blender-build/linux-glibc27-x86_64'
BF_INSTALLDIR = '../blender-install/linux-glibc27-x86_64'
BF_NUMJOBS = 2

# Python configuration
BF_PYTHON_VERSION = '3.2'
BF_PYTHON_ABI_FLAGS = 'mu'
BF_PYTHON = '/opt/python3.2'

WITH_BF_STATICPYTHON = True

# OpenCollada configuration
WITH_BF_COLLADA = True
BF_OPENCOLLADA = '/opt/opencollada'
BF_OPENCOLLADA_INC = '${BF_OPENCOLLADA}/include'
BF_OPENCOLLADA_LIB = 'OpenCOLLADAStreamWriter OpenCOLLADASaxFrameworkLoader OpenCOLLADAFramework OpenCOLLADABaseUtils GeneratedSaxParser UTF MathMLSolver buffer ftoa libxml2-static libexpat-static libpcre-static'
BF_OPENCOLLADA_LIBPATH = '${BF_OPENCOLLADA}/lib /home/sources/staticlibs/lib64'
BF_PCRE_LIB = ''
BF_EXPAT_LIB = ''

# FFMPEG configuration
WITH_BF_FFMPEG = True
WITH_BF_STATICFFMPEG = True

BF_FFMPEG = '/home/sources/staticlibs/ffmpeg'
BF_FFMPEG_LIBPATH = '${BF_FFMPEG}/lib64'
BF_FFMPEG_LIB_STATIC = '${BF_FFMPEG_LIBPATH}/libavformat.a ${BF_FFMPEG_LIBPATH}/libswscale.a ' + \
    '${BF_FFMPEG_LIBPATH}/libavcodec.a ${BF_FFMPEG_LIBPATH}/libavdevice.a ${BF_FFMPEG_LIBPATH}/libavutil.a ' + \
    '${BF_FFMPEG_LIBPATH}/libxvidcore.a ${BF_FFMPEG_LIBPATH}/libx264.a ${BF_FFMPEG_LIBPATH}/libmp3lame.a ' + \
    '${BF_FFMPEG_LIBPATH}/libvpx.a ${BF_FFMPEG_LIBPATH}/libvorbis.a ${BF_FFMPEG_LIBPATH}/libogg.a ' + \
    '${BF_FFMPEG_LIBPATH}/libvorbisenc.a ${BF_FFMPEG_LIBPATH}/libtheora.a ' + \
    '${BF_FFMPEG_LIBPATH}/libschroedinger-1.0.a ${BF_FFMPEG_LIBPATH}/liborc-0.4.a ${BF_FFMPEG_LIBPATH}/libdirac_encoder.a'

# Don't depend on system's libstdc++
WITH_BF_STATICCXX = True
BF_CXX_LIB_STATIC = '/usr/lib/gcc/x86_64-linux-gnu/4.3.2/libstdc++.a'

WITH_BF_OPENAL = True
WITH_BF_STATICOPENAL = True
BF_OPENAL_LIB_STATIC = '/opt/openal/lib/libopenal.a'

WITH_BF_GETTEXT_STATIC = True
BF_FREETYPE_LIB_STATIC = True

WITH_BF_OPENEXR = True
WITH_BF_STATICOPENEXR = True

WITH_BF_TIFF = True
WITH_BF_STATICTIFF = True
BF_TIFF_LIB_STATIC = '${BF_TIFF}/lib/libtiff.a'

WITH_BF_JPEG = True
BF_JPEG_LIB = 'libjpeg'
BF_JPEG_LIBPATH = '/home/sources/staticlibs/lib64'

WITH_BF_PNG = True
BF_PNG_LIB = 'libpng'
BF_PNG_LIBPATH = '/home/sources/staticlibs/lib64'

WITH_BF_STATICLIBSAMPLERATE = True

WITH_BF_ZLIB = True
WITH_BF_STATICZLIB = True
BF_ZLIB_LIB_STATIC = '${BF_ZLIB}/lib/libz.a'

WITH_BF_SDL = True
WITH_BF_OGG = True

WITH_BF_OPENMP = True

WITH_BF_GAMEENGINE = True
WITH_BF_BULLET = True

# Blender player (would be enabled in it's own config)
WITH_BF_PLAYER = False

# Use jemalloc memory manager
WITH_BF_JEMALLOC = True
WITH_BF_STATICJEMALLOC = True
BF_JEMALLOC = '/home/sources/staticlibs/jemalloc'
BF_JEMALLOC_LIBPATH = '${BF_JEMALLOC}/lib64'

# Compilation and optimization
BF_DEBUG = False
REL_CFLAGS = ['-O2']
REL_CCFLAGS = ['-O2']
PLATFORM_LINKFLAGS = ['-L/home/sources/staticlibs/lib64']
