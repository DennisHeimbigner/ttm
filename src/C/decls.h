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
/* Global variables */

/* Cause <argv;0>, <wd>, <time>, <xtime>, etc. to output fixed values
   so that we can compare to baseline without massaging
   When the code gets one of these values, and testing is enabled,
   then if the key is non-NULL, a macro-ized version is returned.
   Later code will detect it and will replace it with the value.
   Note that this means that the macro must be extremely unlikely
   to occur when testing state is disabled. Currently, The macro
   is of the form "~~key~~", where key is one of the values
   in the first column of the following table.
   WARNINGS:
    * If the "~~" sequence changes then modify builtins.h#expandpretends.
    * If the longest key name is >= 32, then modify builtins.h#expandpretends.

## Semantics:

|   Key    |  Macro       |   Pretend Value  |         Semantics             |
|----------|--------------|------------------|-------------------------------|
| argv0    | ~~argv0~~    | ttm.exe          | The value of argv[0]          |
|----------|--------------|------------------|-------------------------------|
| builddir | ~~builddir~~ | /ttm/build       | The builder build directory   |
|          |              |                  | Generally same as srcdir      |
|          |              |                  | unless builder is cmake       |
|----------|--------------|------------------|-------------------------------|
| builder  | ~~builder~~  | cmake            | A name for the build system.  |
|          |              |                  | One of cmake|autotools|make   |
|----------|--------------|------------------|-------------------------------|
| platform | ~~platform~~ | Unix             | The value of argv[0]          |
|----------|--------------|------------------|-------------------------------|
| srcdir   | ~~srcdir~~   | /ttm             | The builder source directory  |
|----------|--------------|------------------|-------------------------------|
| time     | ~~time~~     | 100000000000     | The time since start of epoch |
|----------|--------------|------------------|-------------------------------|
| xtime    | ~~xtime~~    | 100              | The execution time;
|----------|--------------|------------------|-------------------------------|
| wd       | ~~wd~~       | /ttm             | The current working directory |
|----------|--------------|------------------|-------------------------------|

*/
struct TestSpecial {
    enum SpecialEnum id;
    const char* key;
    const char* macro;
    const char* pretend; /* the pretend value for testing */
    char* actual; /* the real value when not testing; computed at run time */
} testspecials[] = {
{SP_ARGV0,    "argv0",    "~~argv0~~",    "ttm.exe",      NULL}, 
{SP_BUILDDIR, "builddir", "~~builddir~~", "/ttm/build",   NULL}, 
{SP_BUILDER,  "builder",  "~~builder~~",  "cmake",        NULL}, 
{SP_PLATFORM, "platform", "~~platform~~", "Unix",         NULL}, 
{SP_SRCDIR,   "srcddir",  "~~srcddir~~",  "/ttm",         NULL}, 
{SP_TIME,     "time",     "~~time~~",     "100000000000", NULL}, 
{SP_XTIME,    "xtime",    "~~xtime~~",    "100",          NULL}, 
{SP_WD,       "wd",       "~~wd~~",       "/ttm",         NULL}, 
{SP_UNDEF,    NULL,       NULL,           NULL,           NULL}
};

/**************************************************/
/* Collect all global state */


static TTMglobal ttmglobal;

#endif /*DECLS_H*/
