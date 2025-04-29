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
NULL
};

const struct Properties dfalt_properties = {
.stacksize	= DFALTSTACKSIZE,
.execcount	= DFALTEXECCOUNT,
.showfinal	= DFALTSHOWFINAL,
};

const struct Debug dfalt_debug = {
.trace		= DFALTTRACE,
.verbose	= DFALTVERBOSE,
};

/**************************************************/
/* Forward Types */

struct VString;
struct VList;

/**************************************************/
/* Global variables */

static VList* argoptions = NULL;

#ifdef TTMGLOBAL
static TTM* ttm = NULL;
#endif

