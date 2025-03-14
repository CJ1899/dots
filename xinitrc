#!/bin/dash
#picom --no-fading-openclose &
picom &
sudo lxqt-policykit-agent &
sudo xinput set-prop 9 311 0 &
#~/scripts/msollis &
#feh --bg-fill ~/wallpapers --randomize &
#feh --bg-fill ~/Pictures/C.png &
#~/sky &
[ -f "$HOME/.fehbg" ] && sh "$HOME/.fehbg" &
#nitrogen --restore &
#nice xwinwrap -b -s -fs -st -sp -nf -ov -fdt -- mpv -wid WID --loop --really-quiet --framedrop=vo --no-audio --no-osc --no-input-default-bindings --panscan="1.0" ~/Videos/lily.webm &
#/home/pc/scripts/sbar &
bar &
dbus-update-activation-environment --all &
xrdb ~/.config/X11/.Xresources
dunst &
unclutter --timeout 5 &
#xautolock -time 10 -locker "slock" &
#exec i3
#polybar -r &
#thunderbird --headless &
#mpv ~/nt4.mp3 &
/usr/bin/xset b off &
udiskie &
xclip &
xset r rate 200 50 &
bat-notify &
#setxkbmap -option "caps:swapescape" &
#exec ~/src/dwm-distrotube/dwm
#exec chadwm
#mpc clear &
#mpc load "1" &
#qbittorrent &
exec dwm
#exec 2wm
#exec 2bwm
