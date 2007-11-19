#!/bin/sh

for arg in `cat /proc/cmdline`; do
    if [ "${arg##bootfile=}" != "${arg}" ]; then
	bootfile="${arg##bootfile=}"
    fi
done

clear
echo
echo
echo "gPXE Universal Boot"
echo "==================="
echo
echo
/sbin/lsmod
echo
echo
/sbin/route
echo
echo
echo "Booted from $bootfile:"
echo
wget -q "$bootfile" -O -
echo

export PS1="\u@\h:\w# "
exec /bin/sh
