#!/bin/sh

echo "*** EIGEN2-SVN Update utility"
echo "*** This gets a new eigen2-svn tree and adapts it to blenders build structure"
echo "*** Warning! This script will wipe all the header file"
echo "*** Please run again with --i-really-know-what-im-doing ..."

if [ "x$1" = "x--i-really-know-what-im-doing" ] ; then
   echo proceeding...
else
   exit -1
fi

mkdir eigen2

# get the latest revision from SVN.
#svn co svn://anonsvn.kde.org/home/kde/trunk/kdesupport/eigen2/Eigen eigen2
svn co svn://anonsvn.kde.org/home/kde/tags/eigen/2.0.1/Eigen eigen2

rm -rf `find eigen2/ -type d -name ".svn"`
rm -f `find eigen2/ -type f -name "CMakeLists.txt"`

for i in "eigen2/*" ; do
    cp -r $i Eigen
done

rm -rf eigen2
