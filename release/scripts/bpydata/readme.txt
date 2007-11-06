This directory is the default place for scripts to put their data,
like internal files needed by the script and its saved configuration.

Scripts can find the path to this dir using Blender.Get("datadir").
Ex:

import Blender
print Blender.Get("datadir")

