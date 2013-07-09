#!/bin/sh

echo "*** EIGEN#-HG Update utility"
echo "*** This gets a new eigen3-hg tree and adapts it to blenders build structure"
echo "*** Warning! This script will wipe all the header file"

if [ "x$1" = "x--i-really-know-what-im-doing" ] ; then
    echo Proceeding as requested by command line ...
else
    echo "*** Please run again with --i-really-know-what-im-doing ..."
    exit 1
fi

# get the latest revision from repository.
hg clone http://bitbucket.org/eigen/eigen
if [ -d eigen ]
then
    cd eigen
    # put here the version you want to use
    hg up 3.0
    rm -f `find Eigen/ -type f -name "CMakeLists.txt"`
    cp -r Eigen ..
    cd ..
    rm -rf eigen
else
    echo "Did you install Mercurial?"
fi

