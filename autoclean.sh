find . -name Makefile.in -exec rm -fr {} \;  2> /dev/null
find . -name .deps -exec rm -fr {} \; 2> /dev/null
find . -name .libs -exec rm -fr {} \; 2> /dev/null
rm Makefile 2> /dev/null
rm ./configure 2> /dev/null
rm ./config.h.in 2> /dev/null
rm ./config.h 2> /dev/null
rm ./aclocal.m4 2> /dev/null
rm ./config.status 2> /dev/null
rm ./config.log 2> /dev/null
rm ./libtool 2> /dev/null
rm ./stamp-h1 2> /dev/null
rm -r autom4te.cache 2> /dev/null
rm -r build-aux 2> /dev/null
cd m4 2> /dev/null
rm libtool.m4  ltoptions.m4  ltsugar.m4  ltversion.m4  lt~obsolete.m4 2> /dev/null
cd - 2> /dev/null


