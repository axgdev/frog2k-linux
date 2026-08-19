#!/bin/sh

if [ -w /dev/kmsg ]; then
	printf 'sf2000_userspace: storage init script start\n' > /dev/kmsg
else
	echo 'sf2000_userspace: storage init script start'
fi
exec /etc/init.d/rcS start
