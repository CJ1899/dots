static const char black[]       = "#181818"; //OG #222222
static const char gray2[]       = "#005577"; //OG #444444
static const char gray3[]       = "#bbbbbb";
static const char gray4[]       = "#eeeeee";
static const char blue[]        = "#1E1E2E"; //OG #005577 Purple #8D6298 Bl#2e3e64 C #1E1E2E */
static const char *colors[][3]      = {
	//               fg        bg       border
	[SchemeNorm] = { gray3,   black,    0 },
	[SchemeSel]  = { gray4,   blue,   blue },
	[SchemeHid]  = { blue,    black,    gray4 },
};

