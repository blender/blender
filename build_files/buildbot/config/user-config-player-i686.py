BF_BUILDDIR = '../blender-build/linux-glibc27-i686'
BF_INSTALLDIR = '../blender-install/linux-glibc27-i686'

# Python configuration
BF_PYTHON_VERSION = '3.2'
BF_PYTHON_ABI_FLAGS = 'mu'
BF_PYTHON = '/opt/python3.2'

WITH_BF_STATICPYTHON = True

# OpenCollada configuration
WITH_BF_COLLADA = False

# FFMPEG configuration
WITH_BF_FFMPEG = False

# Don't depend on system's libstdc++
WITH_BF_STATICCXX = True
BF_CXX_LIB_STATIC = '/usr/lib/gcc/i486-linux-gnu/4.3.2/libstdc++.a'

WITH_BF_OPENAL = True
WITH_BF_STATICOPENAL = True
BF_OPENAL_LIB_STATIC = '/opt/openal/lib/libopenal.a'

WITH_BF_GETTEXT_STATIC = True
BF_FREETYPE_LIB_STATIC = True

WITH_BF_OPENEXR = False
WITH_BF_STATICOPENEXR = True

WITH_BF_TIFF = False
WITH_BF_STATICTIFF = True
BF_TIFF_LIB_STATIC = '${BF_TIFF}/lib/libtiff.a'

WITH_BF_JPEG = True
BF_JPEG_LIB = 'libjpeg'
BF_JPEG_LIBPATH = '/home/sources/staticlibs/lib32'

WITH_BF_PNG = True
BF_PNG_LIB = 'libpng'
BF_PNG_LIBPATH = '/home/sources/staticlibs/lib32'

WITH_BF_STATICLIBSAMPLERATE = True

WITH_BF_ZLIB = True
WITH_BF_STATICZLIB = True
BF_ZLIB_LIB_STATIC = '${BF_ZLIB}/lib/libz.a'

WITH_BF_SDL = True
WITH_BF_OGG = False

WITH_BF_OPENMP = True

WITH_BF_GAMEENGINE = True
WITH_BF_BULLET = True

# Do not build blender when building blenderplayer
WITH_BF_NOBLENDER = True
WITH_BF_PLAYER = True

# Compilation and optimization
BF_DEBUG = False
REL_CFLAGS = ['-O2']
REL_CCFLAGS = ['-O2']
PLATFORM_LINKFLAGS = ['-L/home/sources/staticlibs/lib32']
