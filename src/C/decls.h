/**************************************************/
/**
Global Declarations
*/

/**
Startup commands: execute before any -e or -f arguments.
If -R command is defined, then do not execute startup commands (raw execution).
Beware that only the defaults instance variables are defined.
*/

static char* startup_commands[] = {
"#<ds;def;<##<ds;name;<text>>##<ss;name;subs>>>##<ss;def;name;subs;text>",
"#<def;defcr;<name;subs;crs;text>;<##<ds;name;<text>>##<ss;name;subs>##<cr;name;crs>>>",
"##<lf;def;defcr>",
NULL
};

/**************************************************/
/* Forward Types */

struct VString;
struct VList;
struct Debug;

/**************************************************/
/* Global variables */

struct Debug dfalt_debug = {
	DFALTTRACE,		/* .trace */
	0,			/* .debug */
	{			/* .xpr */
		{0},		/* /xpr.xbuf */
		0,		/* /xpr.outnl */
	}
};

/* Cause <argv;0>, <wd>, <time>, <xtime> to output fixed values
   so that we can compare to baseline without massaging
*/
struct Testing {
    const char* argv0;
    const char* wd;
    const char* time;
    const char* xtime;
    const char* fps;
} fixedtestvalues = {
    "ttm.exe",		/* .argv0 */
    "/ttm/src/C",	/* .wd */
    ":time:",		/* .time */
    ":xtime:",		/* .xtime */
    "/",		/* .fps */
};

static VList* argoptions = NULL; /* command line arguments */
static VList* propoptions = NULL; /* command line properties */

#ifdef TTMGLOBAL
static TTM* ttm = NULL;
#endif

