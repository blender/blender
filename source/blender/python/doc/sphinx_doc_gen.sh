#!/bin/sh
# run from the blender source dir
#   bash source/blender/python/doc/sphinx_doc_gen.sh
# ssh upload means you need a login into the server

BLENDER="./blender.bin"
SSH_HOST="ideasman42@emo.blender.org"
SSH_UPLOAD="/data/www/vhosts/www.blender.org/documentation/250PythonDoc"

# clear doc dir
rm -rf ./source/blender/python/doc/sphinx-in ./source/blender/python/doc/sphinx-out
$BLENDER -b -P ./source/blender/python/doc/sphinx_doc_gen.py

# html
sphinx-build source/blender/python/doc/sphinx-in source/blender/python/doc/sphinx-out
cp source/blender/python/doc/sphinx-out/contents.html source/blender/python/doc/sphinx-out/index.html
ssh ideasman42@emo.blender.org 'rm -rf '$SSH_UPLOAD'/*'
rsync --progress -avze "ssh -p 22" /b/source/blender/python/doc/sphinx-out/* $SSH_HOST:$SSH_UPLOAD/

# pdf
sphinx-build -b latex source/blender/python/doc/sphinx-in source/blender/python/doc/sphinx-out
cd source/blender/python/doc/sphinx-out
make
cd ../../../../../
rsync --progress -avze "ssh -p 22" source/blender/python/doc/sphinx-out/contents.pdf $SSH_HOST:$SSH_UPLOAD/blender_python_reference_250.pdf
