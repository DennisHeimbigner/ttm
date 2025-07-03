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

static VList* argoptions = NULL; /* command line arguments */
static VList* propoptions = NULL; /* command line properties */

#ifdef TTMGLOBAL
static TTM* ttm = NULL;
#endif

