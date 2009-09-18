# epy_docgen.sh
# generates blender python doc using epydoc
# requires epydoc in your PATH.
# run from the doc directory containing the .py files
# usage:  sh epy_docgen.sh

# set posix locale so regex works properly for [A-Z]*.py
LC_ALL=POSIX

epydoc --debug -v  -o BPY_GE --url "http://www.blender.org" --top API_intro \
 --name "Blender GameEngine" --no-private --no-sourcecode --inheritance=included \
 *.py \
 ../../../source/blender/python/api2_2x/doc/BGL.py \
 ../../../source/blender/python/api2_2x/doc/Mathutils.py \
 ../../../source/blender/python/api2_2x/doc/Geometry.py
 
