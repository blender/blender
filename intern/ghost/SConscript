ghost_env = Environment()

# Import the C flags set in the SConstruct file
Import ('cflags')
Import ('cxxflags')
Import ('defines')
ghost_env.Append (CCFLAGS = cflags)
ghost_env.Append (CXXFLAGS = cxxflags)
ghost_env.Append (CPPDEFINES = defines)

Import ('window_system')

source_files = ['intern/GHOST_Buttons.cpp',
                'intern/GHOST_C-api.cpp',
                'intern/GHOST_CallbackEventConsumer.cpp',
                'intern/GHOST_DisplayManager.cpp',
                'intern/GHOST_EventManager.cpp',
                'intern/GHOST_EventPrinter.cpp',
                'intern/GHOST_ISystem.cpp',
                'intern/GHOST_ModifierKeys.cpp',
                'intern/GHOST_Rect.cpp',
                'intern/GHOST_System.cpp',
                'intern/GHOST_TimerManager.cpp',
                'intern/GHOST_Window.cpp',
                'intern/GHOST_WindowManager.cpp']

if window_system == 'X11':
    source_files += ['intern/GHOST_DisplayManagerX11.cpp',
                     'intern/GHOST_SystemX11.cpp',
                     'intern/GHOST_WindowX11.cpp']
elif window_system == 'WIN32':
    source_files += ['intern/GHOST_DisplayManagerWin32.cpp',
                     'intern/GHOST_SystemWin32.cpp',
                     'intern/GHOST_WindowWin32.cpp']
elif window_system == 'CARBON':
    source_files += ['intern/GHOST_DisplayManagerCarbon.cpp',
                     'intern/GHOST_SystemCarbon.cpp',
                     'intern/GHOST_WindowCarbon.cpp']
else:
    print "Unknown window system specified."

ghost_env.Append (CPPPATH = ['.',
                             '../string'])

ghost_env.Library (target='#/lib/blender_GHOST', source=source_files)
