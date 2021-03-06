#! /bin/bash

cat <<EOF

 =========================== A T T E N T I O N ! !  ===========================
 ===                                                                        ===
 ===  - This is an unstable cvs development and working version.            ===
 ===                                                                        ===
 ===  - This version currently does no actual window management.            ===
 ===                                                                        ===
 ===  - If you are looking for the "current cvs version" of uwm get the     ===
 ===       latest version from the cvs 0.2 branch labelled BRANCH-0_2.      ===
 ===                                                                        ===
 ===  - If you want to do development on uwm 0.3 get the latest version     ===
 ===       from the cvs 0.2 branch or any other window manager for          ===
 ===       window management and keep this version for hacking.             ===
 ===                                                                        ===
 =========================== A T T E N T I O N ! !  ===========================

EOF

BRANCH02URL="`svn info | sed -e 's/^Repository Root: //' -e t -e d`/uwm/branches/BRANCH-0.2"

while true ; do 
  echo "Do you now want to patch this version back to the latest version that"
  echo "actually does window management, i.e. the latest version from the"
  echo -n "0.2 branch?   [y(es)/n(o)] "
  read answer
  case $answer in
    y|yes)
      echo
      cat <<EOF
This script will now take the necessary steps to patch uwm back to the
0.2 branch. Please make sure that your computer is connected to the internet
and that you have access to the ude svn server and press return.

You can also press CTRL-C at this point and run 
svn switch "$BRANCH02URL"
in this directory anytime later.
EOF
      read
      svn switch "$BRANCH02URL"
      exec $@
      break
      ;;
    n|no)
      echo
      cat <<EOF
Thanks for being interested in helping us out with uwm 0.3.
contact christian ruppert <arc@users.sourceforge.net> for details and full
svn access.

press return to proceed.
EOF
      read
      break
      ;;
  esac
done

echo "============================== cleaning up possibly improper checkins"
mv Makefile.am Makefile.am.origcvs
echo "*** Makefile.am saved to Makefile.am.origcvs"
cat Makefile.am.origcvs | sed '
/\\$/N
/SUBDIRS[[:space:]]*=/ {
s/\([=]\|[[:space:]]\)intl\([[:space:]]\|$\)/\1/g
s/\([=]\|[[:space:]]\)m4\([[:space:]]\|$\)/\1/g
}
/EXTRA_DIST[[:space:]]*=/ {
s/\([=]\|[[:space:]]\)mkinstalldirs\([[:space:]]\|$\)/\1/g
s/\([=]\|[[:space:]]\)config\.rpath\([[:space:]]\|$\)/\1/g
}
/ACLOCAL_AMFLAGS[[:space:]]*=/ {
d
}
' > Makefile.am
mv configure.in configure.in.origcvs
echo "*** configure.in saved to configure.in.origcvs"
cat configure.in.origcvs | sed '
/\\$/N
/AC_OUTPUT[[:space:]]*(/ {
s/\([=]\|[[:space:]]\)intl\/Makefile\([[:space:]]\|)\)/\1/g
s/\([=]\|[[:space:]]\)po\/Makefile\.in\([[:space:]]\|)\)/\1/g
s/\([=]\|[[:space:]]\)po\/Makefile\([[:space:]]\|)\)/\1/g
s/\([=]\|[[:space:]]\)m4\/Makefile\([[:space:]]\|)\)/\1/g
}
' > configure.in

echo "============================== creating some symlinks"
ln -s ../README doc
ln -s ../uwm/ude_config_consts.h config

echo "============================== gettextize"
gettextize -f --intl --no-changelog

echo "============================== aclocal"
aclocal -I m4
# The next two lines should be uncommented when the libude library be done
# echo "============================== libtoolize"
# libtoolize --force 

echo "============================== autoheader"
autoheader

echo "============================== automake"
automake -a

echo "============================== autoconf"
autoconf

echo "============================== configure"
./configure $*
