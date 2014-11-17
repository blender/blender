#!/bin/bash

SDL="SDL2"
INCLUDE_DIR="/usr/include/${SDL}"
SCRIPT=`realpath -s $0`
DIR=`dirname $SCRIPT`
DIR=`dirname $DIR`

for f in $DIR/include/${SDL}/*.h; do
  file_name=`basename $f`
  echo "Striping $file_name..."
  sed -r ':a; s%(.*)/\*.*\*/%\1%; ta; /\/\*/ !b; N; ba' -i $f
  sed 's/[ \t]*$//' -i $f
  sed '/^$/N;/^\n$/D' -i $f
done
