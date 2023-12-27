#!/bin/sh
if [ -f configure.ac ] ; then
   version=`grep AC_INIT configure.ac | head -n 1 | sed 's/^[^[]*.//;s/^[^[]*.//;s/].*//'`
fi
echo "$version"
