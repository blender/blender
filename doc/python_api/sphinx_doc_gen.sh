#!/bin/sh
# run from the blender source dir
#   bash source/blender/python/doc/sphinx_doc_gen.sh
# ssh upload means you need an account on the server

BLENDER="./blender.bin"
SSH_USER="ideasman42"
SSH_HOST=$SSH_USER"@emo.blender.org"
SSH_UPLOAD="/data/www/vhosts/www.blender.org/documentation" # blender_python_api_VERSION, added after

# 'Blender 2.53 (sub 1) Build' --> '2_53_1' as a shell script.
# "_".join(str(v) for v in bpy.app.version)
# custom blender vars
blender_srcdir=$(dirname $0)/../../
blender_version=$(grep "BLENDER_VERSION\s" $blender_srcdir/source/blender/blenkernel/BKE_blender.h | awk '{print $3}')
blender_version_char=$(grep BLENDER_VERSION_CHAR $blender_srcdir/source/blender/blenkernel/BKE_blender.h | awk '{print $3}')
blender_version_cycle=$(grep BLENDER_VERSION_CYCLE $blender_srcdir/source/blender/blenkernel/BKE_blender.h | awk '{print $3}')
blender_subversion=$(grep BLENDER_SUBVERSION $blender_srcdir/source/blender/blenkernel/BKE_blender.h | awk '{print $3}')

if [ "$blender_version_cycle" == "release" ]
then
	BLENDER_VERSION=$(expr $blender_version / 100)_$(expr $blender_version % 100)$blender_version_char"_release"
else
	BLENDER_VERSION=$(expr $blender_version / 100)_$(expr $blender_version % 100)_$blender_subversion
fi

SSH_UPLOAD_FULL=$SSH_UPLOAD/"blender_python_api_"$BLENDER_VERSION

SPHINXBASE=doc/python_api/

# dont delete existing docs, now partial updates are used for quick builds.
$BLENDER --background --factory-startup --python $SPHINXBASE/sphinx_doc_gen.py

# html
sphinx-build $SPHINXBASE/sphinx-in $SPHINXBASE/sphinx-out
cp $SPHINXBASE/sphinx-out/contents.html $SPHINXBASE/sphinx-out/index.html
ssh $SSH_USER@emo.blender.org 'rm -rf '$SSH_UPLOAD_FULL'/*'
rsync --progress -avze "ssh -p 22" $SPHINXBASE/sphinx-out/* $SSH_HOST:$SSH_UPLOAD_FULL/

# symlink the dir to a static URL
ssh $SSH_USER@emo.blender.org 'rm '$SSH_UPLOAD'/250PythonDoc && ln -s '$SSH_UPLOAD_FULL' '$SSH_UPLOAD'/250PythonDoc'

# pdf
sphinx-build -b latex $SPHINXBASE/sphinx-in $SPHINXBASE/sphinx-out
cd $SPHINXBASE/sphinx-out
make
cd -

# rename so local PDF has matching name.
mv $SPHINXBASE/sphinx-out/contents.pdf $SPHINXBASE/sphinx-out/blender_python_reference_$BLENDER_VERSION.pdf
rsync --progress -avze "ssh -p 22" $SPHINXBASE/sphinx-out/blender_python_reference_$BLENDER_VERSION.pdf $SSH_HOST:$SSH_UPLOAD_FULL/blender_python_reference_$BLENDER_VERSION.pdf
