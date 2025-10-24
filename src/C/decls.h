#ifndef DECLS_H
#define DECLS_H

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
   When the code gets one of these values, and testing is enabled,
   then if the key is non-NULL, it is returned. Later code will
   execute it, which will replace it with the value.

Semantics:
|   Name   |   Pretend Value      |         Semantics             |
|----------|----------------------|-------------------------------|
| argv0    | ttm.exe              | The value of argv[0]          |
|----------|----------------------|-------------------------------|
| builddir | N.A.                 | The builder build directory   |
|          |                      | Generally same as srcdir      |
|          |                      | unless builder is cmake       |
|----------|----------------------|-------------------------------|
| builder  | cmake|autotools|make | The builder build directory   |
|----------|----------------------|-------------------------------|
| platform | Unix                 | The value of argv[0]          |
|----------|----------------------|-------------------------------|
| srcdir   | N.A.                 | The builder source directory  |
|----------|----------------------|-------------------------------|
| time     | 100000000000         | The time since start of epoch |
|----------|----------------------|-------------------------------|
| xtime    | 100                  | The execution time;
|----------|----------------------|-------------------------------|
| wd       | N.A.                 | The current working directory |
|----------|----------------------|-------------------------------|

*/
struct TestSpecial {
    enum SpecialEnum id;
    const char* key;
    const char* macro;
    const char* pretend; /* the pretend value for testing */
    char* actual; /* the real value when not testing; computed at run time */
} testspecials[] = {
{SP_ARGV0,    "argv0",    "#<:;argv0>",    "ttm.exe",      NULL}, 
{SP_BUILDDIR, "builddir", "#<:;builddir>", "/ttm/build",   NULL}, 
{SP_BUILDER,  "builder",  "#<:;builder>",  "cmake",        NULL}, 
{SP_PLATFORM, "platform", "#<:;platform>", "Unix",         NULL}, 
{SP_SRCDIR,   "srcddir",  "#<:;srcddir>",  "/ttm",         NULL}, 
{SP_TIME,     "time",     "#<:;time>",     "100000000000", NULL}, 
{SP_XTIME,    "xtime",    "#<:;xtime>",    "100",          NULL}, 
{SP_WD,       "wd",       "#<:;wd>",       "/ttm",         NULL}, 
{SP_UNDEF,    NULL,       NULL,            NULL,           NULL}
};

static VList* argoptions = NULL; /* command line arguments */
#if 0
static VList* propoptions = NULL; /* command line properties */
#endif

#ifdef TTMGLOBAL
static TTM* ttm = NULL;
#endif

#endif /*DECLS_H*/
