#!/bin/sh

for arg in `cat /proc/cmdline`; do
    if [ "${arg##bootfile=}" != "${arg}" ]; then
	bootfile="${arg##bootfile=}"
    fi
done

echo
echo
echo
echo "iPXE Boot Demonstration"
echo "======================="
echo
echo
/bin/uname -a
echo
echo
echo "Congratulations!  You have successfully booted the iPXE demonstration"
echo "image from ${bootfile}"
echo
echo "See http://ipxe.org for more ideas on how to use iPXE."
echo

export PS1="\u:\w# "
exec /bin/sh
