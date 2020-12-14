# Do NOT use CACHE behavior here, as this has the same effect as defining an option,
# it exposes the setting in main CMake config (which we do not want here).

set(AUDASPACE_STANDALONE FALSE)
set(BUILD_DEMOS FALSE)  # "Build and install demos"
set(SHARED_LIBRARY FALSE)  # "Build Shared Library"
set(WITH_C TRUE)  # "Build C Module"
set(WITH_DOCS FALSE)  # "Build C++ HTML Documentation with Doxygen"
set(WITH_FFMPEG ${WITH_CODEC_FFMPEG})  # "Build With FFMPEG"
set(WITH_FFTW FALSE)  # "Build With FFTW"
set(WITH_LIBSNDFILE ${WITH_CODEC_SNDFILE})  # "Build With LibSndFile"
set(SEPARATE_C FALSE)  # "Build C Binding as separate library"
set(PLUGIN_FFMPEG FALSE)  # "Build FFMPEG Plugin"
set(PLUGIN_JACK FALSE)  # "Build JACK Plugin"
set(PLUGIN_LIBSNDFILE FALSE)  # "Build LibSndFile Plugin"
set(PLUGIN_OPENAL FALSE)  # "Build OpenAL Plugin"
set(PLUGIN_SDL FALSE)  # "Build SDL Plugin"
set(WITH_PYTHON_MODULE FALSE)  # "Build Python Module"
set(DYNLOAD_JACK ${WITH_JACK_DYNLOAD})  # "Dynamically load JACK"
set(WITH_BINDING_DOCS FALSE)  # "Build C/Python HTML Documentation with Sphinx"
set(DEFAULT_PLUGIN_PATH "plugins")  # "Default plugin installation and loading path."
set(FFMPEG_FOUND ${WITH_CODEC_FFMPEG})
set(JACK_FOUND ${WITH_JACK})
set(LIBSNDFILE_FOUND ${WITH_CODEC_SNDFILE})
set(OPENAL_FOUND ${WITH_OPENAL})
set(PYTHONLIBS_FOUND TRUE)
set(NUMPY_FOUND TRUE)
set(NUMPY_INCLUDE_DIRS ${PYTHON_NUMPY_INCLUDE_DIRS})
set(SDL_FOUND ${WITH_SDL})
