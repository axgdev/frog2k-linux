#!/bin/sh

exec >/dev/console 2>&1
echo 'sf2000_buildroot: storage init script start'
exec /etc/init.d/rcS start
