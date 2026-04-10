#!/bin/dash
picom &
dbus-update-activation-environment --all &
xrdb ~/etc/X11/.Xresources
dunst &
unclutter --timeout 5 &
#xautolock -time 10 -locker "slock" &
/usr/bin/xset b off &
xset r rate 200 50 &
bat-notify &
wl &
polybar -r &
xsetroot -cursor_name left_ptr &
exec bspwm
