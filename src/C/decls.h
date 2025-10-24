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
/* Defaulters */

static struct Debug
dfalt_debug(void)
{
    struct Debug dfalt;
    memset(&dfalt,0,sizeof(struct Debug));
    return dfalt;
};

/**************************************************/
/* Global variables */

/* Cause <argv;0>, <wd>, <time>, <xtime>, etc. to output fixed values
   so that we can compare to baseline without massaging
   Note: do not declare "const" because we need to modify.
   These testing values can be accessed using ##<ttm;testing;<which>>;
   for example ##<ttm;testing;argv0>.
   Semantics:
	argv0    -- testing executable name
	builddir -- testing the build directory
	time     -- testing fixed time of day in 100'th of a second
	platform -- testing platform OS name
	sep      -- testing path separator
	srcdir   -- testing the source directory
	xtime    -- testing fixed run time
*/
static struct TestSpecial {
    const char* name;
    char* value; /* Filled in by initTTM */
} testspecials[] = {
    {"argv0",    NULL},
    {"builddir", NULL},
    {"platform", NULL},
    {"srcdir",   NULL},
    {"time",     NULL},
    {"xtime",    NULL},
    {NULL,       NULL}, /* table terminator*/
};

static VList* argoptions = NULL; /* command line arguments */
static VList* propoptions = NULL; /* command line properties */

#ifdef TTMGLOBAL
static TTM* ttm = NULL;
#endif

