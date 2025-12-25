#define TERMINAL "st"
#define TERMCLASS "St"
#define BROWSER "firefox"

static const unsigned int borderpx  = 2;        /* border pixel of windows */
static const unsigned int snap      = 32;       /* snap pixel */
static const unsigned int gappih    = 25;       /* horiz inner gap between windows */
static const unsigned int gappiv    = 25;       /* vert inner gap between windows */
static const unsigned int gappoh    = 20;       /* horiz outer gap between windows and screen edge */
static const unsigned int gappov    = 20;       /* vert outer gap between windows and screen edge */
static const int swallowfloating    = 0;        /* 1 means swallow floating windows by default */
static       int smartgaps          = 0;        /* 1 means no outer gap when there is only one window */
static const int showbar            = 1;        /* 0 means no bar */
static const int topbar             = 1;        /* 0 means bottom bar */
static const int vertpad            = 5;        /* vertical padding of bar */
static const int sidepad            = 7;        /* horizontal padding of bar */
static const char *barlayout        = "tln|s";
static const char *fonts[]          = { "JetBrainsMono:size=7", "NotoColorEmoji:pixelsize=12:antialias=true:autohint=true"};
static const char dmenufont[]       = "JetBrainsMono:size=7";
static unsigned int baralpha        = 0xb0;
static unsigned int borderalpha     = OPAQUE;

//static const char *tags[] = { "", "", "", "", "", "", "", "", "", "", "", "", ""};
static const char *tags[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "*"};
/*static const char *tags[] = {
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    ""
};*/

static const Rule rules[] = {
	/* xprop(1):
	 *	WM_CLASS(STRING) = instance, class
	 *	WM_NAME(STRING) = title
	 */

	/* class           instance  title           tags mask   isfloating  isterminal  noswallow  monitor */
/*	{ "Gimp",          NULL,     NULL,            0,          1,          0,           0,        -1 }, */
	{ "Firefox",       NULL,     NULL,            1 << 8,     0,          0,          -1,        -1 },
//	{ "qBittorrent",   NULL,     NULL,            1 << 11,    0,          0,          -1,        -1 },
	{ "LibreWolf",     NULL,     NULL,            0,          0,          0,          -1,        -1 },
	{ "Arandr",        NULL,     NULL,            0,          1,          0,          -1,        -1 },
	{ TERMCLASS,       NULL,     NULL,            0,          0,          1,           0,        -1 },
	{ NULL,            NULL,     "Event Tester",  0,          0,          0,           1,        -1 }, /* xev */
	{ TERMCLASS,       NULL,     "lf",            0,          0,          1,           0,        -1 }, /* lf */
//	{ NULL,            NULL,     "sudo",          0,          1,          0,          -1,        -1 },
};

static const float mfact     = 0.50; /* factor of master area size [0.05..0.95] */
static const int nmaster     = 1;    /* number of clients in master area */
static const int resizehints = 0;    /* 1 means respect size hints in tiled resizals */
static const int lockfullscreen = 1; /* 1 will force focus on the fullscreen window */

#include "themes/def.h"
//#include "themes/pinkt.h"

static const unsigned int stairpx   = 70;       /* depth of the stairs layout */
static const int stairdirection     = 1;        /* 0: left-aligned, 1: right-aligned */
static const int stairsamesize      = 1;        /* 1 means shrink all the staired windows to the same size */

#define FORCE_VSPLIT 1  /* nrowgrid layout: force two clients to always split vertically */
#include "vanitygaps.c"

static const Layout layouts[] = {
	{ "[]=",      tile         },
	{ "TTT",      bstack       },

	{ "><>",      NULL    },
	{ "HHH",      grid    },

	{ "[D]",      deck    },
	{ "[M]",      monocle },

	{ "[@]",      spiral  },
	{ "[\\]",     dwindle },

        { "|M|",      centeredmaster         },
	{ ">M>",      centeredfloatingmaster },

	{ "[S]",      stairs                },
	{ "###",      nrowgrid              },
};

#define MODKEY  Mod4Mask
#define ALTKEY  Mod1Mask
#define SHIFT   ShiftMask
#define CONTROL ControlMask
#define TAGKEYS(KEY,TAG) \
	{ MODKEY,                       KEY,      view,           {.ui = 1 << TAG} }, \
	{ MODKEY|CONTROL,           KEY,      toggleview,     {.ui = 1 << TAG} }, \
	{ MODKEY|SHIFT,             KEY,      tag,            {.ui = 1 << TAG} }, \
	{ MODKEY|CONTROL|SHIFT, KEY,      toggletag,      {.ui = 1 << TAG} },

#define SHCMD(cmd) { .v = (const char*[]){ "/bin/sh", "-c", cmd, NULL } }

static char dmenumon[2] = "0";

#include <X11/XF86keysym.h>

static const Key keys[] = {
	{ MODKEY,                  XK_Return, spawn, {.v = (const char*[]){ TERMINAL, NULL } } },
	{ MODKEY,                  XK_d,      spawn, {.v = (const char*[]){"rofi", "-show", "drun", NULL } } },
        { MODKEY|SHIFT,            XK_d,      spawn, {.v = (const char*[]){"rofi", "-show", "run", NULL } } },
	{ MODKEY,                  XK_w,      spawn, {.v = (const char*[]){ BROWSER, NULL} } },
	{ MODKEY|SHIFT,            XK_w,      spawn, {.v = (const char*[]){ TERMINAL, "sudo", "nmtui",  NULL } } },
	{ MODKEY|SHIFT,            XK_a,      spawn, {.v = (const char*[]){"pcmanfm", NULL } } },
	{ MODKEY,                  XK_a,      spawn, {.v = (const char*[]){ TERMINAL, "lf", NULL } } },
//	{ MODKEY,                  XK_v,      spawn, {.v = (const char*[]){"sudo","systemctl", "suspend", NULL } } },
	{ MODKEY,                  XK_v,      spawn, SHCMD("slock & sudo systemctl suspend") },
	{ MODKEY,                  XK_z,      spawn, {.v = (const char*[]){"slock", NULL } } },
	{ MODKEY,                  XK_s,      spawn, {.v = (const char*[]){ "dmenu_run", "-m", dmenumon, "-fn", dmenufont, "-nb", black, "-nf", gray3, "-sb", blue, "-sf", gray4, NULL } } },
	{ MODKEY,                  XK_r,      spawn, {.v = (const char*[]){ TERMINAL, "sudo", "htop",   NULL } } },
	{ MODKEY,		   XK_n,      spawn, {.v = (const char*[]){ TERMINAL,  "nvim", NULL } } },
//	{ MODKEY,                  XK_g,      spawn, {.v = (const char*[]){ "gedit", NULL } } },
	{ MODKEY,                  XK_g,      spawn, {.v = (const char*[]){ "qbittorrent", NULL } } },
	{ MODKEY,                  XK_c,      spawn, {.v = (const char*[]){ "s", NULL } } },
	{ MODKEY,                  XK_x,      spawn, {.v = (const char*[]){ "s", "-b", NULL } } },
	{ MODKEY|CONTROL|SHIFT,    XK_c,      spawn, {.v = (const char*[]){ "s", "-r", NULL } } },
	{ MODKEY,                  XK_m,      spawn, {.v = (const char*[]){ "/home/pc/scripts/s2", NULL } } },
	{ MODKEY|SHIFT,            XK_c,      spawn, {.v = (const char*[]){ "s2", NULL } } },
	{ MODKEY|SHIFT,            XK_x,      spawn, {.v = (const char*[]){ "s2", "-b", NULL } } },
	{ MODKEY|CONTROL,          XK_c,      spawn, {.v = (const char*[]){ "s2", "-r", NULL } } },
	{ MODKEY,                  XK_Home,   spawn, {.v = (const char*[]){ "/home/pc/scripts/layout", NULL } } },


	{ 0,                       XF86XK_MonBrightnessUp,     spawn, {.v = (const char*[]){ "brightness", "up", NULL } } },
       /*{ 0,                      XF86XK_MonBrightnessUp,     spawn, {.v = (const char*[]){ "xbacklight", "+10", NULL } } },*/
	{ MODKEY|SHIFT,            XF86XK_MonBrightnessUp,     spawn, {.v = (const char*[]){ "xbacklight", "+1", NULL } } },
        { MODKEY,                  XF86XK_MonBrightnessUp,     spawn, {.v = (const char*[]){ "xbacklight", "=", "100", NULL } } },
        { 0,                       XF86XK_MonBrightnessDown,   spawn, {.v = (const char*[]){ "brightness", "down", NULL } } },
      /*{ 0,                       XF86XK_MonBrightnessDown,   spawn, {.v = (const char*[]){ "xbacklight", "-10", NULL } } },*/
        { MODKEY|SHIFT,            XF86XK_MonBrightnessDown,   spawn, {.v = (const char*[]){ "xbacklight", "-1", NULL } } },
        { MODKEY,                  XF86XK_MonBrightnessDown,   spawn, {.v = (const char*[]){ "xbacklight", "=", "1", NULL } } },
    	{ 0,                       XF86XK_AudioLowerVolume,    spawn, {.v = (const char*[]){ "volume", "down", NULL } } },
      /*{ 0,                       XF86XK_AudioLowerVolume,    spawn, {.v = (const char*[]){ "amixer", "-D", "pulse", "sset", "Master", "5%-", NULL } } },*/
	{ MODKEY,                  XF86XK_AudioLowerVolume,    spawn, {.v = (const char*[]){ "amixer", "-D", "pulse", "sset", "Master", "1%-", NULL } } },
    	{ 0,                       XF86XK_AudioMute,           spawn, {.v = (const char*[]){ "volume", "mute",  NULL } } },
      /*{ 0,                       XF86XK_AudioMute,           spawn, {.v = (const char*[]){ "amixer", "-D", "pulse", "sset", "Master", "toggle", NULL } } },*/
	{ MODKEY,                  XF86XK_AudioMute,           spawn, {.v = (const char*[]){ TERMINAL, "pulsemixer", NULL } } },
    	{ 0,                       XF86XK_AudioRaiseVolume,    spawn, {.v = (const char*[]){ "volume", "up",  NULL } } },
      /*{ 0,                       XF86XK_AudioRaiseVolume,    spawn, {.v = (const char*[]){ "amixer", "-D", "pulse", "sset", "Master", "5%+", NULL } } },*/
	{ MODKEY,                  XF86XK_AudioRaiseVolume,    spawn, {.v = (const char*[]){ "amixer", "-D", "pulse", "sset", "Master", "1%+", NULL } } },
	{ 0,                       XF86XK_AudioMicMute,        spawn, {.v = (const char*[]){ "amixer", "-D", "pulse", "sset", "Capture", "toggle", NULL } } },
	{ MODKEY,                  XF86XK_AudioMicMute,        spawn, {.v = (const char*[]){ "rofi", "-show", "powermenu", "-modi", "powermenu:rofi-power-menu", NULL } } },
/*	{ MODKEY,                  XF86XK_AudioMicMute,        spawn, {.v = (const char*[]){ "sudo", "shutdown", "now", NULL } } },
	{ MODKEY|SHIFT,            XF86XK_AudioMicMute,        spawn, {.v = (const char*[]){ "reboot", NULL } } },*/

	{ 0,                       XF86XK_Tools,    spawn, {.v = (const char*[]){"lxappearance", NULL} } },
	{ 0,                       XF86XK_Explorer, spawn, {.v = (const char*[]){"nitrogen", NULL } } },
	{ 0,                       XK_Print,        spawn, {.v = (const char*[]){ "scrot", "/home/pc/Pictures/scr/1.jpg", NULL } } },
	{ 0,                       XF86XK_Search,   spawn, {.v = (const char*[]){ "fsearch", NULL } } },
	{ 0,                       XF86XK_Display,  spawn, {.v = (const char*[]){ "arandr", NULL } } },
	{ 0,                       XF86XK_LaunchA,  spawn, {.v = (const char*[]){ "thunderbird", NULL } } },

	{ MODKEY,                  XK_e,            spawn, {.v = (const char*[]){ TERMINAL,  "ncmpcpp", NULL } } },
	{ ALTKEY,                  XK_e,            spawn, {.v = (const char*[]){  "ario", NULL } } },
	{ MODKEY|SHIFT,            XK_e,            spawn, {.v = (const char*[]){ "mpc", "update", NULL } } },
        { MODKEY|SHIFT,            XK_bracketleft,  spawn, {.v = (const char*[]){ "mpc", "prev", NULL } } },
        { MODKEY,                  XK_bracketleft,  spawn, {.v = (const char*[]){ "mpc", "seek", "-5", NULL } } },
        { MODKEY,                  XK_bracketright, spawn, {.v = (const char*[]){ "mpc", "toggle", NULL } } },
	{ MODKEY|SHIFT,            XK_bracketright, spawn, {.v = (const char*[]){ "mpc", "stop", NULL } } },
        { MODKEY|SHIFT,            XK_backslash,    spawn, {.v = (const char*[]){ "mpc", "next", NULL } } },
        { MODKEY,                  XK_backslash,    spawn, {.v = (const char*[]){ "mpc", "seek", "+5", NULL } } },


	{ MODKEY,                       XK_b,           togglebar,                {0} },
	{ MODKEY,                       XK_j,           focusstackvis,     {.i = -1 } },
	{ MODKEY,                       XK_k,           focusstackvis,     {.i = +1 } },
        { MODKEY|CONTROL,               XK_j,           focusstackhid,     {.i = -1 } },
        { MODKEY|CONTROL,               XK_k,           focusstackhid,     {.i = +1 } },
        { MODKEY|SHIFT,                 XK_j,           movestack,         {.i = +1 } },
        { MODKEY|SHIFT,                 XK_k,           movestack,         {.i = -1 } },
	{ MODKEY,                       XK_comma,       incnmaster,        {.i = +1 } },
	{ MODKEY,                       XK_period,      incnmaster,        {.i = -1 } },
	{ MODKEY,                       XK_h,           setmfact,        {.f = -0.05} },
	{ MODKEY,                       XK_l,           setmfact,        {.f = +0.05} },
	{ MODKEY,                       XK_Tab,         view,                     {0} },
	{ MODKEY,                       XK_q,           killclient,               {0} },
        { MODKEY|SHIFT,                 XK_q,           bulkill,            {.ui = 1} },
        { MODKEY,                       XK_Prior,       shiftview,          {.i = -1} },
        { MODKEY,                       XK_Next,        shiftview,          {.i = +1} },
	{ MODKEY|CONTROL,               XK_BackSpace,   quit,                     {0} },


	{ MODKEY,                   XK_t,      setlayout,          {.v = &layouts[0]}  },
	{ MODKEY|SHIFT,             XK_t,      setlayout,          {.v = &layouts[1]}  },
	{ MODKEY,                   XK_y,      setlayout,          {.v = &layouts[2]}  },
	{ MODKEY|SHIFT,             XK_y,      setlayout,          {.v = &layouts[3]}  },
	{ MODKEY|SHIFT,             XK_u,      setlayout,          {.v = &layouts[4]}  },
	{ MODKEY,                   XK_u,      setlayout,          {.v = &layouts[5]}  },
	{ MODKEY,                   XK_i,      setlayout,          {.v = &layouts[6]}  },
	{ MODKEY|SHIFT,             XK_i,      setlayout,          {.v = &layouts[7]}  },
	{ MODKEY,                   XK_o,      setlayout,          {.v = &layouts[8]}  },
	{ MODKEY|SHIFT,             XK_o,      setlayout,          {.v = &layouts[9]}  },
	{ MODKEY,                   XK_p,      setlayout,          {.v = &layouts[10]} },
	{ MODKEY|SHIFT,             XK_p,      setlayout,          {.v = &layouts[11]} },

	{ MODKEY,                   XK_space,  togglefloating,                     {0} },
	{ MODKEY,                   XK_f,      togglefullscreen,                   {0} },
	{ MODKEY|SHIFT,             XK_space,  zoom,                               {0} },


	{ MODKEY|CONTROL,             XK_t,      setcfact,       {.f = +0.25} },
	{ MODKEY|CONTROL,             XK_y,      setcfact,       {.f = -0.25} },
	{ MODKEY|CONTROL,             XK_u,      setcfact,       {.f =  0.00} },
	{ MODKEY|ALTKEY,              XK_o,      incrgaps,       {.i = +1 } },
	{ MODKEY|ALTKEY|SHIFT,        XK_o,      incrgaps,       {.i = -1 } },
	{ MODKEY|ALTKEY,              XK_i,      incrigaps,      {.i = +1 } },
	{ MODKEY|ALTKEY|SHIFT,        XK_i,      incrigaps,      {.i = -1 } },
	{ MODKEY|ALTKEY,              XK_p,      incrogaps,      {.i = +1 } },
	{ MODKEY|ALTKEY|SHIFT,        XK_p,      incrogaps,      {.i = -1 } },
	{ MODKEY|ALTKEY,              XK_6,      incrihgaps,     {.i = +1 } },
	{ MODKEY|ALTKEY|SHIFT,        XK_6,      incrihgaps,     {.i = -1 } },
	{ MODKEY|ALTKEY,              XK_7,      incrivgaps,     {.i = +1 } },
	{ MODKEY|ALTKEY|SHIFT,        XK_7,      incrivgaps,     {.i = -1 } },
	{ MODKEY|ALTKEY,              XK_8,      incrohgaps,     {.i = +1 } },
	{ MODKEY|ALTKEY|SHIFT,        XK_8,      incrohgaps,     {.i = -1 } },
	{ MODKEY|ALTKEY,              XK_9,      incrovgaps,     {.i = +1 } },
	{ MODKEY|ALTKEY|SHIFT,        XK_9,      incrovgaps,     {.i = -1 } },
	{ MODKEY|ALTKEY,              XK_0,      togglegaps,     {0} },
	{ MODKEY|ALTKEY|SHIFT,        XK_0,      defaultgaps,    {0} },

	{ MODKEY,                   XK_slash,     focusmon,       {.i = -1 } },
	{ MODKEY,                   XK_semicolon, focusmon,       {.i = +1 } },
	{ MODKEY|SHIFT,             XK_slash,     tagmon,         {.i = -1 } },
	{ MODKEY|SHIFT,             XK_semicolon, tagmon,         {.i = +1 } },

	TAGKEYS(                        XK_1,                      0)
	TAGKEYS(                        XK_2,                      1)
	TAGKEYS(                        XK_3,                      2)
	TAGKEYS(                        XK_4,                      3)
	TAGKEYS(                        XK_5,                      4)
	TAGKEYS(                        XK_6,                      5)
	TAGKEYS(                        XK_7,                      6)
	TAGKEYS(                        XK_8,                      7)
	TAGKEYS(                        XK_9,                      8)
	TAGKEYS(                        XK_0,                      9)
	TAGKEYS(                        XK_minus,                  10)
	TAGKEYS(                        XK_equal,                  11)
	{ MODKEY,                       XK_BackSpace,  view,           {.ui = ~0 } },
	{ MODKEY|SHIFT,                 XK_BackSpace,  tag,            {.ui = ~0 } },


	{ ALTKEY,                 XK_j,      moveresize,     {.v = "0x 25y 0w 0h"} },
	{ ALTKEY,                 XK_k,      moveresize,     {.v = "0x -25y 0w 0h"} },
        { ALTKEY,                 XK_l,      moveresize,     {.v = "25x 0y 0w 0h"} },
        { ALTKEY,                 XK_h,      moveresize,     {.v = "-25x 0y 0w 0h"} },

	{ ALTKEY|SHIFT,           XK_j,      moveresize,     {.v = "0x 0y 0w 25h"} },
	{ ALTKEY|SHIFT,           XK_k,      moveresize,     {.v = "0x 0y 0w -25h"} },
	{ ALTKEY|SHIFT,           XK_l,      moveresize,     {.v = "0x 0y 25w 0h"} },
	{ ALTKEY|SHIFT,           XK_h,      moveresize,     {.v = "0x 0y -25w 0h"} },

	{ ALTKEY|CONTROL,         XK_k,      moveresizeedge, {.v = "t"} },
        { ALTKEY|CONTROL,         XK_j,      moveresizeedge, {.v = "b"} },
        { ALTKEY|CONTROL,         XK_h,      moveresizeedge, {.v = "l"} },
        { ALTKEY|CONTROL,         XK_l,      moveresizeedge, {.v = "r"} },

	{ ALTKEY|CONTROL|SHIFT,   XK_k,      moveresizeedge, {.v = "T"} },
        { ALTKEY|CONTROL|SHIFT,   XK_j,      moveresizeedge, {.v = "B"} },
        { ALTKEY|CONTROL|SHIFT,   XK_h,      moveresizeedge, {.v = "L"} },
        { ALTKEY|CONTROL|SHIFT,   XK_l,      moveresizeedge, {.v = "R"} },

};

/* click can be ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle, ClkClientWin, or ClkRootWin */
static const Button buttons[] = {
	{ ClkLtSymbol,          0,              Button1,        setlayout,      {0} },
	{ ClkLtSymbol,          0,              Button3,        setlayout,      {.v = &layouts[2]} },
	{ ClkWinTitle,          0,              Button2,        zoom,           {0} },
	{ ClkStatusText,        0,              Button1,        spawn,          {.v = (const char*[]){TERMINAL} } },
	{ ClkClientWin,         MODKEY,         Button1,        movemouse,      {0} },
	{ ClkClientWin,         MODKEY,         Button2,        togglefloating, {0} },
	{ ClkClientWin,         MODKEY,         Button3,        resizemouse,    {0} },
	{ ClkTagBar,            0,              Button1,        view,           {0} },
	{ ClkTagBar,            0,              Button3,        toggleview,     {0} },
	{ ClkTagBar,            MODKEY,         Button1,        tag,            {0} },
	{ ClkTagBar,            MODKEY,         Button3,        toggletag,      {0} },
};

