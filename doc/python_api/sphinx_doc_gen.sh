#!/bin/sh
# run from the blender source dir
#   bash doc/python_api/sphinx_doc_gen.sh
# ssh upload means you need an account on the server


# ----------------------------------------------------------------------------
# Upload vars

# disable for testing
DO_UPLOAD=true
DO_EXE_BLENDER=true
DO_OUT_HTML=true
DO_OUT_HTML_ZIP=true
DO_OUT_PDF=false


BLENDER="./blender.bin"
SSH_USER="ideasman42"
SSH_HOST=$SSH_USER"@blender.org"
SSH_UPLOAD="/data/www/vhosts/www.blender.org/api" # blender_python_api_VERSION, added after

# ----------------------------------------------------------------------------
# Blender Version & Info

# 'Blender 2.53 (sub 1) Build' --> '2_53_1' as a shell script.
# "_".join(str(v) for v in bpy.app.version)
# custom blender vars
blender_srcdir=$(dirname -- $0)/../..
blender_version=$(grep "BLENDER_VERSION\s" "$blender_srcdir/source/blender/blenkernel/BKE_blender.h" | awk '{print $3}')
blender_version_char=$(grep "BLENDER_VERSION_CHAR\s" "$blender_srcdir/source/blender/blenkernel/BKE_blender.h" | awk '{print $3}')
blender_version_cycle=$(grep "BLENDER_VERSION_CYCLE\s" "$blender_srcdir/source/blender/blenkernel/BKE_blender.h" | awk '{print $3}')
blender_subversion=$(grep "BLENDER_SUBVERSION\s" "$blender_srcdir/source/blender/blenkernel/BKE_blender.h" | awk '{print $3}')

if [ "$blender_version_cycle" = "release" ] ; then
	BLENDER_VERSION=$(expr $blender_version / 100)_$(expr $blender_version % 100)$blender_version_char"_release"
else
	BLENDER_VERSION=$(expr $blender_version / 100)_$(expr $blender_version % 100)_$blender_subversion
fi

SSH_UPLOAD_FULL=$SSH_UPLOAD/"blender_python_api_"$BLENDER_VERSION

SPHINXBASE=doc/python_api


# ----------------------------------------------------------------------------
# Generate reStructuredText (blender/python only)

if $DO_EXE_BLENDER ; then
	# dont delete existing docs, now partial updates are used for quick builds.
	$BLENDER --background -noaudio --factory-startup --python $SPHINXBASE/sphinx_doc_gen.py
fi


# ----------------------------------------------------------------------------
# Generate HTML (sphinx)

if $DO_OUT_HTML ; then
	# sphinx-build -n -b html $SPHINXBASE/sphinx-in $SPHINXBASE/sphinx-out

	# annoying bug in sphinx makes it very slow unless we do this. should report.
	cd $SPHINXBASE
	sphinx-build -b html sphinx-in sphinx-out

	# XXX, saves space on upload and zip, should move HTML outside
	# and zip up there, for now this is OK
	rm -rf sphinx-out/.doctrees

	# incase we have a zip already
	rm -f blender_python_reference_$BLENDER_VERSION.zip

	# ------------------------------------------------------------------------
	# ZIP the HTML dir for upload

	if $DO_OUT_HTML_ZIP ; then
		# lame, temp rename dir
		mv sphinx-out blender_python_reference_$BLENDER_VERSION
		zip -r -9 blender_python_reference_$BLENDER_VERSION.zip blender_python_reference_$BLENDER_VERSION
		mv blender_python_reference_$BLENDER_VERSION sphinx-out
	fi

	cd -
fi


# ----------------------------------------------------------------------------
# Generate PDF (sphinx/laytex)

if $DO_OUT_PDF ; then
	sphinx-build -n -b latex $SPHINXBASE/sphinx-in $SPHINXBASE/sphinx-out
	make -C $SPHINXBASE/sphinx-out
	mv $SPHINXBASE/sphinx-out/contents.pdf $SPHINXBASE/sphinx-out/blender_python_reference_$BLENDER_VERSION.pdf
fi


# ----------------------------------------------------------------------------
# Upload to blender servers, comment this section for testing

if $DO_UPLOAD ; then

	cp $SPHINXBASE/sphinx-out/contents.html $SPHINXBASE/sphinx-out/index.html
	ssh $SSH_USER@blender.org 'rm -rf '$SSH_UPLOAD_FULL'/*'
	rsync --progress -ave "ssh -p 22" $SPHINXBASE/sphinx-out/* $SSH_HOST:$SSH_UPLOAD_FULL/

	## symlink the dir to a static URL
	#ssh $SSH_USER@blender.org 'rm '$SSH_UPLOAD'/250PythonDoc && ln -s '$SSH_UPLOAD_FULL' '$SSH_UPLOAD'/250PythonDoc'

	# better redirect
	ssh $SSH_USER@blender.org 'echo "<html><head><title>Redirecting...</title><meta http-equiv=\"REFRESH\" content=\"0;url=../blender_python_api_'$BLENDER_VERSION'/\"></head><body>Redirecting...</body></html>" > '$SSH_UPLOAD'/250PythonDoc/index.html'

	# redirect for release only so wiki can point here
	if [ "$blender_version_cycle" = "release" ] ; then
		ssh $SSH_USER@blender.org 'echo "<html><head><title>Redirecting...</title><meta http-equiv=\"REFRESH\" content=\"0;url=../blender_python_api_'$BLENDER_VERSION'/\"></head><body>Redirecting...</body></html>" > '$SSH_UPLOAD'/blender_python_api/index.html'
	fi

	if $DO_OUT_PDF ; then
		# rename so local PDF has matching name.
		rsync --progress -ave "ssh -p 22" $SPHINXBASE/sphinx-out/blender_python_reference_$BLENDER_VERSION.pdf $SSH_HOST:$SSH_UPLOAD_FULL/blender_python_reference_$BLENDER_VERSION.pdf
	fi

	if $DO_OUT_HTML_ZIP ; then
		rsync --progress -ave "ssh -p 22" $SPHINXBASE/blender_python_reference_$BLENDER_VERSION.zip $SSH_HOST:$SSH_UPLOAD_FULL/blender_python_reference_$BLENDER_VERSION.zip
	fi

fi


# ----------------------------------------------------------------------------
# Print some useful text

echo ""
echo "Finished! view the docs from: "
if $DO_OUT_HTML ; then echo "  html:" $SPHINXBASE/sphinx-out/contents.html ; fi
if $DO_OUT_PDF ; then  echo "   pdf:" $SPHINXBASE/sphinx-out/blender_python_reference_$BLENDER_VERSION.pdf ; fi
