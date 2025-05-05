This folder contains files used by ctest testing system in Blender.
The purpose of this files it to do automated render tests for Cycles,
Workbench and EEVEE.

If you want to extend this collection make sure the file is really
small and uses as few samples as possible for reliable detection of
render regressions.

Each file is expected to be rendered in around one second.
