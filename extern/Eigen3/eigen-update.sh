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
git clone https://gitlab.com/libeigen/eigen.git eigen.git
if [ -d eigen.git ]
then
    rm -rf Eigen
    cd eigen.git
    # put here the version you want to use
    git checkout 3.4.0
    rm -f `find Eigen/ -type f -name "CMakeLists.txt"`
    cp -r Eigen ..
    cd ..
    rm -rf eigen.git
    find Eigen -type f -exec chmod 644 {} \;
else
    echo "Did you install Git?"
fi

