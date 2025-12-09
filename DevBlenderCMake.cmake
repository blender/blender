# Blender GUI Cycles Render Configuration for Windows
# For: Interactive Blender with GUI for troubleshooting, Cycles rendering, FBX I/O
# Same as headless build but with GUI and mouse input enabled

# ============================================================
# GPU ACCELERATION SETTINGS
# ============================================================

# OptiX SDK location - update version number if using different version
set(OPTIX_ROOT_DIR "C:/ProgramData/NVIDIA Corporation/OptiX SDK 9.0.0" CACHE PATH "")

# CUDA architectures for your target GPUs
# Adjust these based on the GPUs you'll be rendering on
# sm_75 = Tesla T4, RTX 2060-2080 (Turing)
# sm_86 = A10, RTX 3060-3090 (Ampere)  
# sm_89 = RTX 4060-4090 (Ada Lovelace)
# sm_90 = H100 (Hopper)
set(CYCLES_CUDA_BINARIES_ARCH "sm_75;sm_86;sm_89" CACHE STRING "")

# ============================================================
# CORE BUILD CONFIGURATION
# ============================================================

set(WITH_BLENDER ON CACHE BOOL "")  # Build full Blender (vs just Cycles standalone)
set(WITH_HEADLESS OFF CACHE BOOL "") # Enable GUI - for interactive use and troubleshooting
set(WITH_BUILDINFO OFF CACHE BOOL "") # Exclude build date/time for faster development builds

# ============================================================
# PYTHON - REQUIRED FOR CYCLES AND SCRIPTING
# ============================================================

set(WITH_PYTHON ON CACHE BOOL "")  # Enable Python API - REQUIRED for Cycles renderer
set(WITH_PYTHON_MODULE OFF CACHE BOOL "")  # We're building an executable, not a Python module
set(WITH_PYTHON_INSTALL ON CACHE BOOL "")  # Bundle Python runtime with Blender
set(WITH_PYTHON_NUMPY ON CACHE BOOL "")  # NumPy used by simulation systems
set(WITH_PYTHON_SAFETY OFF CACHE BOOL "")  # Disable API error checking - production builds don't need this overhead
set(WITH_PYTHON_SECURITY OFF CACHE BOOL "")  # Allow .blend files to run embedded scripts

# ============================================================
# RENDERING - CYCLES AND DEPENDENCIES
# ============================================================

set(WITH_CYCLES ON CACHE BOOL "" FORCE)  # FORCE to override any auto-disable
set(WITH_BOOST ON CACHE BOOL "" FORCE)  # Required by Cycles for various utilities
set(WITH_TBB ON CACHE BOOL "" FORCE)  # Threading Building Blocks - required for Cycles multi-threading
set(WITH_LLVM ON CACHE BOOL "")  # Required for Cycles OSL shader compilation

# Photorealistic rendering features
set(WITH_OPENCOLORIO ON CACHE BOOL "")  # Color management - essential for accurate color workflows (ACES, etc)
set(WITH_OPENSUBDIV ON CACHE BOOL "")  # GPU-accelerated subdivision surfaces for smooth organic models
set(WITH_OPENVDB ON CACHE BOOL "")  # Volume rendering - smoke, fire, clouds, atmospheric effects
set(WITH_OPENIMAGEDENOISE ON CACHE BOOL "")  # AI-powered denoising - dramatically improves render quality at lower samples
set(WITH_COMPOSITOR_CPU ON CACHE BOOL "")  # Node-based compositing for post-processing and multi-layer renders

set(WITH_FREESTYLE OFF CACHE BOOL "")  # Non-photorealistic line rendering - not needed for photorealistic work

# ============================================================
# CYCLES GPU ACCELERATION
# ============================================================

set(WITH_CYCLES_DEVICE_CUDA ON CACHE BOOL "")  # NVIDIA CUDA support - standard GPU rendering
set(WITH_CYCLES_DEVICE_OPTIX ON CACHE BOOL "")  # NVIDIA OptiX - hardware ray tracing for RTX cards (much faster)
set(WITH_CYCLES_CUDA_BINARIES ON CACHE BOOL "")  # Pre-compile CUDA kernels at build time

# Disable other GPU backends we're not using
set(WITH_CYCLES_DEVICE_HIP OFF CACHE BOOL "")  # AMD GPU support - disable if not using AMD
set(WITH_CYCLES_DEVICE_METAL OFF CACHE BOOL "")  # Apple Metal - macOS only
set(WITH_CYCLES_DEVICE_ONEAPI OFF CACHE BOOL "")  # Intel GPU support - disable if not using Intel Arc

# ============================================================
# WINDOWING/GUI - ENABLE FOR INTERACTIVE USE
# ============================================================

set(WITH_GHOST_SDL ON CACHE BOOL "")  # SDL windowing - enables GUI interface for troubleshooting

# ============================================================
# INPUT DEVICES - ENABLE 3D MOUSE
# ============================================================

set(WITH_INPUT_NDOF ON CACHE BOOL "")  # 3D mouse support (SpaceNavigator, 3Dconnexion devices)
set(WITH_INPUT_IME OFF CACHE BOOL "")  # Asian character input - not needed

# ============================================================
# AUDIO - ALL OFF
# ============================================================
# Audio is completely unnecessary for still frame/animation rendering

set(WITH_AUDASPACE OFF CACHE BOOL "")  # Blender's audio system
set(WITH_CODEC_SNDFILE OFF CACHE BOOL "")  # Sound file I/O
set(WITH_JACK OFF CACHE BOOL "")  # JACK audio server
set(WITH_SDL ON CACHE BOOL "")  # SDL audio - required to be on for SDL windowing
set(WITH_OPENAL OFF CACHE BOOL "")  # OpenAL 3D audio
set(WITH_PULSEAUDIO OFF CACHE BOOL "")  # Linux PulseAudio
set(WITH_PIPEWIRE OFF CACHE BOOL "")  # Linux Pipewire
set(WITH_PIPEWIRE_DYNLOAD OFF CACHE BOOL "")  # Dynamic Pipewire loading
set(WITH_COREAUDIO OFF CACHE BOOL "")  # macOS CoreAudio
set(WITH_WASAPI OFF CACHE BOOL "")  # Windows audio
set(WITH_RUBBERBAND OFF CACHE BOOL "")  # Audio time-stretching

# ============================================================
# VIDEO/MEDIA - ALL OFF
# ============================================================
# We only need still frame output, not video encoding

set(WITH_CODEC_FFMPEG OFF CACHE BOOL "")  # Video encoding/decoding - use external tools for video assembly
set(WITH_CODEC_AVI OFF CACHE BOOL "")  # AVI codec
set(WITH_SEQUENCER OFF CACHE BOOL "")  # Video Sequence Editor - not needed for pure rendering

# ============================================================
# FILE FORMATS
# ============================================================

# 3D Formats
set(WITH_IO_FBX ON CACHE BOOL "")  # FBX import/export - REQUIRED per project specs
set(WITH_ALEMBIC OFF CACHE BOOL "")  # Alembic - disable if not using cached simulations
set(WITH_USD ON CACHE BOOL "")  # Universal Scene Description - complex format, needed
set(WITH_COLLADA OFF CACHE BOOL "")  # COLLADA (.dae) - older format, rarely used
set(WITH_IO_WAVEFRONT_OBJ OFF CACHE BOOL "")  # OBJ - simple format but not needed for this workflow
set(WITH_IO_STL OFF CACHE BOOL "")  # STL - 3D printing format, not for rendering
set(WITH_IO_PLY OFF CACHE BOOL "")  # PLY - point cloud format, not needed
set(WITH_IO_GREASE_PENCIL OFF CACHE BOOL "")  # Grease Pencil SVG/PDF export
set(WITH_DRACO OFF CACHE BOOL "")  # Draco mesh compression for glTF
set(WITH_MATERIALX OFF CACHE BOOL "")  # MaterialX - advanced material system, not needed

# Image Formats - Keep professional rendering formats
set(WITH_IMAGE_OPENEXR ON CACHE BOOL "")  # OpenEXR - ESSENTIAL for HDR rendering, multi-layer output, production pipelines
set(WITH_IMAGE_TIFF ON CACHE BOOL "")  # TIFF - professional still image format with extensive metadata
set(WITH_IMAGE_HDR ON CACHE BOOL "")  # Radiance HDR - environment/HDRI lighting maps

# Disable unused image formats
set(WITH_IMAGE_OPENJPEG OFF CACHE BOOL "")  # JPEG2000 - rarely used, slow
set(WITH_IMAGE_DDS OFF CACHE BOOL "")  # DirectDraw Surface - game engine format
set(WITH_IMAGE_CINEON OFF CACHE BOOL "")  # Cineon/DPX - film scanning format (enable if doing film work)
set(WITH_IMAGE_WEBP OFF CACHE BOOL "")  # WebP - web format, not for rendering

# ============================================================
# PHYSICS & SIMULATION
# ============================================================

set(WITH_BULLET OFF CACHE BOOL "")  # Physics engine - disable if not doing physics simulations
set(WITH_MOD_FLUID OFF CACHE BOOL "")  # Mantaflow fluid sim - very slow to build, disable if not needed
set(WITH_MOD_OCEANSIM OFF CACHE BOOL "")  # Ocean modifier - specialized effect, disable if not used
set(WITH_MOD_REMESH ON CACHE BOOL "")  # Remesh modifier - useful for mesh cleanup and topology
set(WITH_QUADRIFLOW OFF CACHE BOOL "")  # Quadriflow remesher - advanced feature, not commonly needed

# ============================================================
# MATH & GEOMETRY LIBRARIES
# ============================================================

set(WITH_GMP OFF CACHE BOOL "")  # Exact boolean operations - very slow, disable unless precision critical
set(WITH_MANIFOLD OFF CACHE BOOL "")  # Fast boolean operations - newer system, not essential
set(WITH_FFTW3 OFF CACHE BOOL "")  # Fast Fourier Transform - used by sim (we disabled those)
set(WITH_IK_ITASC OFF CACHE BOOL "")  # Inverse kinematics solver - animation feature, not needed for rendering
set(WITH_IK_SOLVER OFF CACHE BOOL "")  # Legacy IK solver - animation feature
set(WITH_UV_SLIM OFF CACHE BOOL "")  # Advanced UV unwrapping - modeling feature, not needed at render time
set(WITH_POTRACE OFF CACHE BOOL "")  # Vector tracing - niche feature

# ============================================================
# ADDITIONAL FEATURES - ALL OFF
# ============================================================

set(WITH_XR_OPENXR OFF CACHE BOOL "")  # VR support - not for standard rendering
set(WITH_LIBMV OFF CACHE BOOL "")  # Motion tracking/camera solving - not needed for rendering
set(WITH_HYDRA OFF CACHE BOOL "")  # Hydra render delegate - advanced feature
set(WITH_HARU OFF CACHE BOOL "")  # PDF export for Grease Pencil
set(WITH_PUGIXML OFF CACHE BOOL "")  # XML parsing - only needed for some I/O formats we disabled
set(WITH_INTERNATIONAL OFF CACHE BOOL "")  # UI translations - English only for troubleshooting
set(WITH_BLENDER_THUMBNAILER OFF CACHE BOOL "")  # Thumbnail generation - not needed

# ============================================================
# TESTING & DEBUGGING - ALL OFF FOR PRODUCTION
# ============================================================

set(WITH_GTESTS OFF CACHE BOOL "")  # Google unit tests - development only
set(WITH_GPU_RENDER_TESTS OFF CACHE BOOL "")  # GPU rendering tests
set(WITH_GPU_BACKEND_TESTS OFF CACHE BOOL "")  # GPU backend tests
set(WITH_GPU_DRAW_TESTS OFF CACHE BOOL "")  # GPU drawing tests
set(WITH_GPU_COMPOSITOR_TESTS OFF CACHE BOOL "")  # Compositor tests
set(WITH_GPU_MESH_PAINT_TESTS OFF CACHE BOOL "")  # Mesh painting tests
set(WITH_UI_TESTS OFF CACHE BOOL "")  # UI tests
set(WITH_UI_TESTS_HEADLESS OFF CACHE BOOL "")  # Headless UI tests
set(WITH_DOC_MANPAGE OFF CACHE BOOL "")  # Man page generation
set(WITH_DRAW_DEBUG OFF CACHE BOOL "")  # Draw manager debugging
set(WITH_GPU_SHADER_ASSERT OFF CACHE BOOL "")  # Shader assertions - debug builds only
set(WITH_RENDERDOC OFF CACHE BOOL "")  # RenderDoc GPU debugger integration
set(WITH_ASSERT_ABORT OFF CACHE BOOL "")  # Abort on assertion - debug builds only
set(WITH_MEM_VALGRIND OFF CACHE BOOL "")  # Valgrind memory debugging
set(WITH_COMPILER_ASAN OFF CACHE BOOL "")  # Address sanitizer - debug builds only
set(WITH_CLANG_TIDY OFF CACHE BOOL "")  # Static analysis tool

# ============================================================
# BUILD OPTIMIZATIONS
# ============================================================

set(WITH_INSTALL_PORTABLE ON CACHE BOOL "")  # Self-contained installation - all files in one directory
set(WITH_CPU_SIMD ON CACHE BOOL "")  # Enable SIMD instructions (SSE/AVX) for better performance
set(WITH_UNITY_BUILD ON CACHE BOOL "")  # Combine source files for faster compilation
set(WITH_COMPILER_PRECOMPILED_HEADERS ON CACHE BOOL "")  # Pre-compile common headers for speed
set(WITH_EXPERIMENTAL_FEATURES OFF CACHE BOOL "")  # Disable experimental/unstable features

# ============================================================
# CRITICAL: WINDOWS RUNTIME LIBRARY HANDLING
# ============================================================
# MUST be OFF to avoid side-by-side manifest issues
# We manually copy runtime DLLs after build instead

set(WITH_WINDOWS_BUNDLE_CRT OFF CACHE BOOL "")  # Do NOT bundle runtime - causes manifest errors