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
*/
struct Special {
    const char* argv0;
    const char* wd;
    const char* srcd;
    const char* time;
    const char* xtime;
    const char* sep;
    const char* platform;
} specialnames = {
    ":argv0",     /* .argv0 */
    ":wd:",       /* .wd */
    ":srcd:",     /* .srcd */
    ":time:",     /* .time */
    ":xtime:",    /* .xtime */
    ":sep:",      /* .fps */
    ":platform:", /* .platform */
};

/* Hold the computed special values */
struct Special specialvalues = {
    "ttm.exe", /* .argv0 */
    NULL,      /* .wd */
    NULL,      /* .srcd */
    NULL,      /* .time */
    NULL,      /* .xtime */
    "/",       /* .fps */
    "Unix",    /* .platform */
};

static VList* argoptions = NULL; /* command line arguments */
static VList* propoptions = NULL; /* command line properties */

#ifdef TTMGLOBAL
static TTM* ttm = NULL;
#endif

