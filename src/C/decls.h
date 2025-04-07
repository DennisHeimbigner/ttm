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

/* The reason for these macros is to get around the fact that changing ttm->vs.active->index invalidates cp8 pointers */
#define TTMINIT(cp8) do{ \
	(cp8)=(utf8*)vsindexp(ttm->vs.active); \
	}while(0)	/* ttm->vs.active->index->cp8 */
#define TTMGETPTR(cp8) do{ \
	(cp8)=(utf8*)vsindexp(ttm->vs.active); \
	}while(0)	/* ttm->vs.active->index->cp8 */
#define TTMSETPTR(pcp8) do{ \
	vsindexskip(ttm->vs.active, \
		((cp8)-(utf8*)vsindexp(ttm->vs.active)));  \
	}while(0)	/* cp8->ttm->vs.active->index */

/* use frame stack  to track depth */
#define frameresult(ttm) (ttm->frames.top >= 0 ? ttm->frames.stack[ttm->frames.top].result : ttm->vs.passive)
#define frameparent(ttm) (ttm->frames.top > 0 ? ttm->frames.stack[ttm->frames.top-1].result : ttm->vs.passive)

/**************************************************/
/* Forward Types */

struct VString;
struct VList;

/**************************************************/
/* Global variables */

static struct VList* argoptions = NULL;

#ifdef TTMGLOBAL
static TTM* ttm = NULL;
#endif

/**************************************************/
/* Macro Functions */

#ifdef CATCH
#define THROW(err) ttmthrow(err)
#else
#define THROW(err) (err)
#endif

#define FAILX(ttm,eno,fmt,...) THROW(failx(ttm,eno,__FILE__,__LINE__,fmt  __VA_OPT__(,) __VA_ARGS__))
#define FAIL(ttm,eno) fail(ttm,eno,__FILE__,__LINE__)
