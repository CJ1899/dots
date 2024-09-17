static const char black[]       = "#2A303C";
static const char white[]       = "#D8DEE9";
static const char gray2[]       = "#3B4252"; // unfocused window border
static const char gray3[]       = "#606672";
static const char gray4[]       = "#6d8dad";
static const char blue[]        = "#81A1C1";  // focused window border
static const char green[]       = "#89b482";
static const char red[]         = "#d57780";
static const char orange[]      = "#caaa6a";
static const char yellow[]      = "#EBCB8B";
static const char pink[]        = "#e39a83";
static const char col_borderbar[]  = "#2A303C"; // inner border
static const char *colors[][3]      = {
	//               fg         bg         border
	[SchemeNorm] = { pink, black, gray2 },
	[SchemeSel]  = { black, pink,  pink  },
	[SchemeHid]  = { pink,  black, gray2  },
};

