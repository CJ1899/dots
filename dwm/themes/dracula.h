static const char black[]       = "#21222C";
static const char white[]       = "#f8f8f2";
static const char gray2[]       = "#282a36"; // unfocused window border
static const char gray3[]       = "#44475a";
static const char gray4[]       = "#282a36";
static const char blue[]        = "#bd93f9";  // focused window border
/*static const char green[]       = "#50fa7b";
static const char red[]         = "#ff5555";
static const char orange[]      = "#ffb86c";
static const char yellow[]      = "#f1fa8c";
static const char pink[]        = "#ff79c6";
static const char col_borderbar[]  = "#21222c"; // inner border*/
static const char *colors[][3]      = {
	//               fg         bg         border
	[SchemeNorm] = { blue, gray4, gray2 },
	[SchemeSel]  = { gray4, blue,  blue  },
	[SchemeHid]  = { blue,  gray4, gray2  },
};

