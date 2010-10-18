#!/bin/sh
# run from the blender source dir
#   bash source/blender/python/doc/sphinx_doc_gen.sh
# ssh upload means you need an account on the server

BLENDER="./blender.bin"
SSH_HOST="ideasman42@emo.blender.org"
SSH_UPLOAD="/data/www/vhosts/www.blender.org/documentation" # blender_python_api_VERSION, added after

# sed string from hell, 'Blender 2.53 (sub 1) Build' --> '2_53_1'
# "_".join(str(v) for v in bpy.app.version)
# custom blender vars
blender_srcdir=$(dirname $0)/../../
blender_version=$(grep BLENDER_VERSION $blender_srcdir/source/blender/blenkernel/BKE_blender.h | tr -dc 0-9)
blender_subversion=$(grep BLENDER_SUBVERSION $blender_srcdir/source/blender/blenkernel/BKE_blender.h | tr -dc 0-9)
BLENDER_VERSION=$(expr $blender_version / 100)_$(expr $blender_version % 100)_$blender_subversion

BLENDER_VERSION=`$BLENDER --version | cut -f2-4 -d" " | sed 's/(//g' | sed 's/)//g' | sed 's/ sub /./g' | sed 's/\./_/g'`
SSH_UPLOAD_FULL=$SSH_UPLOAD/"blender_python_api_"$BLENDER_VERSION

SPHINXBASE=doc/python_api/

# dont delete existing docs, now partial updates are used for quick builds.
$BLENDER --background --python $SPHINXBASE/sphinx_doc_gen.py

# html
sphinx-build $SPHINXBASE/sphinx-in $SPHINXBASE/sphinx-out
cp $SPHINXBASE/sphinx-out/contents.html $SPHINXBASE/sphinx-out/index.html
ssh ideasman42@emo.blender.org 'rm -rf '$SSH_UPLOAD_FULL'/*'
rsync --progress -avze "ssh -p 22" $SPHINXBASE/sphinx-out/* $SSH_HOST:$SSH_UPLOAD_FULL/

# pdf
sphinx-build -b latex $SPHINXBASE/sphinx-in $SPHINXBASE/sphinx-out
cd $SPHINXBASE/sphinx-out
make
cd -
rsync --progress -avze "ssh -p 22" $SPHINXBASE/sphinx-out/contents.pdf $SSH_HOST:$SSH_UPLOAD_FULL/blender_python_reference_$BLENDER_VERSION.pdf
