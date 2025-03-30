/**
This software is released under the terms of the Apache License version 2.
For details of the license, see http://www.apache.org/licenses/LICENSE-2.0.
*/

/**************************************************/

#define VERSION "1.0"

/**************************************************/

#define CATCH
/* Debug has a level attached: 0 is equivalent to undef */
#define DEBUG 1
#define GDB

#if DEBUG > 0
#ifndef GDB
#define GDB
#endif
#endif

#undef TTMGLOBAL

/**************************************************/
/**
(Un)Define these if you do (not) have the specified capability.
This is in lieu of the typical config.h.
*/

/* Define if the equivalent of the standard Unix memove() is available */
#define HAVE_MEMMOVE

/**************************************************/

/* It is not clear what the correct Windows CPP Tag should be.
   Assume _WIN32, but this may not work with cygwin.
   In any case, create our own.
*/

#if (defined _WIN32 || defined _MSC_VER) && !defined(__CYGWIN__)
#define MSWINDOWS 1
#endif

/* Reduce visual studio verbosity */
#ifdef MSWINDOWS
#define _CRT_SECURE_NO_WARNINGS 1
#endif

/**************************************************/
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <errno.h>
#include <assert.h>

#ifdef MSWINDOWS
#include <windows.h>  /* To get GetProcessTimes() */
#include <cstddef.h>
#include <ctype.h>
#include <cstdio.h>
#else /*!MSWINDOWS*/
#include <stddef.h>
#include <stdio.h>
#include <unistd.h> /* This defines getopt */
#include <sys/times.h> /* to get times() */
#include <sys/time.h> /* to get gettimeofday() */
#include <wctype.h>
#endif /*!MSWINDOWS*/

/**************************************************/
/* Unix/Linux versus Windows Definitions */

/* snprintf */
#ifdef MSWINDOWS
#define snprintf _snprintf /* Microsoft has different name for snprintf */
#define strdup _strdup
#endif /*!MSWINDOWS*/

/* Getopt */
#ifdef MSWINDOWS
static char* optarg;		/* global argument pointer */
static int   optind = 0;	/* global argv index */
static int getopt(int argc, char* const* argv, const char* optstring);
#else /*!MSWINDOWS*/
#include <unistd.h> /* This defines getopt */
#endif /*!MSWINDOWS*/

/* Wrap both unix and windows timing in this function */
static long long getRunTime(void);

/* Wrap both unix and windows time of day in this function */
static int timeofday(struct timeval *tv);

/**************************************************/
/* UTF and char Definitions */

/**
We assume that all strings/characters in the external work use utf-8.
This includes at least:
1. files
2. libc

We use the "char" type to indicate the
use of external string/characters.
In practice, this is of course utf8.

Note that, unless otherwise specified, lengths
are in units of bytes, not utf8 codepoints.

Note also that it is assumed that all C string constants
in this file are restricted to US-ASCII, which is a subset
of UTF-8.

This code uses these formats to hold a utf8 character"
1. 4-byte array holding the bytes of the utf8 codepoint.
2. 32-bit integer
The two formats are inter-convertible using a union
with two fields: case 1 field and case 2 field.
Note that it is important that when converting,
that unused bytes be initialized to zero.
*/

/* Maximum codepoint size when using utf8 */
#define MAXCP8SIZE 4

/* Note that we could use char instead of utf8, but
   setting up a typedef helps to clarify the code
   about when using codepoints is important.
*/
typedef unsigned char utf8; /* 8-bit utf char type. */

/* Array for holding a single codepoint; should never be used as C function arg or return value */
typedef utf8 utf8cpa[MAXCP8SIZE];
#define empty_u8cpa {0,0,0,0}

#ifndef MININT
#define MININT -2147483647
#endif
#ifndef MAXINT
#define MAXINT 2147483647
#endif

/**************************************************/
/* Misc Utility Macros */

/* Watch out: x is evaluated multiple times */
#define nullfree(x) do{if(x) free(x);}while(0)

/**************************************************/
/**
Define an internal form of memmove if not
defined by platform.
@param dst where to store bytes
@param src source of bytes
@param len no. of bytes to move
@return void
*/
static void
memmovex(char* dst, char* src, size_t len)
{
#ifdef HAVE_MEMMOVE
    memmove((void*)dst,(void*)src,len*sizeof(utf8));
#else
    src += len;
    dst += len;
    while(len--) {*(--dst) = *(--src);}
#endif
}

/**************************************************/
/**
Constants
*/

/**
This code takes over part of the UTF8 space to store segment
marks and create marks.	 Specifically a segment mark (and
creation mark) are encoded as two bytes: 0x7f,0xB0..0xBF.
The first byte, the ascii DEL character, is assumed to not be used
in real text and is suppressed by the code for reading external data.
It indicates the start of a segment mark.
The second byte is a utf8 continuation code, and so is technically illegal.
The second byte encodes the segment mark index as the continuation code in
the range: 0xB0 .. 0xBF.
This second byte is divided as follows:
1. The (singleton) create mark index: 0xBF, so complete create mark is (0x7F,0xB0)
2. The 62 segment marks: 0xB1 .. 0xBF), so complete create mark is (0x7F,0xB1..0xBF)
*/

/* Segment mark contants (term "mark" comes from gin CalTech TTM). */
#define SEGMARKSIZE 2 /*bytes*/
#define SEGMARK0 (0x7f)
#define CREATEMARK SEGMARK0
/* Segment index constants */
#define SEGINDEX0   0
#define CREATEINDEX 63
/* Masks for set/clr the continuation bits */
#define SEGMARKINDEXMASK ((utf8)0x80)
#define SEGMARKINDEXUNMASK ((utf8)0x3F)
/* Misc */
#define CREATELEN 4 /* # of characters for a create mark */
#define CREATEFORMAT "%04u"
#define MAXSEGMARKS 62

#define MAXARGS 63
#define ARB MAXARGS
#define MAXINCLUDES 1024
#define MAXINTCHARS 32

#define NUL8 '\0'

#define COMMA ','
#define LPAREN '('
#define RPAREN ')'
#define LBRACKET '['
#define RBRACKET ']'

/* When encountered during scan, these non-printable characters are ignored: Must be ASCII */
#define NONPRINTIGNORE "\001\002\003\004\005\006\a\b\n\r\016\017\020\021\022\023\024\025\026\027\030\031\032\e\034\035\036\037\177"

/* These non-printable characters are not ignored */
#define NONPRINTKEEP = "\t\c\f"

#define DFALTSTACKSIZE 64
#define DFALTEXECCOUNT (1<<20)
#define DFALTSHOWFINAL 1
#define DFALTTRACE 0
#define DFALTVERBOSE 1
#define CONTEXTLEN 20

/*Mnemonics*/
#define TRACING 1
#define TOEOS ((size_t)0xffffffffffffffff)

#if DEBUG > 0
#define PASSIVEMAX 20
#define ACTIVEMAX 20
#endif

/* Max number of pushback codepoints for ttmgetc8 */
#define MAXPUSHBACK 4 /*codepoints */

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

/**************************************************/
/* Enumeration types; mostly to simplify switch statements. */

/* Set of Function Consequences */
/* "S" => side effect only, "SV" => side-effect and/or returns a value */
enum FcnSV {
SV_S,
SV_V,
SV_SV
};

/* Current enum of defined properties */
enum PropEnum {
PE_UNDEF=0,
PE_STACKSIZE,
PE_EXECCOUNT,
PE_SHOWFINAL,
};

enum MetaEnum {
ME_UNDEF=0,
ME_SHARP,
ME_SEMI,
ME_ESCAPE,
ME_META,
ME_OPEN,
ME_CLOSE,
ME_LBR,
ME_RBR,
};

/* Track state of a potential function call */
enum FcnCallCases {
FCN_UNDEF,
FCN_ONE,	/* Have # */
FCN_TWO,	/* Have ## */
FCN_THREE,	/* Have ### */
FCN_ACTIVE,	/* Have #< */
FCN_PASSIVE,	/* have ##< */
};

/** ttm command cases */
enum TTMEnum {
TE_UNDEF,
TE_META,
TE_INFO,
TE_NAME,
TE_LIST,
TE_CLASS,
TE_STRING,
TE_BUILTIN,
TE_ALL
};

/**************************************************/
/* Error Numbers */
/* Renamed TTM_EXXX because of multiple Windows conflicts */
/* Note: use non-positive no. to allow using **nix ernno values.*/
typedef enum TTMERR {
TTM_NOERR		= (  0),  /* No error; for completeness */
TTM_ERROR		= ( -1),  /* Generic unknown error */
TTM_ENONAME		= ( -2),  /* Dictionary Name Name Not Found */
TTM_EDUPNAME		= ( -3),  /* Attempt to create duplicate name */
TTM_ENOPRIM		= ( -4),  /* Primitives Not Allowed */
TTM_EFEWPARMS		= ( -5),  /* Too Few Parameters Given */
TTM_EFORMAT		= ( -6),  /* Incorrect Format */
TTM_EQUOTIENT		= ( -7),  /* Quotient Is Too Large */
TTM_EDECIMAL		= ( -8),  /* Decimal Integer Required */
TTM_EMANYDIGITS		= (-10),  /* Too Many Digits */
TTM_EMANYSEGMARKS	= (-11),  /* Too Many Segment Marks */
TTM_EMEMORY		= (-12),  /* Dynamic Storage Overflow */
TTM_EPARMROLL		= (-13),  /* Parm Roll Overflow */
TTM_EINPUTROLL		= (-14),  /* Input Roll Overflow */
TTM_EIO			= (-15),  /* An I/O Error Occurred */
TTM_ETTM		= (-16),  /* A TTM Processing Error Occurred */
TTM_ENOTNEGATIVE	= (-17),  /* Only unsigned decimal integer */
/* Error messages new to this implementation */
TTM_ESTACKOVERFLOW	= (-100), /* Function calls nested too deep */
TTM_ESTACKUNDERFLOW	= (-101), /* Attempt to remote a non-existent function */
TTM_EEXECCOUNT		= (-102),  /* Too many execution calls */
TTM_EMANYINCLUDES	= (-103),  /* Too many includes (obsolete)*/
TTM_EINCLUDE		= (-104),  /* Cannot read Include file */
TTM_ERANGE		= (-105),  /* Index out of legal range */
TTM_EMANYPARMS		= (-106),  /* # Parameters > MAXARGS */
TTM_EEOS		= (-107),  /* Unexpected end of string */
TTM_EASCII		= (-108),  /* Ascii characters only */
TTM_EUTF8		= (-109),  /* Illegal UTF-8 codepoint */
TTM_ETTMCMD		= (-110),  /* Illegal #<ttm> command */
TTM_ETIME		= (-111),  /* Gettimeofday failed */
TTM_EEMPTY		= (-112),  /* Gettimeofday failed */
TTM_ENOCLASS		= (-113),  /* Character Class Name Not Found */
TTM_EINVAL		= (-114),  /* Generic invalid argument */
TTM_ELOCKED		= (-115),  /* Attempt to modify/erase a locked functon */
TTM_EEOF		= (-116),  /* EOF encountered on input*/

#ifdef IMPLEMENTED
/* Errors not implemented */
TTM_EDUPLIBNAME		= (-0),	 /* Name Already On Library */
TTM_ELIBNAME		= (-0),	 /* Name Not On Library */
TTM_ELIBSPACE		= (-0),	 /* No Space On Library */
TTM_EINITIALS		= (-0),	 /* Initials Not Allowed */
TTM_EATTACH		= (-0),	 /* Could Not Attach */
TTM_ESTORAGE		= (-0),	 /* Error In Storage Format */
#endif

} TTMERR;

/**************************************************/
/* "inline" functions */

#define isnul(cp)(*(cp) == NUL8?1:0)
#define isescape(cpa) u8equal(cpa,ttm->meta.escapec)
#define isascii(cpa) (*cpa <= 0x7F)

#if 0
#define ismark(cp) (*(cp) == SEGMARK?1:0)
#endif

#define segmarkindex(cp) ((utf8)(*((cp)+1) & SEGMARKINDEXUNMASK))
#define segmarkindexbyte(index) ((utf8)((index)|SEGMARKINDEXMASK))
#define issegmark(cp) ((*(cp) == SEGMARK0)?1:0)
#define iscreateindex(idx) ((idx) == CREATEINDEX)
#define iscreatemark(cp) (issegmark(cp) && iscreateindex(segmarkindex(cp)))

#define iscontrol(c) ((c) < ' ' || (c) == 127 ? 1 : 0)
#define isdec(c) (((c) >= '0' && (c) <= '9') ? 1 : 0)
#define ishex(c) ((c >= '0' && c <= '9') \
		  || (c >= 'a' && c <= 'f') \
		  || (c >= 'A' && c <= 'F') ? 1 : 0)

#define fromhex(c) \
    (c >= '0' && c <= '9' \
	? (c - '0') \
	: (c >= 'a' && c <= 'f' \
	    ? ((c - 'a') + 10) \
	    : (c >= 'A' && c <= 'F' \
		? ((c - 'a') + 10) \
		: -1)))

#define FAILNONAME(i)  FAILNONAMES(frame->argv[i])
#define FAILNOCLASS(i) FAILNOCLASSS(frame->argv[i])
#define FAILNONAMES(s)	FAILX(ttm,TTM_ENONAME,"Missing dict name=%s\n",(const char*)(s))
#define FAILNOCLASSS(s) FAILX(ttm,TTM_ENOCLASS,"Missing class name=%s\n",(const char*)(s))

/**************************************************/
/**
Structure Type declarations
*/

typedef struct TTM TTM;
typedef struct Function Function;
typedef struct Charclass Charclass;
typedef struct Frame Frame;
typedef struct VString VString;

typedef void (*TTMFCN)(TTM*, Frame*);

/**************************************************/
/* Generic pseudo-hashtable */

/**
HASHSIZE constraints:
1. Must be a power of two so that HASHTABLEMASK becomes a sequence of one bits.
2. Must be <= 256 since it used a single byte to index the top-level of table.
*/

#ifdef GDB
#define HASHSIZE 16
#else
#define HASHSIZE 256
#endif

#define HASHTABLEMASK ((unsigned)(HASHSIZE-1))

struct HashEntry {
    utf8* name;
    unsigned hash;
    struct HashEntry* next;
};

/* Note that the ith entry is actually only used to provide
   a place to store the pointer to the first real entry.
*/
struct HashTable {
    struct HashEntry table[HASHSIZE];
};

struct HashWalk {
    int chain;
    struct HashTable* table;
    struct HashEntry* entry;	
};

/* Generic operations */
static int hashLocate(struct HashTable* table, const utf8* name, struct HashEntry** prevp);
static void hashRemove(struct HashTable* table, struct HashEntry* prev, struct HashEntry* entry);
static void hashInsert(struct HashTable* table, struct HashEntry* prev, struct HashEntry* entry);
static void* hashwalk(struct HashTable* table);
static void hashwalkstop(void* walkstate);
static int hashnext(void* walkstate, struct HashEntry** ithentryp);

/**************************************************/
/**
TTM state object
*/

struct TTM {
    /* TTM Execution Properties */
    struct Properties { /* WARN: reflect changes to PropEnum and its uses */
	size_t stacksize;
	size_t execcount;
	int showfinal; /* 1=>print contents of passive buffer after scan() finishes; 0=>suppress */
    } properties;
    struct Debug {
	/*Debug Flags */
	int trace;   /* Forcibly trace all function executions */
	int verbose; /* Dump more info in the fail function */
	struct Xprint {
	    char xbuf[1<<14];
	    int outnl; /* current xprint line ended with newline */
	} xpr;
    } debug;
    struct Flags {
	int depth; /* track the depth of functions being processed */
	int exit; /* Stop scanning, cleanup, and exit ttm */
	unsigned exitcode;
	unsigned crcounter; /* for cr marks */
	int lineno; /* Estimate of current line no */
    } flags;
    struct OPTS {int quiet; int bare; int interactive; char* executefilename;} opts;
    struct MetaChars {
	utf8cpa sharpc; /* sharp-like char */
	utf8cpa semic; /* ;-like char */
	utf8cpa escapec; /* escape-like char */
	utf8cpa eofc; /* read eof char */
	utf8cpa openc; /* <-like char */
	utf8cpa closec; /* >-like char */
	utf8cpa lbrc; /* escaped string bracket open char */
	utf8cpa rbrc; /* escaped string bracket close char */
    } meta;
    struct Buffers {
	VString* active; /* string being processed */
	VString* passive; /* already processed part of active */
	VString* result; /* contains result strings from functions */
	VString* tmp; /* misc text */
    } vs;
    struct Stack {
	size_t next; /* |stack| == (stacknext) */
	Frame* stack;
    } stack;
    struct IO {
	struct TTMFILE {
	    FILE* file;
	    int isstd;
	    int interactive;
	    /* Provide a stack of pushed codepoints */
	    int npushed; /* number of pushed codepoints -1..(MAXPUSHBACK-1)*/
	    utf8cpa stack[MAXPUSHBACK]; /* support pushback of several full codepoints */
	} *stdin;
	struct TTMFILE* stdout;
	struct TTMFILE* stderr;
    } io;
    /* Following 2 fields are hashtables indexed by low order 7 bits of some character */
    struct Tables {
	struct HashTable dictionary;
	struct HashTable charclasses;
    } tables;
};

/* Convenience */
typedef struct TTMFILE TTMFILE;

const struct Properties dfalt_properties = {
.stacksize	= DFALTSTACKSIZE,
.execcount	= DFALTEXECCOUNT,
.showfinal	= DFALTSHOWFINAL,
};

const struct Debug dfalt_debug = {
.trace		= DFALTTRACE,
.verbose	= DFALTVERBOSE,
};

/**
  Define a ttm frame
*/

struct Frame {
  utf8* argv[MAXARGS+1]; /* Allow for final NULL arg as signal */
  size_t argc;
  int active; /* 1 => # 0 => ## */
  VString* arg; /* collect each frame arg in turn */
};

/**
Function Storage and the Dictionary
*/

/*
Note that for ttm purposes, any entry in the dictionary can be treated
as a function whose name is the entry name and whose body is the value
of the name in the dictionary.

However, the dictionary entry also serves as a stateful string object
-- the entry body -- that has a pointer into the body string where
that pointer can be moved to any point in the body and from which
characters can be extracted.
*/

/* If you add field to this, you need
   to modify especially ttm_ds
*/

/*
Note: the rules of C casting allow this to be
cast to a struct HashEntry.
*/
struct Function {
    struct HashEntry entry;
    struct FcnData { /* structify to simplify re-use under #<ds> */
	int trace;
	int locked;
	int builtin;
	size_t minargs;
	size_t maxargs;
	enum FcnSV sv;
	int novalue; /* suppress return value */
	size_t residual; /* residual index in terms of bytes not codepoints */
	size_t maxsegmark; /* highest segment mark number in use in this string */
	TTMFCN fcn; /* builtin == 1 */
	utf8* body; /* builtin == 0 */
    } fcn;
};

/**
Character Classes and the Charclass table
*/

struct Charclass {
    struct HashEntry entry;
    utf8* characters;
    int negative;
};

/**
Provide a pointer wrapper for
use with functions that take control
of a pointer argument.
*/
//typedef void** Thunk; /* Pointer to a pointer; set to NULL if fcn takes control */
#define Thunk void**
/* Define a macro to record actual type of the ref */
#define THUNK(type,var) Thunk var##p /* varp is type** */
#define THUNKREF(type,var) type* var = (*((type**)(var##p)))
#define THUNKCLAIM(var) *var##p = NULL
#define THUNKIFY(var) ((void**)&(var))

#define SCAN1 do{if((err=scan1(ttm,&cp8))) {stop = 1; goto done;}}while(0)

/* The reason for these macros is to get around the fact that changing ttm->vs.active->index invalidates cp8 pointers */
#define TTMINIT(cp8) do{(cp8)=(utf8*)vsindexp(ttm->vs.active);}while(0)	/* ttm->vs.active->index->cp8 */
#define TTMGETPTR(cp8) do{(cp8)=(utf8*)vsindexp(ttm->vs.active);}while(0)	/* ttm->vs.active->index->cp8 */
#define TTMSETPTR(pcp8) do{vsindexset(ttm->vs.active,\
			 ((cp8)-(utf8*)vsindexp(ttm->vs.active)));}while(0)	/* cp8->ttm->vs.active->index */
#define TTMPTRARG(pcp8) do{*(pcp8)=cp8;}while(0)					/* cp8->*pcp8 */

/**************************************************/
/* Forward */

struct Builtin;

#include "forward.h"

/**************************************************/

#include "vsl.h"
#include "debug.h"

/**************************************************/
/* Global variables */

static VList* eoptions = NULL;
static VList* argoptions = NULL;

#ifdef TTMGLOBAL
static TTM* ttm = NULL;
#endif

/**************************************************/
/**
HashTable Management:
The table is only pseudo-hash simplified by making it an array
of chains indexed by the low order n bits of the name[0].  The
hashcode is just the simple sum of the characters in the name
shifted by 1 bit each.
*/

/* Define a hash computing macro */
static unsigned
computehash(const utf8* name)
{
    unsigned hash;
    const utf8* p;
    for(hash=0,p=name;*p!=NUL8;p++) hash = hash + (*p <<1);
    if(hash==0) hash=1;
    return hash;
}

/* Locate a named entry in the hashtable;
   return 1 if found; 0 otherwise.
   prev is a pointer to HashEntry "before" the found entry.
*/

static int
hashLocate(struct HashTable* table, const utf8* name, struct HashEntry** prevp)
{
    struct HashEntry* prev;
    struct HashEntry* next;
    unsigned index;
    unsigned hash;

    hash = computehash(name);
    if(!(table != NULL && name != NULL))
    assert(table != NULL && name != NULL);
    index = (((unsigned)(name[0])) & HASHTABLEMASK);
    prev = &table->table[index];
    next = prev->next;
    while(next != NULL) {
	if(next->hash == hash
	   && strcmp((char*)name,(char*)next->name)==0)
	    break;
	prev = next;
	next = next->next;
    }
    if(prevp) *prevp = prev;
    return (next == NULL ? 0 : 1);
}

/* Remove an entry specified by argument 'entry'.
   Assumes that the predecessor of entry is specified
   by 'prev' as returned by hashLocate.
*/

static void
hashRemove(struct HashTable* table, struct HashEntry* prev, struct HashEntry* entry)
{
   assert(table != NULL && prev != NULL && entry != NULL);
   assert(prev->next == entry); /* validate the removal */
   prev->next = entry->next;
}


/* Insert an entry specified by argument 'entry'.
   Assumes that the predecessor of entry is specified
   by 'prev' as returned by hashLocate.
*/

static void
hashInsert(struct HashTable* table, struct HashEntry* prev, struct HashEntry* entry)
{
   assert(table != NULL && prev != NULL && entry != NULL);
   assert(entry->hash != 0);
   entry->next = prev->next;
   prev->next = entry;
}

static void*
hashwalk(struct HashTable* table)
{
    struct HashWalk* walker = NULL;
    if((walker = calloc(sizeof(struct HashWalk),1))==NULL) return NULL;
    assert(walker != NULL);
    walker->table = table;
    walker->chain = 0;
    walker->entry = walker->table->table[walker->chain].next;
    return (void*)walker;
}

static void
hashwalkstop(void* walkstate)
{
    if(walkstate != NULL) {
	free(walkstate);
    }
}

static int
hashnext(void* walkstate, struct HashEntry** ithentryp)
{
    struct HashWalk* walker = (struct HashWalk*)walkstate;
    while(walker->entry == NULL) { /* Find next non-empty chain */
	walker->chain++;
	if(walker->chain >= HASHSIZE) return 0; /* No more chains to search */
	walker->entry = walker->table->table[walker->chain].next;
    }
    if(ithentryp) *ithentryp = walker->entry;
    walker->entry = walker->entry->next;
    return 1;
}

/**************************************************/
/* Provide subtype specific wrappers for the HashTable operations. */

static Function*
dictionaryLookup(TTM* ttm, const utf8* name)
{
    struct HashTable* table = &ttm->tables.dictionary;
    struct HashEntry* prev;
    struct HashEntry* entry;
    Function* def = NULL;

    if(hashLocate(table,name,&prev)) {
	entry = prev->next;
	def = (Function*)entry;
    } /*else Not found */
    return def;
}

static Function*
dictionaryRemove(TTM* ttm, const utf8* name)
{
    struct HashTable* table = &ttm->tables.dictionary;
    struct HashEntry* prev;
    struct HashEntry* entry;
    Function* def = NULL;

    if(hashLocate(table,name,&prev)) {
	entry = prev->next;
	hashRemove(table,prev,entry);
	entry->next = NULL;
	def = (Function*)entry;
    } /*else Not found */
    return def;
}

static int
dictionaryInsert(TTM* ttm, Function* fcn)
{
    struct HashTable* table = &ttm->tables.dictionary;
    struct HashEntry* prev;

    if(hashLocate(table,fcn->entry.name,&prev)) return 0;
    /* Does not already exist */
    fcn->entry.hash = computehash(fcn->entry.name);/*make sure*/
    hashInsert(table,prev,(struct HashEntry*)fcn);
    return 1;
}

static Charclass*
charclassLookup(TTM* ttm, const utf8* name)
{
    struct HashTable* table = &ttm->tables.charclasses;
    struct HashEntry* prev;
    struct HashEntry* entry;
    Charclass* def = NULL;

    if(hashLocate(table,name,&prev)) {
	entry = prev->next;
	def = (Charclass*)entry;
    } /*else Not found */
    return def;
}

static Charclass*
charclassRemove(TTM* ttm, const utf8* name)
{
    struct HashTable* table = &ttm->tables.charclasses;
    struct HashEntry* prev;
    struct HashEntry* entry;
    Charclass* def = NULL;

    if(hashLocate(table,name,&prev)) {
	entry = prev->next;
	hashRemove(table,prev,entry);
	entry->next = NULL;
	def = (Charclass*)entry;
    } /*else Not found */
    return def;
}

static int
charclassInsert(TTM* ttm, Charclass* cl)
{
    struct HashTable* table = &ttm->tables.charclasses;
    struct HashEntry* prev;
    if(hashLocate(table,cl->entry.name,&prev))
	return 0;
    /* Not already exists */
    cl->entry.hash = computehash(cl->entry.name);
    hashInsert(table,prev,(struct HashEntry*)cl);
    return 1;
}

/**************************************************/

static TTM*
newTTM(struct Properties* initialprops)
{
    TTM* ttm = (TTM*)calloc(1,sizeof(TTM));
    if(ttm == NULL) return NULL;
    ttm->properties = propertiesclone(initialprops);
    ascii2u8('#',ttm->meta.sharpc);
    ascii2u8(';',ttm->meta.semic);
    ascii2u8('\\',ttm->meta.escapec);
    ascii2u8('\n',ttm->meta.eofc);
    ascii2u8('<',ttm->meta.openc);
    ascii2u8('>',ttm->meta.closec);
    memcpy(ttm->meta.lbrc,ttm->meta.openc,sizeof(utf8cpa));
    memcpy(ttm->meta.rbrc,ttm->meta.closec,sizeof(utf8cpa));
    ttm->vs.active = vsnew();
    ttm->vs.passive = vsnew();
    ttm->vs.result = vsnew();
    ttm->vs.tmp = vsnew();
    ttm->stack.next = 0;
    ttm->stack.stack = (Frame*)calloc(sizeof(Frame),initialprops->stacksize);
    if(ttm->stack.stack == NULL) FAIL(ttm,TTM_EMEMORY);
    memset((void*)&ttm->tables.dictionary,0,sizeof(ttm->tables.dictionary));
    memset((void*)&ttm->tables.charclasses,0,sizeof(ttm->tables.charclasses));
#if DEBUG > 0
    ttm->debug.trace = 1;
#endif
    return ttm;
}

static void
freeTTM(TTM* ttm)
{
    vsfree(ttm->vs.active);
    vsfree(ttm->vs.passive);
    vsfree(ttm->vs.result);
    vsfree(ttm->vs.tmp);
    freeFramestack(ttm,ttm->stack.stack,ttm->stack.next);
    clearDictionary(ttm,&ttm->tables.dictionary);
    clearCharclasses(ttm,&ttm->tables.charclasses);
    ttmclose(ttm,ttm->io.stdin);
    ttmclose(ttm,ttm->io.stdout);
    ttmclose(ttm,ttm->io.stderr);
    nullfree(ttm->opts.executefilename);
    free(ttm);
}

/**************************************************/

static void
clearHashEntry(struct HashEntry* entry)
{
    if(entry == NULL) return;
    nullfree(entry->name);
    memset(entry,0,sizeof(struct HashEntry));
}

/**************************************************/

/* Manage the frame stack */
static Frame*
pushFrame(TTM* ttm)
{
    Frame* frame;
    if(ttm->stack.next >= ttm->properties.stacksize)
	FAIL(ttm,TTM_ESTACKOVERFLOW);
    frame = &ttm->stack.stack[ttm->stack.next];
    frame->argc = 0;
    frame->active = 0;
    ttm->stack.next++;
    frame->arg = vsnew();
    return frame;
}

static void
popFrame(TTM* ttm)
{
    Frame* frame;
    if(ttm->stack.next == 0)
	FAIL(ttm,TTM_ESTACKUNDERFLOW); /* Actually an internal error */
    ttm->stack.next--;
    if(ttm->stack.next == 0)
	frame = NULL;
    else
	frame = &ttm->stack.stack[ttm->stack.next-1];
    clearFrame(ttm,frame);
}

static void
clearFrame(TTM* ttm, Frame* frame)
{
    size_t i;
    if(frame == NULL) return;
    for(i=0;i<frame->argc;i++)
	if(frame->argv[i] != NULL) free(frame->argv[i]);
    vsfree(frame->arg);
}

static void
freeFramestack(TTM* ttm, Frame* stack, size_t stacksize)
{
    size_t i;
    for(i=0;i<stacksize;i++) {
	Frame* f = &stack[i];
	clearFrame(ttm,f);
    }
    nullfree(stack);
}

/**************************************************/
static Function*
newFunction(TTM* ttm)
{
    Function* f = (Function*)calloc(1,sizeof(Function));
    if(f == NULL) FAIL(ttm,TTM_EMEMORY);
    return f;
}

static void
freeFunction(TTM* ttm, Function* f)
{
    assert(f != NULL);
    if(!f->fcn.builtin) nullfree(f->fcn.body);
    clearHashEntry(&f->entry);
    free(f);
}

static void
clearDictionary(TTM* ttm, struct HashTable* dict)
{
    size_t i;
    for(i=0;i<HASHSIZE;i++) {
	struct HashEntry* cur = dict->table[i].next; /* First entry is a placeholder */
	while(cur != NULL) {
	    struct HashEntry* next = cur->next;
	    struct Function* fcn = (struct Function*)cur;
	    freeFunction(ttm,fcn);
	    cur = next;
	}
	dict->table[i].next = NULL;
    }
    memset(dict,0,sizeof(struct HashTable));
}

/**************************************************/
static Charclass*
newCharclass(TTM* ttm)
{
    Charclass* cl = (Charclass*)calloc(1,sizeof(Charclass));
    if(cl == NULL) FAIL(ttm,TTM_EMEMORY);
    return cl;
}

static void
freeCharclass(TTM* ttm, Charclass* cl)
{
    assert(cl != NULL);
    nullfree(cl->characters);
    clearHashEntry(&cl->entry);
    free(cl);
}

static void
clearCharclasses(TTM* ttm, struct HashTable* charclasses)
{
    size_t i;
    for(i=0;i<HASHSIZE;i++) {
	struct HashEntry* cur = charclasses->table[i].next; /* First entry is a placeholder */
	while(cur != NULL) {
	    struct HashEntry* next = cur->next;
	    struct Charclass* cc = (struct Charclass*)cur;
	    freeCharclass(ttm,cc);
	    cur = next;
	}
	charclasses->table[i].next = NULL;
    }
    memset(charclasses,0,sizeof(struct HashTable));
}

static int
charclassMatch(utf8* cp, utf8* charclass)
{
    utf8* p;
    for(p=charclass;*p;p+=u8size(p)) {
	if(u8equal(cp,p)) return 1;
    }
    return 0;
}

/* Property enum detector */
static enum PropEnum
propenumdetect(const utf8* s)
{
    if(strcmp("stacksize",(const char*)s)==0) return PE_STACKSIZE;
    if(strcmp("execcount",(const char*)s)==0) return PE_EXECCOUNT;
    if(strcmp("showfinal",(const char*)s)==0) return PE_SHOWFINAL;
    return PE_UNDEF;
}

/* MetaEnum detector */
static enum MetaEnum
metaenumdetect(const utf8* s)
{
    if(strcmp("sharp",(const char*)s)==0) return ME_SHARP;
    if(strcmp("semi",(const char*)s)==0) return ME_SEMI;
    if(strcmp("escape",(const char*)s)==0) return ME_ESCAPE;
    if(strcmp("meta",(const char*)s)==0) return ME_META;
    if(strcmp("open",(const char*)s)==0) return ME_OPEN;
    if(strcmp("close",(const char*)s)==0) return ME_CLOSE;
    if(strcmp("lbr",(const char*)s)==0) return ME_LBR;
    if(strcmp("rbr",(const char*)s)==0) return ME_RBR;
    return ME_UNDEF;
}

/* MetaEnum detector */
static enum TTMEnum
ttmenumdetect(const utf8* s)
{
    if(strcmp("meta",(const char*)s)==0) return TE_META;
    if(strcmp("info",(const char*)s)==0) return TE_INFO;
    if(strcmp("name",(const char*)s)==0) return TE_NAME;
    if(strcmp("class",(const char*)s)==0) return TE_CLASS;
    if(strcmp("string",(const char*)s)==0) return TE_STRING;
    if(strcmp("list",(const char*)s)==0) return TE_LIST;
    if(strcmp("all",(const char*)s)==0) return TE_ALL;
    if(strcmp("builtin",(const char*)s)==0) return TE_BUILTIN;
    return TE_UNDEF;
}

/**************************************************/

/**
This is basic top level scanner.
*/
static TTMERR
scan(TTM* ttm)
{
    TTMERR err = TTM_NOERR;
    int stop = 0;
    utf8* cp8;
    int ncp;

    TTMGETPTR(cp8); ncp = u8size(cp8);
    do {
	stop = 0;
statetrace();
	/* Look for special chars */
	if(isnul(cp8)) { /* EOS => stop */
	    stop = 1;
	} else if(isascii(cp8) && strchr(NONPRINTIGNORE,*cp8)) {
	    /* non-printable ignored chars must be ASCII */
	    cp8 += ncp;
	} else if(isescape(cp8)) {
	    cp8 += ncp; ncp = u8size(cp8);
	    if(isnul(cp8)) { /* escape as very last character in buffer */
		vsappendn(ttm->vs.passive,(const char*)ttm->meta.escapec,u8size(ttm->meta.escapec));
	    } else {
		vsappendn(ttm->vs.passive,(const char*)cp8,ncp);
	    }
	    cp8 += ncp;
	} else if(u8equal(cp8,ttm->meta.sharpc)) {/* possible fcn call */
	    if((err = processfcn(ttm))) goto done;
	    TTMGETPTR(cp8); ncp = u8size(cp8);
	} else if(u8equal(cp8,ttm->meta.openc)) {/* <...> nested brackets */
	    if((err=collectescaped(ttm,ttm->vs.passive))) goto done;
	    TTMGETPTR(cp8); ncp = u8size(cp8);
	    /* collected string will have been appended to end of  ttm->vs.passive */
	} else { /* All other characters */
	    vsappendn(ttm->vs.passive,(const char*)cp8,ncp);
	    cp8 += ncp;
	}
    } while(!stop);

    /** When we get here, we are finished, so clean up. */
//    TTMSETPTR(cp8); /* make sure ttm->vs.active->index is set */
    vsindexset(ttm->vs.active,0); /* make sure ttm->vs.active->index is set */
    
    /** The TTM documentation is silent about what to do with
	the final content of the passive buffer.
    */
    {
	if(ttm->properties.showfinal) {
	    printstring(ttm,(const utf8*)"\n",ttm->io.stdout);
	    printstring(ttm,(const utf8*)print2len(ttm->vs.passive),ttm->io.stdout);
	}
	vsclear(ttm->vs.passive);
    }
done:
    return THROW(err);
}

/*
Helper function for scan().
Get validated pointer and size for current codepoint to be SCAN'd,
and move past that codepoint.
@param ttm
@param cp8p store current codepoint pointer here
@return TTMERR
*/
static TTMERR
scan1(TTM* ttm,utf8** pcp8)
{
    TTMERR err = TTM_NOERR;
    utf8* cp8;
    int ncp;

    TTMGETPTR(cp8); ncp = u8size(cp8);
    /* If already at EOS, then attempt to move past it is an error */
    if(isnul(cp8)) {err = THROW(TTM_EEOS); goto done;}
    cp8 = (utf8*)vsindexskip(ttm->vs.active,ncp);
    ncp = u8size(cp8);
    if(*cp8 == '\n') ttm->flags.lineno++;
    TTMPTRARG(pcp8);
done:
    return THROW(err);
}

/**
Helper fcn for scan().
Check for an occurrence of the start of a function.
This means either '#<" or '##<'.
There is an odd possibility that you will encounter '###<f..>'
and the unlikely possibility exists that the function f
returns a partial function invocation so you get '#<g...>'
The CalTech documentation of the scan algorithm does not
allow this, so '###' is treated almost the same as '##X'
except that the leading sharp is passed and the scan cycle
starts with the two right-most # characters.

At the start of this function, it is assumed the scan is at
the a sharp character (as represented by ttm->meta.sharpc).
@param ttm
@param pcp8 (in/out) ptr to initial character
@param pfcncase return the determined fcn case
@return
*/
static TTMERR
testfcnprefix(TTM* ttm, enum FcnCallCases* pfcncase)
{
    TTMERR err = TTM_NOERR;
    utf8* cp8;
    utf8cpa shorops = empty_u8cpa;
    utf8cpa opens = empty_u8cpa;
    enum FcnCallCases fcncase = FCN_UNDEF;

    TTMGETPTR(cp8); /* ptr to # */
    assert(u8equal(cp8,ttm->meta.sharpc));
    /* peek the next two chars after initial sharp */
    if((err=peek(ttm->vs.active,1,shorops))) goto done; /* peek a # or < */
    if((err=peek(ttm->vs.active,2,opens))) goto done;	/* peek at > */
    if(u8equal(shorops,ttm->meta.openc)) /* #< active call */
	fcncase = FCN_ACTIVE;
    else if(u8equal(shorops,ttm->meta.sharpc) && u8equal(opens,ttm->meta.openc)) /* ##< passive call */
	fcncase = FCN_PASSIVE;
    else if(u8equal(shorops,ttm->meta.sharpc) && u8equal(opens,ttm->meta.sharpc)) /* ### */
	fcncase = FCN_ONE;
    else if(isnul(shorops))
	fcncase = FCN_ONE;
    else if(u8equal(shorops,ttm->meta.sharpc))
	fcncase = (isnul(opens) ? FCN_TWO : FCN_THREE);
    else
	fcncase = (isnul(shorops) ? FCN_ONE : FCN_TWO);

    switch (fcncase) {
    case FCN_PASSIVE:
	/* skip '#' */
	cp8 += u8size(ttm->meta.sharpc);
	/* fall thru */
    case FCN_ACTIVE:
	/* ship '#' and '<' */
	cp8 += (u8size(ttm->meta.sharpc)+u8size(ttm->meta.openc));
	/* Should now be at the start of argv[0] (the fcn name) */
	break;
    case FCN_ONE: case FCN_TWO: case FCN_THREE:
	break;
    case FCN_UNDEF: break;
    }
    TTMSETPTR(cp8);
    *pfcncase = fcncase;
done:
    return err;
}

static TTMERR
exec(TTM* ttm, Frame* frame)
{
    TTMERR err = TTM_NOERR;
    Function* fcn;
#if DEBUG > 0
dumpframe(ttm,frame);
#endif
    if(ttm->properties.execcount-- <= 0) {err = THROW(TTM_EEXECCOUNT); goto done;}
    if(ttm->flags.exit) goto done;
    /* Now execute this function, which will leave result in ttm->vs.result */
    if(frame->argc == 0) {err = FAILX(ttm,TTM_EFEWPARMS,NULL); goto done;}
    if(strlen((const char*)frame->argv[0])==0) {err = FAILX(ttm,TTM_EEMPTY,NULL); goto done;}
    /* Locate the function to execute */
    if((fcn = dictionaryLookup(ttm,frame->argv[0]))==NULL) {err = FAILNONAME(0); goto done;}
    if(fcn->fcn.minargs > (frame->argc - 1)) /* -1 to account for function name*/
	{err = THROW(TTM_EFEWPARMS); goto done;}
    /* Reset the result buffer */
    vsclear(ttm->vs.result);
    if(ttm->debug.trace || fcn->fcn.trace)
	trace(ttm,1,TRACING);
    if(fcn->fcn.builtin) {
	fcn->fcn.fcn(ttm,frame);
	if(fcn->fcn.novalue) vsclear(ttm->vs.result);
	if(ttm->flags.exit) goto done;
    } else /* invoke the pseudo function "call" */
	call(ttm,frame,fcn->fcn.body);

    if(ttm->debug.trace || fcn->fcn.trace)
	trace(ttm,0,TRACING);

    /* Now, put the result into the appropriate active or passive buffer */
    if(vslength(ttm->vs.result) > 0) {
	if(frame->active) {
	    /* insert result at current index so it will be actively scanned */
	    /* WARNING: invalidates any pointers into active */
	    (void)vsindexinsertn(ttm->vs.active,vscontents(ttm->vs.result),vslength(ttm->vs.result));
	} else {/*!frame->active*/
	    vsappendn(ttm->vs.passive,vscontents(ttm->vs.result),vslength(ttm->vs.result));
	}
	vsclear(ttm->vs.result);
#if DEBUG > 1
xprintf(ttm,"context:\n\tpassive=|%s|\n\tactive=|%s|\n",
	vscontents(ttm->vs.passive),vscontents(ttm->vs.active)+vsindex(ttm->vs.active));
#endif
    }
done:
    popFrame(ttm);
    return THROW(err);
}

static TTMERR
processfcn(TTM* ttm)
{
    TTMERR err = TTM_NOERR;
    enum FcnCallCases fcncase = FCN_UNDEF;
    Frame* frame = NULL;
    size_t startpos,endpos;
    utf8* cp8;

    TTMGETPTR(cp8);
    startpos = vsindex(ttm->vs.active); /* remember where we start parsing */
    if((err=testfcnprefix(ttm,&fcncase))) goto done;
    /* if testfcnprefix indicates a function call, then
       cp8 should be pointing to first character of argv[0] */
    TTMGETPTR(cp8);
    if(fcncase == FCN_PASSIVE || fcncase == FCN_ACTIVE) {
	/* Parse and collect the args of the function call */
	if((err=collectargs(ttm,&frame))) goto done;
	TTMGETPTR(cp8);
	endpos = vsindex(ttm->vs.active); /* remember where we end parsing */
	vsremoven(ttm->vs.active,startpos,(endpos-startpos)); /* remove the collected args from the active buffer */
    }
    switch (fcncase) {
    case FCN_PASSIVE: /* execute */
	frame->active = 0; 
	if((err=exec(ttm,frame))) goto done;
	/* WARNING: invalidates pointers into active */
	break;
    case FCN_ACTIVE: /* execute */
	frame->active = 1;
	/* execute the function */
	if((err=exec(ttm,frame))) goto done;
	/* WARNING: invalidates pointers into active */
	TTMGETPTR(cp8);
	break;
    case FCN_ONE: /* Pass one char */
	cp8 = (utf8*)vsindexskip(ttm->vs.active,u8size(cp8));
	vsappendn(ttm->vs.passive,(const char*)cp8,u8size(cp8));
	break;
    case FCN_TWO: /* pass two */
	cp8 = (utf8*)vsindexskip(ttm->vs.active,u8size(cp8));
	vsappendn(ttm->vs.passive,(const char*)cp8,u8size(cp8));
	cp8 = (utf8*)vsindexskip(ttm->vs.active,u8size(cp8));
	vsappendn(ttm->vs.passive,(const char*)cp8,u8size(cp8));
	break;
    case FCN_THREE: /* pass SSX */
	cp8 = (utf8*)vsindexskip(ttm->vs.active,u8size(cp8));
	vsappendn(ttm->vs.passive,(const char*)cp8,u8size(cp8));
	cp8 = (utf8*)vsindexskip(ttm->vs.active,u8size(cp8));
	vsappendn(ttm->vs.passive,(const char*)cp8,u8size(cp8));
	cp8 = (utf8*)vsindexskip(ttm->vs.active,u8size(cp8));
	vsappendn(ttm->vs.passive,(const char*)cp8,u8size(cp8));
	break;
    case FCN_UNDEF: abort(); /* Should never happen */
    }
done:
   return err;
}

/**
Construct a frame; leave ttm->vs.active pointing just past the closing '>'
@param ttm
@param framep store created frame
@param cp8p (in/out) on entry, this points pointer to first arg,
		     on exit this returns pointer to char just after
		     last consumed char (usually '>')
@return TTMERR
*/
static TTMERR
collectargs(TTM* ttm, Frame** framep)
{
    TTMERR err = TTM_NOERR;
    Frame* frame = NULL;
    int stop;
    utf8* cp8;
    int ncp;
    VString* arg = NULL;

    TTMGETPTR(cp8); ncp = u8size(cp8);

    frame = pushFrame(ttm);
    arg = frame->arg;

    /* Scan() will have left the index for active buffer at the
       start of function name */

    ttm->flags.depth++;
    stop = 0;
statetrace();
#if DEBUG > 1
fprintf(stderr,"\targ=|%s|\n",printsubseq(ttm->vs.passive,argstart,vslength(ttm->vs.passive)));
#endif /* DEBUG > 1 */
    arg = vsnew();
    do { /* Collect 1 arg */
	/* Look for special chars */
	if(isnul(cp8)) { /* EOS => stop */
	    stop = 1;
	} else if(isascii(cp8) && strchr(NONPRINTIGNORE,*cp8)) {
	    /* non-printable ignored chars must be ASCII */
	    SCAN1;
	} else if(isescape(cp8)) {
	    SCAN1;
	    if(isnul(cp8)) { /* escape as very last character in buffer */
		vsappendn(arg,(const char*)ttm->meta.escapec,u8size(ttm->meta.escapec));
	    } else {
		vsappendn(arg,(const char*)cp8,ncp);
	    }
	} else if(u8equal(cp8,ttm->meta.sharpc)) {
	    if((err = processfcn(ttm))) goto done;
	    TTMGETPTR(cp8); ncp = u8size(cp8);
	} else if(u8equal(cp8,ttm->meta.lbrc)) {
	    if((err=collectescaped(ttm,arg))) goto done;
	    TTMGETPTR(cp8); ncp = u8size(cp8);
	} else if(u8equal(cp8,ttm->meta.semic) || u8equal(cp8,ttm->meta.rbrc)) {
	    if(frame->argc >= MAXARGS) {err = THROW(TTM_EMANYPARMS); goto done;}
	    if(u8equal(cp8,ttm->meta.rbrc))
		ttm->flags.depth--;
#if DEBUG > 1
xprintf(ttm,"collectargs: argv[%d]=|%s|\n",frame->argc,vscontents(arg));
#endif /* DEBUG > 1 */
	    frame->argv[frame->argc++] = (utf8*)vsextract(arg);
	    if(u8equal(cp8,ttm->meta.rbrc))
		stop = 1;
	    vsclear(arg);
	    SCAN1; /* move past the ';' or '>' */
	} else { /* ordinary char */
	    vsappendn(arg,(const char*)cp8,ncp);
	    SCAN1;
	}
    } while(!stop);
    /* add null to end of the arg list as signal */
    frame->argv[frame->argc] = NULL;
    *framep = frame; frame = NULL;
    TTMSETPTR(cp8);
done:
    if(frame != NULL) popFrame(ttm);
    return THROW(err);
}

/**
Collect a bracketed string and append to specified vstring.
Collection starts at the ttm->vs.active index and at the end, the ttm->vs.tmp index
points just past the bracketed string openc.
@param ttm
@param dst where to store bracketed string
@param src from where to get bracketed string
@param dst where to append the collect string
@return TTMERR
Note: on entry, the src points to the codepoint just after the opening bracket
*/
static TTMERR
collectescaped(TTM* ttm, VString* dst)
{
    TTMERR err = TTM_NOERR;
    int stop = 0;
    utf8* cp8;
    int depth = 0;
    int ncp;
    
    TTMGETPTR(cp8); ncp = u8size(cp8);

    /* Accumulate the bracketed string in dst */
    /* Assume the initial LBRACKET was not consumed by caller */
    do {
	stop = 0;
	/* Look for special chars */
	if(isnul(cp8)) { /* EOS => stop */
	    stop = 1;
	} else if(isascii(cp8) && strchr(NONPRINTIGNORE,*cp8)) {
	    /* non-printable ignored chars must be ASCII */
	    cp8 += ncp;
	} else if(isescape(cp8)) {
	    cp8 += ncp; ncp = u8size(cp8);
	    if(depth > 0 || isnul(cp8)) /* escape nested or very last character in buffer */
		vsappendn(dst,(const char*)ttm->meta.escapec,u8size(ttm->meta.escapec));
	    if(!isnul(cp8))
		vsappendn(dst,(const char*)cp8,u8size(cp8));
	    cp8 += ncp;
	} else if(u8equal(cp8,ttm->meta.lbrc)) {/* <... nested brackets */
	    if(depth > 0)
		vsappendn(dst,(const char*)ttm->meta.lbrc,u8size(ttm->meta.lbrc));
	    depth++;
	    cp8 += ncp;
	} else if(u8equal(cp8,ttm->meta.rbrc)) {/* ...> nested brackets */
	    depth--;
	    if(depth > 0)
		vsappendn(dst,(const char*)ttm->meta.rbrc,u8size(ttm->meta.rbrc));
	    if(depth == 0)
		stop = 1;
	    cp8 += ncp;
	} else { /* All other characters */
	    vsappendn(dst,(const char*)cp8,u8size(cp8));
	    cp8 += ncp;
	}
    } while(!stop);
    TTMSETPTR(cp8);
    return err;
}

/**************************************************/
/**
Execute a non-builtin function
@param ttm
@param frame args for function call
@param body for user-defined functions.
*/
static TTMERR
call(TTM* ttm, Frame* frame, utf8* body)
{
    TTMERR err = TTM_NOERR;
    utf8* p;
    char crval[CREATELEN+1];
    int crlen;
    int ncp = 0;

    /* Compute the body using ttm->vs.result  */
    vsclear(ttm->vs.result);
    for(p=body;;p+=ncp) {
	ncp = u8size(p);
	if(isnul(p))
	    break;
	else if(issegmark(p)) {
	    size_t segindex = (size_t)segmarkindex(p);
	    if(segindex == CREATEINDEX) {
		/* Construct the cr value */
		snprintf(crval,sizeof(crval),CREATEFORMAT,++ttm->flags.crcounter);
		crlen = strlen(crval);
		vsappendn(ttm->vs.result,crval,crlen);
	    } else if(segindex < frame->argc) {
		utf8* arg = frame->argv[segindex+1]; /* +1 to skip argv[0] */
		vsappendn(ttm->vs.result,(const char*)arg,strlen((const char*)arg));
	    } /* else treat as null string */
	} else { /* pass the codepoint */
	    int ncp = u8size(p);
	    vsappendn(ttm->vs.result,(char*)p,ncp);
	}
    }
    return THROW(err);
}

/**************************************************/
/* Built-in Support Procedures */

/**
In order to a void spoofing, the string to be output is modified
to escape all control characters except '\n' and '\r'.
*/
static TTMERR
printstring(TTM* ttm, const utf8* s8, TTMFILE* output)
{
    TTMERR err = TTM_NOERR;
    int slen = 0;

    if(s8 == NULL) goto done;
    if((slen = strlen((const char*)s8))==0) goto done;
    for(;*s8;s8 += u8size(s8)) {
	if(issegmark(s8)) {
	    char info[16];
	    size_t segindex = (size_t)segmarkindex(s8);
	    if(iscreateindex(segindex))
		snprintf(info,sizeof(info),"^%02x",(unsigned)segindex);
	    else
		snprintf(info,sizeof(info),"^CR");
	    fputs(info,output->file);
	} else {
#ifdef NO
	    if(deescape(s8,cpa) < 0) {err = THROW(TTM_EUTF8); goto done;}
	    /* print codepoint */
	    ttmputc8(ttm,cpa,output);
#else
	    /* print codepoint */
	    ttmputc8(ttm,s8,output);
#endif
	}
    }
    fflush(output->file);
done:
    return THROW(err);
}

/**
Return a string created from the defined content of a VString,
which is 0 .. vslength(vs).
@param vs)
@return defined content
*/
static const char*
print2len(VString* vs)
{
    return printsubseq(vs,0,vs->length);
}

static const char*
printsubseq(VString* vs, size_t argstart , size_t argend)
{
    static char ss[1<<12];
    char* ct = vscontents(vs);
    size_t len = (argend - argstart);
    memcpy(ss,ct+argstart,len);
    ss[len] = '\0';
    return ss;
}

/**
It is sometimes useful to print a string, but to convert selected
control chars to escaped form: e.g <cr> to '\n'.  Same goes for
encountered segment and creation marks.	 So, given a pointer, s8, to a
utf8 char and ctrls, a string of control chars, this function creates
a modified copy of the string where where the characters in ctrl are
left as is, but all other controls are converted to escaped form.  In
addition, any segment/creation marks are converted to printable form.
The resulting copy is returned.
@param src utf8 string
@param ctrls specify control chars to leave unchanged
@param pfinallen store the final length of result
@return copy of s8 with controls modified || NULL if non codepoint encountered
*/
static utf8*
printclean(const utf8* s8, char* ctrls, size_t* pfinallen)
{
    utf8* clean = NULL;
    char* p = NULL;
    char* q = NULL;
    size_t len = 0;

    if(ctrls == NULL) ctrls = "";
    len = strlen((const char*)s8);
    clean = (utf8*)malloc(sizeof(utf8)*((4*len)+1)); /* max possible */
    for(p=(char*)s8,q=(char*)clean;*p;) {
	len = u8size((const utf8*)p);
	switch (len) {
	case 0: /* illegal utf8 char */
	    nullfree(clean);
	    clean = NULL;
	    goto done;
	case 2: /* either non-ascii utf8 char or segment/creation mark */
	    if(issegmark(p)) {
		char info[16];
		size_t segindex = (size_t)segmarkindex(p);
		if(iscreateindex(segindex))
		    snprintf(info,sizeof(info),"^CR");
		else
		    snprintf(info,sizeof(info),"^%02x",(unsigned)segindex);
		memcpy(q,info,strlen(info));
		p += SEGMARKSIZE; q += strlen(info);
		break;
	    }
	    /* else non-segmark => fall thru */

	case 3: case 4: /* non-ascii utf8 char */
	    memcpycp((utf8*)q,(const utf8*)p); /* pass as is */
	    p += len; q += len;
	    break;
	case 1: /* ascii char */
	    char c = *p;
	    if(c < ' ' || c == '\177') { /* c is a control char */
		if(strchr(ctrls,c) == NULL) { /* de-controlize */
		    char escaped[3+1] = {'\0','\0','\0','\0'};
		    *q++ = '\\';
		    switch (c) {
		    case '\r': escaped[0] = 'r'; break;
		    case '\n': escaped[0] = 'n'; break;
		    case '\b': escaped[0] = 'b'; break;
		    case '\f': escaped[0] = 'f'; break;
		    case '\t': escaped[0] = 't'; break;
		    default: /* print as octal escape */
			snprintf(escaped,sizeof(escaped),"%03o",c);
		    }
		    len = strlen(escaped);
		    memcpy(q,escaped,len);
		    p++; q += len;
		} else
		    *q++ = *p++; /* pass control as is (ascii) */
	    } else { /* ordinary char */
		len = memcpycp((utf8*)q,(const utf8*)p);
		p += len; q += len;
	    }
	    break;
	default: abort(); /* bad len */
	}
    }
    *q = '\0';
    if(pfinallen) {*pfinallen = (size_t)(q - (char*)clean);}
done:
    return clean;
}

/* Convert '\\' escaped characters using standard C conventions.
The converted result is returned. Note that this is independent
of the TTM '@' escape mechanism.
@param src utf8 string
@param pfinallen store the final length of result
@return copy of s8 with escaped characters modified || NULL if non codepoint encountered
*/
static utf8*
deescape(const utf8* s8, size_t* pfinallen)
{
    utf8* deesc = NULL;
    char* p = NULL;
    char* q = NULL;
    size_t len = 0;

    if(s8 == NULL) goto done;
    len = strlen((const char*)s8);
    deesc = (utf8*)malloc(sizeof(utf8)*(len+1)); /* max possible */
    for(p=(char*)s8,q=(char*)deesc;*p;) {
	len = u8size((const utf8*)p);
	if(len == 0) goto fail; /* illegal utf8 char */
	if(len > 1) {
	    /* non-ascii utf8 char */
	    memcpycp((utf8*)q,(const utf8*)p); /* pass as is */
	    p += len; q += len;
	} else {/* len == 1 => ascii */
	    if(*p == '\\') { /* this is a C escape character */
		len = u8size((const utf8*)(++p)); /* escaped char might be any utf8 char */
		if(len == 1) {/* escaped ascii char */
		    int c = *p;
		    switch (c) {
		    case 'r': c = '\r'; p++; break;
		    case 'n': c = '\n'; p++; break;
		    case 'b': c = '\b'; p++; break;
		    case 'f': c = '\f'; p++; break;
		    case 't': c = '\t'; p++; break;
		    case '1': case '0': /* apparently octal */
			if(1!=scanf(p,"%03o",&c)) goto fail;
			if(c > '\177') goto fail;
			p += 3; /* skip the octal digits */
		    default: break;
		    }
		    *q++ = (char)c; /* pass escaped ascii char */
		} else {/* pass the utf8 char */
		    len = memcpycp((utf8*)q,(const utf8*)p);
		    p+=len; q += len;
		}
	    } else { /* ordinary char */
		len = memcpycp((utf8*)q,(const utf8*)p);
		p += len; q += len;
	    }
	}
    }
    *q = '\0';
    if(pfinallen) {*pfinallen = (size_t)(q - (char*)deesc);}
done:
    return deesc;
fail:
    nullfree(deesc);
    deesc = NULL;
    goto done;
}

/* Get a string from dictionary
@param ttm
@param frame
@param i
@return string or fail if not found or builtin.
*/
static Function*
getdictstr(TTM* ttm, const Frame* frame, size_t i)
{
    Function* str = NULL;

    if((str = dictionaryLookup(ttm,frame->argv[i]))==NULL) FAILNONAME(i);
    if(str->fcn.builtin) FAIL(ttm,TTM_ENOPRIM);
    return str;
}

/**************************************************/
/* Builtin functions */

/* Original TTM functions */

/* Dictionary Operations */
static void
ttm_ap(TTM* ttm, Frame* frame) /* Append to a string */
{
    utf8* body = NULL;
    utf8* newbody = NULL;
    utf8* apstring = NULL;
    Function* str = NULL;
    size_t aplen, bodylen, totallen;

    if((str = dictionaryLookup(ttm,frame->argv[1]))==NULL) {/* Define the string */
	ttm_ds(ttm,frame);
	return;
    }
    if(str->fcn.builtin) FAIL(ttm,TTM_ENOPRIM);
    apstring = frame->argv[2];
    aplen = strlen((const char*)apstring);
    body = str->fcn.body;
    bodylen = strlen((const char*)body);
    totallen = bodylen + aplen;
    /* Fake realloc because windows realloc is flawed */
    newbody = (utf8*)calloc(sizeof(utf8),(totallen+1));
    if(newbody == NULL) FAIL(ttm,TTM_EMEMORY);
    if(bodylen > 0) {
	memcpy((void*)newbody,(void*)body,bodylen);
	newbody[bodylen] = NUL8;
    }
    strncpy((char*)newbody,(const char*)apstring,aplen);
    str->fcn.residual = totallen;
    nullfree(str->fcn.body);
    str->fcn.body = newbody; newbody = NULL;
}

/**
The semantics of #<cf>
have been changed. See ttm.html
*/

static void
ttm_cf(TTM* ttm, Frame* frame) /* Copy a function */
{
    utf8* newname = frame->argv[1];
    utf8* oldname = frame->argv[2];
    Function* newfcn = dictionaryLookup(ttm,newname);
    Function* oldfcn = dictionaryLookup(ttm,oldname);
    struct HashEntry saveentry;

    if(oldfcn == NULL) FAILNONAMES(oldname);
    if(newfcn == NULL) {
	/* create a new string object */
	newfcn = newFunction(ttm);
	assert(newfcn->entry.name == NULL);
	newfcn->entry.name = (utf8*)strdup((const char*)newname);
	dictionaryInsert(ttm,newfcn);
    }
    saveentry = newfcn->entry;
    *newfcn = *oldfcn;
    /* Keep new hash entry */
    newfcn->entry = saveentry;
    /* Do pointer fixup */
    if(newfcn->fcn.body != NULL)
	newfcn->fcn.body = (utf8*)strdup((const char*)newfcn->fcn.body);
}

static void
ttm_cr(TTM* ttm, Frame* frame) /* Mark for creation */
{
    Function* str;
    utf8* crstring;
    size_t bodylen,remainder;

    if((str = getdictstr(ttm,frame,1))==NULL) FAILNONAME(1);
    bodylen = strlen((const char*)str->fcn.body);
    if(str->fcn.residual >= bodylen) return; /* no substitution possible */
    /* copy the remainder of str->fcn.body to ttm->vs.tmp for repeated scanning */
    remainder = bodylen - str->fcn.residual;
    vsclear(ttm->vs.tmp);
    vsappendn(ttm->vs.tmp,(const char*)(str->fcn.body+str->fcn.residual),remainder);

    crstring = frame->argv[2];
    (void)ttm_mark1(ttm,ttm->vs.tmp,crstring,CREATEINDEX);
    /* Replace the str body with contents of ttm->vs.tmp */
    nullfree(str->fcn.body);
    str->fcn.body = (utf8*)vsextract(ttm->vs.tmp);
}

static void
ttm_ds(TTM* ttm, Frame* frame)
{
    Function* str = NULL;
    str = dictionaryLookup(ttm,frame->argv[1]);
    if(str != NULL && str->fcn.locked) FAIL(ttm,TTM_ELOCKED);
    if(str != NULL) { /* clean for re-use */
	nullfree(str->fcn.body);
	memset(&str->fcn,0,sizeof(struct FcnData));
    } else {
	/* create a new string object */
	str = newFunction(ttm);
	assert(str->entry.name == NULL);
	str->entry.name = (utf8*)strdup((const char*)frame->argv[1]);
	dictionaryInsert(ttm,str);
    }
    str->fcn.trace = ttm->debug.trace;
    str->fcn.builtin = 0;
    str->fcn.maxargs = ARB;
    str->fcn.sv = SV_SV;
    str->fcn.novalue = 0;
    str->fcn.body = (utf8*)strdup((const char*)frame->argv[2]);
}

static void
ttm_es(TTM* ttm, Frame* frame) /* Erase string */
{
    size_t i;
    for(i=1;i<frame->argc;i++) {
	utf8* strname = frame->argv[i];
	Function* str = dictionaryLookup(ttm,strname);
	if(str != NULL) {
	    if(str->fcn.locked) FAIL(ttm,TTM_ELOCKED);
	    dictionaryRemove(ttm,strname);
	    freeFunction(ttm,str); /* reclaim the string */
	}
    }
}

/**
Helper function for #<sc> and #<ss> and #<cr>.
Copies from str->fcn.body to ttm->vs.result
while substituting segment or creation marks.
@param ttm
@param frame
@param iscr 1 => replacing creation marks
@param segcountp # of segment counters inserted.
@return void
*/
static void
ttm_ss0(TTM* ttm, Frame* frame, size_t* segcountp)
{
    Function* str;
    size_t segcount = 0;
    size_t i,segindex,bodylen;
    size_t remainder;

    if((str = getdictstr(ttm,frame,1))==NULL) FAILNONAME(1);
    bodylen = strlen((const char*)str->fcn.body);
    if(str->fcn.residual >= bodylen) return; /* no substitution possible */
    /* copy the remainder of str->fcn.body to ttm->vs.tmp for repeated scanning */
    remainder = bodylen - str->fcn.residual;
    vsclear(ttm->vs.tmp);
    vsappendn(ttm->vs.tmp,(const char*)(str->fcn.body+str->fcn.residual),remainder);

    segcount = 0;
    segindex = str->fcn.maxsegmark;

    for(i=2;i<frame->argc;i++) {
	size_t segcount1 = 0; /* no. of hits for this arg */
	utf8* arg = frame->argv[i];
	segcount1 = ttm_mark1(ttm,ttm->vs.tmp,arg,segindex);
	/* If we matched at least once, then bump the segindex for next match */
	if(segcount1 > 0) segindex++;
	segcount += segcount1;
    }
    str->fcn.maxsegmark = segindex; /* Remember the next segment mark to use */
    /* Replace the str body with contents of ttm->vs.tmp */
    nullfree(str->fcn.body);
    str->fcn.body = (utf8*)vsextract(ttm->vs.tmp);
    if(segcountp) *segcountp = segcount;
}

/**
Scan a string for a string and replace with a segment or creation mark.
@param ttm
@param vs holding the string being marked
@param pattern the string for which to search
@param mark the segment/create index for replacement
@reteurn count of the number of replacements
*/
static size_t
ttm_mark1(TTM* ttm, VString* target, utf8* pattern, size_t mark)
{
    size_t segcount = 0;
    utf8 seg[SEGMARKSIZE] = {SEGMARK0,segmarkindexbyte(SEGINDEX0)};
    size_t patternlen;
    utf8* p;
    char* q;
    size_t dstpos;

    patternlen = strlen((const char*)pattern);
    if(patternlen > 0) { /* search only if possible success */
	p = (utf8*)vscontents(target); /* Start search here */
	/* Search for occurrences of pattern */
	while(*p) {
	    q = strstr((const char*)p,(const char*)pattern);
	    if(q == NULL) break; /* Not found => done with this pattern => break */

	    dstpos = (size_t)(q - vscontents(target));
	    /* we have a match, replace match by the mark */
	    /* remove the matching string */
	    vsremoven(target,dstpos,patternlen);
	    /* insert seg mark */
	    seg[1] = segmarkindexbyte(mark);
	    vsinsertn(target,(size_t)dstpos,(const char*)seg,u8size(seg));
	    segcount++;
	    p = (utf8*)(q + patternlen);
	}
    }
    return segcount;
}

static void
ttm_sc(TTM* ttm, Frame* frame) /* Segment and count */
{
    char count[64];
    size_t nsegs;
    ttm_ss0(ttm,frame,&nsegs);
    snprintf(count,sizeof(count),"%zu",nsegs);
    /* Insert into ttm->vs.result */
    vsappendn(ttm->vs.result,(const char*)count,strlen(count));
}

static void
ttm_ss(TTM* ttm, Frame* frame) /* Segment and count */
{
    ttm_ss0(ttm,frame,NULL);
}

/* String Selection */

static void
ttm_cc(TTM* ttm, Frame* frame) /* Call one character */
{
    Function* fcn;

    if((fcn = getdictstr(ttm,frame,1))==NULL) FAILNONAME(1);
    /* Check for pointing at trailing NUL */
    if(fcn->fcn.residual < strlen((const char*)fcn->fcn.body)) {
	int ncp;
	utf8* p = fcn->fcn.body+fcn->fcn.residual;
	if((ncp = u8size(p))<0) FAIL(ttm,TTM_EUTF8);
	vsappendn(ttm->vs.result,(const char*)p,ncp);
	fcn->fcn.residual += ncp;
    }
}

static void
ttm_cn(TTM* ttm, Frame* frame) /* Call n characters (codepoints) */
{
    Function* fcn;
    long long ln;
    int n;
    int ncp, nbytes;
    const utf8* p = NULL;

    if((fcn = getdictstr(ttm,frame,2))==NULL) FAILNONAME(2);

    /* Get number of characters to extract */
    if(1 != sscanf((const char*)frame->argv[1],"%lld",&ln)) FAIL(ttm,TTM_EDECIMAL);
    if(ln < 0) FAIL(ttm,TTM_ENOTNEGATIVE);

    n = (int)ln;
    nbytes = strlen((const char*)p);
    if(n == 0 || nbytes == 0) goto nullreturn;

    /* copy up to n codepoints or EOS encountered */
    /* Fail if we encounter a bad codepoint */
    vsclear(ttm->vs.result);
    while(n-- > 0) {
	if(*p) break; /* EOS */
	if((ncp = u8size(p))<0) FAIL(ttm,TTM_EUTF8);
	vsappendn(ttm->vs.result,(const char*)p,ncp);
	fcn->fcn.residual += ncp;
    }
    return;
nullreturn:
    vsclear(ttm->vs.result);
    return;
}

static void
ttm_sn(TTM* ttm, Frame* frame) /* Skip n characters */
{
    long long num;
    Function* str;
    const utf8* p;

    if((str = getdictstr(ttm,frame,2))==NULL) FAILNONAME(2);
    if(1 != sscanf((const char*)frame->argv[1],"%lld",&num)) FAIL(ttm,TTM_EDECIMAL);
    if(num < 0) FAIL(ttm,TTM_ENOTNEGATIVE);

    for(p=str->fcn.body+str->fcn.residual;num-- > 0;) {
	int ncp = u8size(p);
	if(ncp < 0) FAIL(ttm,TTM_EUTF8);
	if(isnul(p)) break;
	p += ncp;
	str->fcn.residual+=ncp;
    }
}

static void
ttm_isc(TTM* ttm, Frame* frame) /* Initial character scan; moves residual pointer */
{
    Function* str;
    utf8* t;
    utf8* f;
    utf8* result;
    utf8* arg;
    size_t arglen;
    size_t slen;

    str = getdictstr(ttm,frame,2);

    arg = frame->argv[1];
    arglen = strlen((const char*)arg);
    t = frame->argv[3];
    f = frame->argv[4];

    /* check for initial string match */
    if(strncmp((const char*)str->fcn.body+str->fcn.residual,(const char*)arg,arglen)==0) {
	result = t;
	str->fcn.residual += arglen;
	slen = strlen((const char*)str->fcn.body);
	if(str->fcn.residual > slen) str->fcn.residual = slen;
    } else
	result = f;
    vsclear(ttm->vs.result);
    vsappendn(ttm->vs.result,(const char*)result,strlen((const char*)result));
}

static void
ttm_scn(TTM* ttm, Frame* frame) /* Character scan */
{
    Function* str;
    size_t arglen;
    utf8* f;
    utf8* arg;
    utf8* p;
    utf8* p0;

    str = getdictstr(ttm,frame,2);

    arg = frame->argv[1];
    arglen = strlen((const char*)arg);
    f = frame->argv[3];

    /* check for sub string match */
    p0 = str->fcn.body+str->fcn.residual;
    p = (utf8*)strstr((const char*)p0,(const char*)arg);
    vsclear(ttm->vs.result);
    if(p == NULL) {/* no match; return argv[3] */
	vsappendn(ttm->vs.result,(const char*)f,strlen((const char*)f));
    } else {/* return chars from residual ptr to location of string */
	ptrdiff_t len = (p - p0);
	vsappendn(ttm->vs.result,(const char*)p0,(size_t)len);
	/* move rp past matched string */
	str->fcn.residual += (len + arglen);
    }
}

static void
ttm_cp(TTM* ttm, Frame* frame) /* Call parameter */
{
    Function* fcn;
    size_t delta;
    utf8* rp;
    utf8* rp0;
    int depth;

    fcn = getdictstr(ttm,frame,1);

    rp0 = (fcn->fcn.body + fcn->fcn.residual);
    rp = rp0;
    depth = 0;
    for(;;) {
	int ncp = u8size(rp);
	if(u8equal(rp,ttm->meta.semic)) {
	    if(depth == 0) break; /* reached unnested semicolon*/
	} else if(u8equal(rp,ttm->meta.lbrc)) {
	    depth++;
	} else if(u8equal(rp,ttm->meta.rbrc)) {
	    depth--;
	}
	rp += ncp;
    }
    delta = (rp - rp0);
    vsclear(ttm->vs.result);
    vsappendn(ttm->vs.result,(const char*)rp0,delta);
    fcn->fcn.residual += delta;
    if(*rp != NUL8) fcn->fcn.residual++;
}

static void
ttm_cs(TTM* ttm, Frame* frame) /* Call segment */
{
    Function* fcn;
    utf8* p;
    utf8* p0;
    size_t delta;

    fcn = getdictstr(ttm,frame,1);

    /* Locate the next segment mark */
    /* Unclear if create marks also qualify; assume yes */
    p0 = fcn->fcn.body + fcn->fcn.residual;
    p = p0;
    for(;;) {
	int ncp;
	if((ncp = u8size(p))==0) FAIL(ttm,TTM_EUTF8);
	if(isnul(p) || issegmark(p) || iscreatemark(p))
	    break;
	p += ncp;
    }
    delta = (p - p0);
    if(delta > 0) {
	vsclear(ttm->vs.result);
	vsappendn(ttm->vs.result,(const char*)p0,delta);
    }
    /* set residual pointer correctly */
    fcn->fcn.residual += delta;
    if(*p != NUL8) fcn->fcn.residual++;
}

static void
ttm_rrp(TTM* ttm, Frame* frame) /* Reset residual pointer */
{
    Function* str = getdictstr(ttm,frame,1);
    str->fcn.residual = 0;
}

static void
ttm_eos(TTM* ttm, Frame* frame) /* Test for end of string */
{
    Function* str;
    utf8* t;
    utf8* f;
    utf8* result;
    size_t bodylen;

    str = getdictstr(ttm,frame,1);
    t = frame->argv[2];
    f = frame->argv[3];
    bodylen = strlen((const char*)str->fcn.body);
    result = (str->fcn.residual >= bodylen ? t : f);
    vsclear(ttm->vs.result);
    vsappendn(ttm->vs.result,(const char*)result,strlen((const char*)result));
}

/* String Scanning Operations */

static void
ttm_gn(TTM* ttm, Frame* frame) /* Give n characters from argument string*/
{
    utf8* snum = frame->argv[1];
    utf8* s = frame->argv[2];
    long long num;
    int nbytes = 0;

    if(1 != sscanf((const char*)snum,"%lld",&num)) FAIL(ttm,TTM_EDECIMAL);
    if(num > 0) {
	nbytes = u8ith(s,(int)num);
	if(nbytes < 0) FAIL(ttm,TTM_EUTF8);
	vsappendn(ttm->vs.result,(const char*)s,(size_t)nbytes);
    } else if(num < 0) {
	num = -num;
	nbytes = u8ith(s,(int)num);
	if(nbytes < 0) FAIL(ttm,TTM_EUTF8);
	s += nbytes;
	vsappendn(ttm->vs.result,(const char*)s,strlen((const char*)s));
    }
}

static void
ttm_zlc(TTM* ttm, Frame* frame) /* Zero-level commas */
{
    utf8* s;
    utf8* p;
    int depth;

    s = frame->argv[1];
    vsclear(ttm->vs.result);
    for(depth=0,p=s;*p;) {
	int count = u8size(p);
	if(isescape(p)) {
	    p += count;
	    count = u8size(p);
	    vsappendn(ttm->vs.result,(const char*)p,count); /* suppress escape and copy escaped char */
	} else if(*p == COMMA && depth == 0) {
	    p += count; /* skip comma */
	    vsappendn(ttm->vs.result,(const char*)ttm->meta.semic,u8size(ttm->meta.semic)); /*replace ',' */
	} else if(*p == LPAREN) {
	    depth++;
	    vsappendn(ttm->vs.result,(const char*)p,count);
	    p += count; /* pass LPAREN */
	} else if(*p == RPAREN) {
	    depth--;
	    vsappendn(ttm->vs.result,(const char*)p,count);
	    p += count; /* pass RPAREN */
	} else {
	    vsappendn(ttm->vs.result,(const char*)p,count);
	    p += count; /* pass char */
	}
    }
    *p = NUL8; /* make sure it is terminated */
}

static void
ttm_zlcp(TTM* ttm, Frame* frame) /* Zero-level commas and parentheses;
				    exact algorithm is unknown */
{
    /* A(B) and A,B will both give A;B and (A),(B),C will give A;B;C */
    utf8* s;
    utf8* p;
    int depth;

    s = frame->argv[1];

    vsclear(ttm->vs.result);
    for(depth=0,p=s;*p;) {
	int count = u8size(p);
	if(isescape(p)) {
	    p += count;
	    count = u8size(p);
	    vsappendn(ttm->vs.result,(const char*)p,count);
	} else if(depth == 0 && *p == COMMA) {
	    p += count; /* skip comma */
	    if(p[1] != LPAREN) {
		vsappendn(ttm->vs.result,(const char*)ttm->meta.semic,u8size(ttm->meta.semic));
	    }
	} else if(*p == LPAREN) {
	    if(depth == 0 && p > s) {
		p += count;
		vsappendn(ttm->vs.result,(const char*)ttm->meta.semic,u8size(ttm->meta.semic));
	    }
	    if(depth > 0) {
		vsappendn(ttm->vs.result,(const char*)p,count);
		p += count;
	    }
	    depth++;
	} else if(*p == RPAREN) {
	    depth--;
	    if(depth == 0 && p[1] == COMMA) {
		/* do nothing */
	    } else if(depth == 0 && p[1] == NUL8) {
		/* do nothing */
	    } else if(depth == 0) {
		vsappendn(ttm->vs.result,(const char*)ttm->meta.semic,u8size(ttm->meta.semic));
	    } else {/* depth > 0 */
		vsappendn(ttm->vs.result,(const char*)p,count);
		p += count;
	    }
	} else {
	    vsappendn(ttm->vs.result,(const char*)p,count);
	    p += count;
	}
    }
}

static void
ttm_flip(TTM* ttm, Frame* frame) /* Flip a string */
{
    utf8* s;
    utf8* p;
    int slen,i,ncp;

    s = frame->argv[1];
    slen = strlen((const char*)s);
    vsclear(ttm->vs.result);
    if((ncp = u8size(s))<0) FAIL(ttm,TTM_EUTF8);
    p = s + slen;
    for(i=0;i<slen;i++) {
	int count;
	p = (utf8*)u8backup(p); /* backup 1 codepoint */
	count = u8size(p);
	vsappendn(ttm->vs.result,(const char*)p,count);
    }
}

static void
ttm_trl(TTM* ttm, Frame* frame) /* translate to lower case */
{
    utf8* s;

    vsclear(ttm->vs.result);
    s = frame->argv[1];
    {
#ifdef MSWINDOWS
#else
	size_t i,swlen,s8len;
	wchar_t* sw = NULL;
	
	/* Determine the size needed for the wide character string */
	if((swlen = (mbstowcs(NULL, (char*)s, 0) + 1))==((size_t)-1)) FAIL(ttm,TTM_EUTF8);
	/* Allocate memory for the wide character string */
	if((sw = (wchar_t*)malloc(swlen * sizeof(wchar_t)))==NULL) FAIL(ttm,TTM_EMEMORY);
	/* convert utf8 string to wide char (utf16 or utf32) */
	(void)mbstowcs(sw,(char*)s, swlen);
	/* Convert to lower case */
	for(i=0;i<swlen;i++)
	    sw[i] = (wchar_t)towlower(sw[i]);
	/* Convert back to utf8 */	
	/* Get space required */
	if((s8len = (wcstombs(NULL, sw, 0) + 1))==((size_t)-1)) FAIL(ttm,TTM_EUTF8);

	/* Allocate memory for the utf8 character string */
	vssetalloc(ttm->vs.result,s8len);
	/* Insert into the result buffer */
	(void)wcstombs(vscontents(ttm->vs.result),sw,swlen);
	/* Make length consistent */
	vssetlength(ttm->vs.result,s8len-1);
#endif
    }
}

/* Shared helper for dcl and dncl */
static void
ttm_dcl0(TTM* ttm, Frame* frame, int negative)
{
    Charclass* cl = NULL;
    cl = charclassLookup(ttm,frame->argv[1]);
    if(cl == NULL) {
	/* create a new charclass object */
	cl = newCharclass(ttm);
	assert(cl->entry.name == NULL);
	cl->entry.name = (utf8*)strdup((const char*)frame->argv[1]);
	charclassInsert(ttm,cl);
    }
    nullfree(cl->characters);
    cl->characters = (utf8*)strdup((const char*)frame->argv[2]);
    cl->negative = negative;
}

static void
ttm_dcl(TTM* ttm, Frame* frame) /* Define a negative class */
{
    ttm_dcl0(ttm,frame,0);
}

static void
ttm_dncl(TTM* ttm, Frame* frame) /* Define a negative class */
{
    ttm_dcl0(ttm,frame,1);
}

static void
ttm_ecl(TTM* ttm, Frame* frame) /* Erase a class */
{
    size_t i;
    for(i=1;i<frame->argc;i++) {
	utf8* clname = frame->argv[i];
	Charclass* cl = charclassRemove(ttm,clname);
	if(cl != NULL) {
	    freeCharclass(ttm,cl); /* reclaim the character class */
	}
    }
}

static void
ttm_ccl(TTM* ttm, Frame* frame) /* call class */
{
    Charclass* cl = NULL;
    Function* str = NULL;
    utf8* p;
    size_t rr0;

    if((str = getdictstr(ttm,frame,2))==NULL) FAILNONAME(2);
    if((cl = charclassLookup(ttm,frame->argv[1]))==NULL) FAILNOCLASS(1);

    /* Starting at str->fcn.residual, locate first char not in class (or negative) */
    vsclear(ttm->vs.result);
    rr0 = str->fcn.residual;
    for(;;) {
	p = str->fcn.body + str->fcn.residual;
	if(cl->negative && charclassMatch(p,cl->characters)) break;
	if(!cl->negative && !charclassMatch(p,cl->characters)) break;
	str->fcn.residual += u8size(p);
    }
    vsappendn(ttm->vs.result,(const char*)str->fcn.body + rr0,(str->fcn.residual - rr0));
    str->fcn.residual = rr0;
}

static void
ttm_scl(TTM* ttm, Frame* frame) /* skip class */
{
    Charclass* cl = NULL;
    Function* str = NULL;
    utf8* p;

    if((cl = charclassLookup(ttm,frame->argv[1]))==NULL) FAILNOCLASS(1);
    if((str = getdictstr(ttm,frame,2))==NULL) FAILNONAME(2);

    /* Starting at str->fcn.residual, locate first char not in class */
    for(;;) {
	int count;
	p = str->fcn.body + str->fcn.residual;
	if(cl->negative && charclassMatch(p,cl->characters)) break;
	if(!cl->negative && !charclassMatch(p,cl->characters)) break;
	count = u8size(p);
	str->fcn.residual += count;
    }
}

static void
ttm_tcl(TTM* ttm, Frame* frame) /* Test class */
{
    Charclass* cl = NULL;
    Function* str = NULL;
    utf8* retval;
    utf8* t;
    utf8* f;

    if((cl = charclassLookup(ttm,frame->argv[1]))==NULL) FAILNOCLASS(1);
    if((str = getdictstr(ttm,frame,2))==NULL) FAILNONAME(2);

    t = frame->argv[3];
    f = frame->argv[4];
    if(str == NULL)
	retval = f;
    else {
	utf8* p = str->fcn.body + str->fcn.residual;
	/* see if char at str->fcn.residual is in class */
	if(cl->negative && !charclassMatch(p,cl->characters))
	    retval = t;
	else if(!cl->negative && charclassMatch(p,cl->characters))
	    retval = t;
	else
	    retval = f;
    }
    vsappendn(ttm->vs.result,(const char*)retval,strlen((const char*)retval));
}

/* Arithmetic Operators */

static void
ttm_abs(TTM* ttm, Frame* frame) /* Obtain absolute value */
{
    utf8* slhs;
    long long lhs;
    char result[128];
    slhs = frame->argv[1];
    if(1 != sscanf((const char*)slhs,"%lld",&lhs)) FAIL(ttm,TTM_EDECIMAL);
    if(lhs < 0) lhs = -lhs;
    snprintf(result,sizeof(result),"%lld",lhs);
    vsclear(ttm->vs.result);
    vsappendn(ttm->vs.result,result,strlen(result));
}

static void
ttm_ad(TTM* ttm, Frame* frame) /* Add */
{
    utf8* slhs;
    long long lhs;
    utf8* srhs;
    long long rhs;
    char result[128];

    slhs = frame->argv[1];
    srhs = frame->argv[2];
    if(1 != sscanf((const char*)slhs,"%lld",&lhs)) FAIL(ttm,TTM_EDECIMAL);
    if(1 != sscanf((const char*)srhs,"%lld",&rhs)) FAIL(ttm,TTM_EDECIMAL);
    snprintf(result,sizeof(result),"%lld",lhs+rhs);
    vsclear(ttm->vs.result);
    vsappendn(ttm->vs.result,result,strlen(result));
}

static void
ttm_dv(TTM* ttm, Frame* frame) /* Divide and give quotient */
{
    utf8* slhs;
    long long lhs;
    utf8* srhs;
    long long rhs;
    char result[128];

    slhs = frame->argv[1];
    srhs = frame->argv[2];
    if(1 != sscanf((const char*)slhs,"%lld",&lhs)) FAIL(ttm,TTM_EDECIMAL);
    if(1 != sscanf((const char*)srhs,"%lld",&rhs)) FAIL(ttm,TTM_EDECIMAL);
    snprintf(result,sizeof(result),"%lld",lhs / rhs);
    vsclear(ttm->vs.result);
    vsappendn(ttm->vs.result,result,strlen(result));
}

static void
ttm_dvr(TTM* ttm, Frame* frame) /* Divide and give remainder */
{
    utf8* slhs;
    long long lhs;
    utf8* srhs;
    long long rhs;
    char result[128];

    slhs = frame->argv[1];
    srhs = frame->argv[2];
    if(1 != sscanf((const char*)slhs,"%lld",&lhs)) FAIL(ttm,TTM_EDECIMAL);
    if(1 != sscanf((const char*)srhs,"%lld",&rhs)) FAIL(ttm,TTM_EDECIMAL);
    snprintf(result,sizeof(result),"%lld",lhs / rhs);
    vsclear(ttm->vs.result);
    vsappendn(ttm->vs.result,result,strlen(result));
}

static void
ttm_mu(TTM* ttm, Frame* frame) /* Multiply */
{
    utf8* slhs;
    long long lhs;
    utf8* srhs;
    long long rhs;
    char result[128];

    slhs = frame->argv[1];
    srhs = frame->argv[2];
    if(1 != sscanf((const char*)slhs,"%lld",&lhs)) FAIL(ttm,TTM_EDECIMAL);
    if(1 != sscanf((const char*)srhs,"%lld",&rhs)) FAIL(ttm,TTM_EDECIMAL);
    snprintf(result,sizeof(result),"%lld",lhs * rhs);
    vsclear(ttm->vs.result);
    vsappendn(ttm->vs.result,result,strlen(result));
}

static void
ttm_su(TTM* ttm, Frame* frame) /* Substract */
{
    utf8* slhs;
    long long lhs;
    utf8* srhs;
    long long rhs;
    char result[128];

    slhs = frame->argv[1];
    srhs = frame->argv[2];
    if(1 != sscanf((const char*)slhs,"%lld",&lhs)) FAIL(ttm,TTM_EDECIMAL);
    if(1 != sscanf((const char*)srhs,"%lld",&rhs)) FAIL(ttm,TTM_EDECIMAL);
    snprintf(result,sizeof(result),"%lld",lhs - rhs);
    vsclear(ttm->vs.result);
    vsappendn(ttm->vs.result,result,strlen(result));
}

static void
ttm_eq(TTM* ttm, Frame* frame) /* Compare numeric equal */
{
    utf8* slhs;
    utf8* srhs;
    long long lhs,rhs;
    utf8* t;
    utf8* f;
    utf8* result;

    slhs = frame->argv[1];
    srhs = frame->argv[2];
    t = frame->argv[3];
    f = frame->argv[4];

    if(1 != sscanf((const char*)slhs,"%lld",&lhs)) FAIL(ttm,TTM_EDECIMAL);
    if(1 != sscanf((const char*)srhs,"%lld",&rhs)) FAIL(ttm,TTM_EDECIMAL);
    result = (lhs == rhs ? t : f);
    vsclear(ttm->vs.result);
    vsappendn(ttm->vs.result,(const char*)result,strlen((const char*)result));
}

static void
ttm_gt(TTM* ttm, Frame* frame) /* Compare numeric greater-than */
{
    utf8* slhs;
    utf8* srhs;
    long long lhs,rhs;
    utf8* t;
    utf8* f;
    utf8* result;

    slhs = frame->argv[1];
    srhs = frame->argv[2];
    t = frame->argv[3];
    f = frame->argv[4];

    if(1 != sscanf((const char*)slhs,"%lld",&lhs)) FAIL(ttm,TTM_EDECIMAL);
    if(1 != sscanf((const char*)srhs,"%lld",&rhs)) FAIL(ttm,TTM_EDECIMAL);
    result = (lhs > rhs ? t : f);
    vsclear(ttm->vs.result);
    vsappendn(ttm->vs.result,(const char*)result,strlen((const char*)result));
}

static void
ttm_lt(TTM* ttm, Frame* frame) /* Compare numeric less-than */
{
    utf8* slhs;
    utf8* srhs;
    long long lhs,rhs;
    utf8* t;
    utf8* f;
    utf8* result;

    slhs = frame->argv[1];
    srhs = frame->argv[2];
    t = frame->argv[3];
    f = frame->argv[4];

    if(1 != sscanf((const char*)slhs,"%lld",&lhs)) FAIL(ttm,TTM_EDECIMAL);
    if(1 != sscanf((const char*)srhs,"%lld",&rhs)) FAIL(ttm,TTM_EDECIMAL);
    result = (lhs < rhs ? t : f);
    vsclear(ttm->vs.result);
    vsappendn(ttm->vs.result,(const char*)result,strlen((const char*)result));
}

static void
ttm_ge(TTM* ttm, Frame* frame) /* Compare numeric greater-than or equal */
{
    utf8* slhs;
    utf8* srhs;
    long long lhs,rhs;
    utf8* t;
    utf8* f;
    utf8* result;

    slhs = frame->argv[1];
    srhs = frame->argv[2];
    t = frame->argv[3];
    f = frame->argv[4];

    if(1 != sscanf((const char*)slhs,"%lld",&lhs)) FAIL(ttm,TTM_EDECIMAL);
    if(1 != sscanf((const char*)srhs,"%lld",&rhs)) FAIL(ttm,TTM_EDECIMAL);
    result = (lhs >= rhs ? t : f);
    vsclear(ttm->vs.result);
    vsappendn(ttm->vs.result,(const char*)result,strlen((const char*)result));
}

static void
ttm_le(TTM* ttm, Frame* frame) /* Compare numeric less-than or equal */
{
    utf8* slhs;
    utf8* srhs;
    long long lhs,rhs;
    utf8* t;
    utf8* f;
    utf8* result;

    slhs = frame->argv[1];
    srhs = frame->argv[2];
    t = frame->argv[3];
    f = frame->argv[4];

    if(1 != sscanf((const char*)slhs,"%lld",&lhs)) FAIL(ttm,TTM_EDECIMAL);
    if(1 != sscanf((const char*)srhs,"%lld",&rhs)) FAIL(ttm,TTM_EDECIMAL);
    result = (lhs <= rhs ? t : f);
    vsclear(ttm->vs.result);
    vsappendn(ttm->vs.result,(const char*)result,strlen((const char*)result));
}

static void
ttm_eql(TTM* ttm, Frame* frame) /* ? Compare logical equal */
{
    utf8* slhs;
    utf8* srhs;
    utf8* t;
    utf8* f;
    utf8* result;

    slhs = frame->argv[1];
    srhs = frame->argv[2];
    t = frame->argv[3];
    f = frame->argv[4];

    result = (strcmp((const char*)slhs,(const char*)srhs) == 0 ? t : f);
    vsclear(ttm->vs.result);
    vsappendn(ttm->vs.result,(const char*)result,strlen((const char*)result));
}

static void
ttm_gtl(TTM* ttm, Frame* frame) /* ? Compare logical greater-than */
{
    utf8* slhs;
    utf8* srhs;
    utf8* t;
    utf8* f;
    utf8* result;

    slhs = frame->argv[1];
    srhs = frame->argv[2];
    t = frame->argv[3];
    f = frame->argv[4];

    result = (strcmp((const char*)slhs,(const char*)srhs) > 0 ? t : f);
    vsclear(ttm->vs.result);
    vsappendn(ttm->vs.result,(const char*)result,strlen((const char*)result));
}

static void
ttm_ltl(TTM* ttm, Frame* frame) /* ? Compare logical less-than */
{
    utf8* slhs;
    utf8* srhs;
    utf8* t;
    utf8* f;
    utf8* result;

    slhs = frame->argv[1];
    srhs = frame->argv[2];
    t = frame->argv[3];
    f = frame->argv[4];

    result = (strcmp((const char*)slhs,(const char*)srhs) < 0 ? t : f);
    vsclear(ttm->vs.result);
    vsappendn(ttm->vs.result,(const char*)result,strlen((const char*)result));
}

/* Peripheral Input/Output Operations */

/**
In order to a void spoofing, the string to be output is modified
to convert all control characters except '\n' and '\r' ('\t').
Also, segment and create marks, are converted to a printable form.
*/
static void
ttm_ps(TTM* ttm, Frame* frame) /* Print a Function/String */
{
    utf8* stdxx = (frame->argc == 2 ? NULL : frame->argv[2]);
    TTMFILE* target;
    size_t i;

    if(stdxx != NULL && strcmp((const char*)stdxx,"stderr")==0)
	target=ttm->io.stderr;
    else
	target = ttm->io.stdout;
    for(i=1;i<frame->argc;i++) {
	utf8* cleaned = printclean(frame->argv[i],"\n\r\t",NULL);
	printstring(ttm,cleaned,target);
	nullfree(cleaned);
    }
}

static void
ttm_rs(TTM* ttm, Frame* frame) /* Read a Function/String */
{
    int len;
    for(len=0;;len++) {
	int ncp;
	utf8cpa cp8;
	ncp = ttmnonl(ttm,ttm->io.stdin,cp8);
	if(isnul(cp8)) break;
	if(u8equal(cp8,ttm->meta.eofc)) break;
	vsappendn(ttm->vs.result,(const char*)cp8,ncp);
    }
}

static void
ttm_psr(TTM* ttm, Frame* frame) /* Print a string and then read from input */
{
    int argc = frame->argc;
    ttm_ps(ttm,frame);
    ttm_rs(ttm,frame);
    frame->argc = argc;
}

static void
ttm_cm(TTM* ttm, Frame* frame) /* Change meta character */
{
    utf8* smeta = frame->argv[1];
    if(strlen((const char*)smeta) > 0) {
	if(u8size(smeta) > 1) FAIL(ttm,TTM_EASCII);
	memcpycp(ttm->meta.eofc,smeta);
    }
}

static void
ttm_pf(TTM* ttm, Frame* frame) /* Flush stdout and/or stderr */
{
    utf8* stdxx = (frame->argc == 1 ? NULL : frame->argv[1]);
    if(stdxx == NULL || strcmp((const char*)stdxx,"stdout")==0)
	fflush(stderr);
    if(stdxx == NULL || strcmp((const char*)stdxx,"stderr")==0)
	fflush(stderr);
}

/* Library Operations */

static int
stringveccmp(const void* a, const void* b)
{
    const utf8** sa = (const utf8**)a;
    const utf8** sb = (const utf8**)b;
    return strcmp((const char*)(*sa),(const char*)(*sb));
}

static void
ttm_names(TTM* ttm, Frame* frame) /* Obtain all dictionary instance names in sorted order */
{
    int i,nnames,index;
    utf8 const** names;
    /* Note, we ignore any "programnames" argument */
#if 0
    int allnames = (frame->argc > 1 ? 1 : 0);
#endif

    /* First, figure out the number of names for builtin */
    for(nnames=0,i=0;i<HASHSIZE;i++) {
	struct HashEntry* entry = ttm->tables.dictionary.table[i].next;
	while(entry != NULL) {
	    Function* str = (Function*)entry;
	    if(!str->fcn.builtin) { /* I read the documentation to ignore builtin functions */
		nnames++;
	    }
	    entry = entry->next;
	}
    }
    if(nnames == 0) return;

    /* Now collect all the names */
    /* Note the reason we collect the names is because we need to sort them */
    names = (utf8 const**)calloc(sizeof(utf8*),nnames);
    if(names == NULL) FAIL(ttm,TTM_EMEMORY);
    index = 0;
    for(i=0;i<HASHSIZE;i++) {
	struct HashEntry* entry = ttm->tables.dictionary.table[i].next;
	while(entry != NULL) {
	    Function* str = (Function*)entry;
	    if(!str->fcn.builtin) {
		names[index++] = (const utf8*)str->entry.name;
	    }
	    entry = entry->next;
	}
    }
    if(index != nnames) FAIL(ttm,TTM_ETTM); /* Something bad happened */
    /* Quick sort */
    qsort((void*)names, nnames, sizeof(utf8*),stringveccmp);

    /* print names comma separated */
    for(i=0;i<nnames;i++) {
	if(i > 0) vsappend(ttm->vs.result,',');
	vsappendn(ttm->vs.result,(const char*)names[i],strlen((const char*)names[i]));
    }
    if(nnames > 0) free(names); /* but do not free the string contents */
}

static void
ttm_exit(TTM* ttm, Frame* frame) /* Return from TTM */
{
    long long exitcode = 0;
    ttm->flags.exit = 1;
    if(frame->argc > 1) {
	if(1!=sscanf((const char*)frame->argv[1],"%lld",&exitcode)) FAIL(ttm,TTM_EDECIMAL);
	if(exitcode < 0) exitcode = - exitcode;
    }
    ttm->flags.exitcode = (int)exitcode;
    /* Clear any ttm->vs.active contents */
    vsclear(ttm->vs.active);
}

/* Utility Operations */

static void
ttm_ndf(TTM* ttm, Frame* frame) /* Determine if a name is defined */
{
    Function* str;
    utf8* t;
    utf8* f;
    utf8* result;

    str = dictionaryLookup(ttm,frame->argv[1]);
    t = frame->argv[2];
    f = frame->argv[3];
    result = (str == NULL ? f : t);
    vsappendn(ttm->vs.result,(const char*)result,strlen((const char*)result));
}

static void
ttm_norm(TTM* ttm, Frame* frame) /* Obtain the Norm of a string in units of codepoints */
{
    utf8* s;
    char result[32];
    int count;

    s = frame->argv[1];
    if((count = u8cpcount(s,strlen((const char*)s)))<0) FAIL(ttm,TTM_EUTF8);
    snprintf(result,sizeof(result),"%d",count);
    vsappendn(ttm->vs.result,result,strlen(result));
}

static void
ttm_time(TTM* ttm, Frame* frame) /* Obtain time of day */
{
    char result[MAXINTCHARS+1];
    struct timeval tv;
    long long time;

    if(timeofday(&tv) < 0)
	FAIL(ttm,TTM_ETIME);
    time = (long long)tv.tv_sec;
    time *= 1000000; /* convert to microseconds */
    time += tv.tv_usec;
    time = time / 10000; /* Need time in 100th second */
    snprintf(result,sizeof(result),"%lld",time);
    vsappendn(ttm->vs.result,result,strlen(result));
}

static void
ttm_xtime(TTM* ttm, Frame* frame) /* Obtain Execution Time */
{
    long long time;
    char result[MAXINTCHARS+1];

    time = getRunTime();
    snprintf(result,sizeof(result),"%lld",time);
    vsappendn(ttm->vs.result,result,strlen(result));
}

static void
ttm_ctime(TTM* ttm, Frame* frame) /* Convert ##<time> to printable string */
{
    utf8* stod;
    long long tod;
    char result[1024];
    time_t ttod;
    int i;

    stod = frame->argv[1];
    if(1 != sscanf((const char*)stod,"%lld",&tod)) FAIL(ttm,TTM_EDECIMAL);
    tod = tod/100; /* need seconds */
    ttod = (time_t)tod;
    snprintf(result,sizeof(result),"%s",ctime(&ttod));
    /* ctime adds a trailing new line; remove it */
    i = strlen(result);
    for(i--;i >= 0;i--) {
	if(result[i] != '\n' && result[i] != '\r') break;
    }
    result[i+1] = NUL8;
    vsappendn(ttm->vs.result,result,strlen(result));
}

static void
ttm_tf(TTM* ttm, Frame* frame) /* Turn Trace Off */
{
    size_t i;
    if(frame->argc > 1) {/* trace off specific names; a single null name => trace off all functions */
	if(frame->argc == 2 && strlen((const char*)frame->argv[1])==0) {
	    for(i=0;i<HASHSIZE;i++) {
		struct HashEntry* entry = ttm->tables.dictionary.table[i].next;
		while(entry != NULL) {
		    Function* str = (Function*)entry;
		    str->fcn.trace = 0;
		    entry = entry->next;
		}
	    }
	} else for(i=1;i<frame->argc;i++) {
	    Function* fcn = dictionaryLookup(ttm,frame->argv[i]);
	    if(fcn != NULL) fcn->fcn.trace = 0;
	}
    } else { /* turn off global tracing */
	ttm->debug.trace = 0;
    }
}

static void
ttm_tn(TTM* ttm, Frame* frame) /* Turn Trace On */
{
    size_t i;
    if(frame->argc > 1) {/* trace on specific names; a single null name => trace on all functions */
	if(frame->argc == 2 && strlen((const char*)frame->argv[1])==0) {
	    for(i=0;i<HASHSIZE;i++) {
		struct HashEntry* entry = ttm->tables.dictionary.table[i].next;
		while(entry != NULL) {
		    Function* str = (Function*)entry;
		    str->fcn.trace = 1;
		    entry = entry->next;
		}
	    }
	} else for(i=1;i<frame->argc;i++) {
	    Function* fcn = dictionaryLookup(ttm,frame->argv[i]);
	    if(fcn != NULL) fcn->fcn.trace = 1;
	}
    } else { /* turn off global tracing */
	ttm->debug.trace = 1;
    }
}

/* Functions new to this implementation */

/* Get ith command line argument; zero is command */
static void
ttm_argv(TTM* ttm, Frame* frame)
{
    long long index = 0;
    int arglen;
    char* arg;

    if((1 != sscanf((const char*)frame->argv[1],"%lld",&index))) FAIL(ttm,TTM_EDECIMAL);
    if(index < 0 || ((size_t)index) >= vllength(argoptions))FAIL(ttm,TTM_ERANGE);
    arg = vlget(argoptions,(size_t)index);
    arglen = strlen(arg);
    vsappendn(ttm->vs.result,arg,arglen);
}

/* Get the length of argoptions */
static void
ttm_argc(TTM* ttm, Frame* frame)
{
    char result[MAXINTCHARS+1];
    int argc;

    argc = vllength(argoptions);
    snprintf(result,sizeof(result),"%d",argc);
    vsappendn(ttm->vs.result,result,strlen(result));
}

static void
ttm_classes(TTM* ttm, Frame* frame) /* Obtain all character class names */
{
    int i,nclasses,index;
    utf8 const** classes;
    /* Note, we ignore any "programclasses" argument */
#if 0
    int allclasses = (frame->argc > 1 ? 1 : 0);
#endif

    /* First, figure out the number of classes and the total size */
    for(nclasses=0,i=0;i<HASHSIZE;i++) {
	struct HashEntry* entry = ttm->tables.charclasses.table[i].next;
	while(entry != NULL) {
	    nclasses++;
	    entry = entry->next;
	}
    }
    if(nclasses == 0) return;

    /* Now collect all the classes */
    /* Note the reason we collect the classes is because we need to sort them */
    classes = (utf8 const**)calloc(sizeof(utf8*),nclasses);
    if(classes == NULL) FAIL(ttm,TTM_EMEMORY);
    index = 0;
    for(i=0;i<HASHSIZE;i++) {
	struct HashEntry* entry = ttm->tables.charclasses.table[i].next;
	while(entry != NULL) {
	    Charclass* class = (Charclass*)entry;
	    classes[index++] = (const utf8*)class->entry.name;
	    entry = entry->next;
	}
    }
    if(index != nclasses) FAIL(ttm,TTM_ETTM); /* Something bad happened */
    /* Quick sort */
    qsort((void*)classes, nclasses, sizeof(utf8*),stringveccmp);

    /* print classes comma separated */
    for(i=0;i<nclasses;i++) {
	if(i > 0) vsappend(ttm->vs.result,',');
	vsappendn(ttm->vs.result,(const char*)classes[i],strlen((const char*)classes[i]));
    }
    if(nclasses > 0) free(classes); /* but do not free the string contents */
}

static void
ttm_lf(TTM* ttm, Frame* frame) /* Lock a list of function from being deleted; null =>lock all */
{
    size_t i;
    Function* fcn;
    if(frame->argc > 1) {/*lock specific names; a single null name => lock all functions */
	if(frame->argc == 2 && strlen((const char*)frame->argv[1])==0) {
	    for(i=0;i<HASHSIZE;i++) {
		struct HashEntry* entry = ttm->tables.dictionary.table[i].next;
		while(entry != NULL) {
		    fcn = (Function*)entry;
		    if(!fcn->fcn.builtin) fcn->fcn.locked = 1;
		    entry = entry->next;
		}
	    }
	} else for(i=1;i<frame->argc;i++) {
	    fcn = dictionaryLookup(ttm,frame->argv[i]);
	    if(fcn != NULL && !fcn->fcn.builtin) fcn->fcn.trace = 1;
	}
    }
}

static void
ttm_uf(TTM* ttm, Frame* frame) /* Un-Lock a function from being deleted */
{
    size_t i;
    Function* fcn;
    if(frame->argc > 1) {/*lock specific names; a single null name => lock all functions */
	if(frame->argc == 2 && strlen((const char*)frame->argv[1])==0) {
	    for(i=0;i<HASHSIZE;i++) {
		struct HashEntry* entry = ttm->tables.dictionary.table[i].next;
		while(entry != NULL) {
		    fcn = (Function*)entry;
		    if(!fcn->fcn.builtin) fcn->fcn.locked = 0;
		    entry = entry->next;
		}
	    }
	} else for(i=1;i<frame->argc;i++) {
	    fcn = dictionaryLookup(ttm,frame->argv[i]);
	    if(fcn != NULL && !fcn->fcn.builtin) fcn->fcn.trace = 0;
	}
    }
}

static void
ttm_include(TTM* ttm, Frame* frame)  /* Include text of a file */
{
    const char* path;

    path = (const char*)frame->argv[1];
    if(path == NULL || strlen(path) == 0) FAIL(ttm,TTM_EINCLUDE);
    readfile(ttm,path,ttm->vs.tmp);
    /* Append ttm->vs.passive or insert into ttm->vs.active at index */
    if(frame->active) {
	/* WARNING: invalidates any pointers into active */
	vsindexinsertn(ttm->vs.active,vscontents(ttm->vs.tmp),vslength(ttm->vs.tmp));
    } else {
	vsappendn(ttm->vs.passive,vscontents(ttm->vs.tmp),vslength(ttm->vs.tmp));
    }
    vsclear(ttm->vs.tmp);
}

static void
ttm_void(TTM* ttm, Frame* frame) /* Throw away all arguments */
{
    return;
}

/** Properties functions */
static void
ttm_setprop(TTM* ttm, Frame* frame) /* Set specified property */
{
    utf8* key = NULL;
    utf8* value = NULL;
    if(frame->argc < 3) FAIL(ttm,TTM_EFEWPARMS);
    key = frame->argv[1];
    value = frame->argv[2];
    setproperty((const char*)key,(const char*)value,&ttm->properties);
}

static void
ttm_resetprop(TTM* ttm, Frame* frame) /* Set specified property back to default */
{
    utf8* key = NULL;
    if(frame->argc < 2) FAIL(ttm,TTM_EFEWPARMS);
    key = frame->argv[1];
    resetproperty(&ttm->properties,(const char*)key,&dfalt_properties);
}

/**
Helper functions for all the ttm commands
and subcommands
*/

/**
#<ttm;meta;which;char>
where which is one of: "sharp","open","close","semi","escape","meta",
and char is the replacement character for that TTM-significant value.
Note that "meta" is for completeness since the cm command does
the same thing.
*/
static void
ttm_ttm_meta(TTM* ttm, Frame* frame)
{
    utf8* which = NULL;
    utf8* replacement = NULL;
    if(frame->argc < 3) FAIL(ttm,TTM_EFEWPARMS);
    which = frame->argv[1];
    replacement = frame->argv[2];

    if(u8size(replacement) > 1) FAIL(ttm,TTM_EASCII);
    switch (metaenumdetect(which)) {
    case ME_SHARP: memcpycp(ttm->meta.sharpc,replacement); break;
    case ME_SEMI: memcpycp(ttm->meta.semic,replacement); break;
    case ME_ESCAPE: memcpycp(ttm->meta.escapec,replacement); break;
    case ME_META: memcpycp(ttm->meta.eofc,replacement); break;
    case ME_OPEN: memcpycp(ttm->meta.openc,replacement); break;
    case ME_CLOSE: memcpycp(ttm->meta.closec,replacement); break;
    case ME_LBR: memcpycp(ttm->meta.lbrc,replacement); break;
    case ME_RBR: memcpycp(ttm->meta.rbrc,replacement); break;
    default: FAIL(NULL,TTM_ETTM);
    }
}

/**
#<ttm;info;name,{name}>
Return info about name from the dictionary.
*/
static void
ttm_ttm_info_name(TTM* ttm, Frame* frame)
{
    Function* str;
    char info[8192];
    utf8* arg = NULL;
    size_t arglen = 0;

    if(frame->argc < 4) FAIL(ttm,TTM_EFEWPARMS);
    arg = frame->argv[3];
    arglen = strlen((const char*)arg);
    str = dictionaryLookup(ttm,arg);
    if(str == NULL) { /* not defined*/
	vsappendn(ttm->vs.result,(const char*)arg,arglen);
	vsappendn(ttm->vs.result,"|,-,-,-|",0);
	goto done;
    }
    /* assert(str != NULL) */
    vsappendn(ttm->vs.result,(const char*)str->entry.name,strlen((const char*)str->entry.name));
    if(str->fcn.builtin) {
	char digits[64];
	snprintf(digits,sizeof(digits),"%zu",str->fcn.maxargs);
	snprintf(info,sizeof(info),",%zu,%s,%s",
		 str->fcn.minargs,(str->fcn.maxargs == ARB?"*":digits),sv(str));
	vsappendn(ttm->vs.result,info,strlen(info));
    } else {/*!str->fcn.builtin*/
	snprintf(info,sizeof(info),",0,%zu,V",str->fcn.maxsegmark);
	vsappendn(ttm->vs.result,info,strlen(info));
    }
    if(!str->fcn.builtin && ttm->debug.verbose) {
	utf8* cleaned = NULL;
	size_t ncleaned = 0;
	snprintf(info,sizeof(info)," residual=%zu body=|",str->fcn.residual);
	vsappendn(ttm->vs.result,info,strlen(info));
	cleaned = printclean(str->fcn.body,NULL,&ncleaned);
	vsappendn(ttm->vs.result,(const char*)cleaned,ncleaned);
    }
done:
#if DEBUG > 0
xprintf(ttm,"info.name: |%s|\n",vscontents(ttm->vs.result));
#endif
    return;
}

/**
#<ttm;info;class;{classname}...>
*/
static void
ttm_ttm_info_class(TTM* ttm, Frame* frame) /* Misc. combined actions */
{
    Charclass* cl;
    utf8* p;
    size_t i,len;

    for(i=3;i<frame->argc;i++) {
	cl = charclassLookup(ttm,frame->argv[i]);
	if(cl == NULL) FAILNOCLASS(i);
	len = strlen((const char*)cl->entry.name);
	vsappendn(ttm->vs.result,(const char*)cl->entry.name,len);
	vsappend(ttm->vs.result,' ');
	vsappend(ttm->vs.result,LBRACKET);
	if(cl->negative) vsappend(ttm->vs.result,'^');
	p=cl->characters;
	for(;;) {
	    int ncp = u8size(p);
	    if(ncp == 1 && (*p == LBRACKET || *p == RBRACKET))
		vsappend(ttm->vs.result,'\\');
	    p += ncp;
	}
	vsappend(ttm->vs.result,'\n');
    }
#if DEBUG > 0
    xprintf(ttm,"info.class: |%s|\n",vscontents(ttm->vs.result));
#endif
}

/**
#<ttm;info;string;{name}>
if name is builtin then use value of "" else return the string
value (including any seg/create marks). The result is
bracketed (<...>).
*/
static void
ttm_ttm_info_string(TTM* ttm, Frame* frame) /* Misc. combined actions */
{
    Function* str;
    utf8* arg = NULL;

    if(frame->argc < 4) FAIL(ttm,TTM_EFEWPARMS);
    arg = frame->argv[3];
    str = dictionaryLookup(ttm,arg);
    /* Surround the body with <...> */
    vsappendn(ttm->vs.result,(const char*)ttm->meta.lbrc,u8size(ttm->meta.lbrc));
    if(str != NULL ||! str->fcn.builtin) {
	vsappendn(ttm->vs.result,(const char*)str->fcn.body,strlen((const char*)str->fcn.body));
    } else { /* Use "" */
	/* append nothing */
    }
    vsappendn(ttm->vs.result,(const char*)ttm->meta.rbrc,u8size(ttm->meta.rbrc));
}

/**
#<ttm;list;{case}>
#<ttm;list;class;{clname}>
Where case is one of:
"all" => list all names
"builtin" => list only builtin names
"string" => list only user-defined names
Each result is ';' separated and surrounded by <...>
Names are sorted.
*/

static int stringcmp(const void* a, const void* b); /*forward*/

static void
ttm_ttm_list(TTM* ttm, Frame* frame) /* Misc. combined actions */
{
    size_t i;
    int first;
    VList* vl = vlnew();
    void* walker = NULL;
    struct HashEntry* entry = NULL;
    enum TTMEnum tte;
    struct HashTable* table = NULL;
    
    if(frame->argc < 3) FAIL(ttm,TTM_EFEWPARMS);

    /* Figure out what we are collecting */
    switch (tte = ttmenumdetect(frame->argv[2])) {
    case TE_CLASS: table = &ttm->tables.charclasses; break;
    case TE_ALL: case TE_BUILTIN: case TE_STRING: table = &ttm->tables.dictionary; break;
    default: FAIL(ttm,TTM_EINVAL);
    }

    /* Pass one: collect all the names */
    walker = hashwalk(table);
    while(hashnext(walker,&entry)) {
	Function* fcn = (Function*)entry;
	switch (tte) {
	case TE_ALL:
	    vlpush(vl,strdup((const char*)fcn->entry.name));
	    break;
	case TE_BUILTIN:	
	    if(fcn->fcn.builtin) vlpush(vl,strdup((const char*)fcn->entry.name));
	    break;
	case TE_STRING:	
	    if(!fcn->fcn.builtin) vlpush(vl,strdup((const char*)fcn->entry.name));
	    break;
	default: FAIL(ttm,TTM_EINVAL);
	}
    }
    hashwalkstop(walker);
    {
    void* contents = vlcontents(vl); /* suppress gcc warning */
    /* Sort the names */
    qsort(contents,(size_t)vllength(vl),sizeof(utf8*),stringcmp);
    }
    /* construct the final result */
    for(first=1,i=0;i<vllength(vl);i++,first=0) {
	if(!first)
	    vsappendn(ttm->vs.result,(const char*)ttm->meta.semic,u8size(ttm->meta.semic));
	/* Surround each name with <...> */
	vsappendn(ttm->vs.result,(const char*)ttm->meta.lbrc,u8size(ttm->meta.lbrc));
	vsappendn(ttm->vs.result,(const char*)vlget(vl,i),strlen((const char*)vlget(vl,i)));
	vsappendn(ttm->vs.result,(const char*)ttm->meta.rbrc,u8size(ttm->meta.rbrc));
    }
}

/* Sort helper function */
static int
stringcmp(const void* a, const void* b)
{
    const char** sa = (const char**)a;
    const char** sb = (const char**)b;
    return strcmp(*sa,*sb);
}

/**
#<ttm;meta;which;char>		# Set the value for various meta characters
#<ttm;info;name;{name}*>	# return info about each {name}
#<ttm;info;class;{class}*>	# return info about each {class}
#<ttm;list;{case};{name}*>	# return sorted list of names defined by case
*/
static void
ttm_ttm(TTM* ttm, Frame* frame) /* Misc. combined actions */
{
    utf8* which = NULL;
    utf8* klass = NULL;

    if(frame->argc < 2) FAIL(ttm,TTM_EFEWPARMS);
    which = frame->argv[1];
    switch (ttmenumdetect(which)) {
    case TE_META:
	ttm_ttm_meta(ttm,frame);
	break;
    case TE_INFO:
	if(frame->argc < 3) FAIL(ttm,TTM_EFEWPARMS);
	klass = frame->argv[2];
	switch (ttmenumdetect(klass)) {
	case TE_NAME:
	    ttm_ttm_info_name(ttm,frame);
	    break;
	case TE_CLASS:
	    ttm_ttm_info_class(ttm,frame);
	    break;
	case TE_STRING:
	    ttm_ttm_info_string(ttm,frame);
	    break;
	default: FAIL(ttm,TTM_ETTMCMD);
	}
	break;
    case TE_LIST:
	ttm_ttm_list(ttm,frame);
	break;
    default:
	FAIL(ttm,TTM_ETTMCMD);
	break;
    }
}

/**************************************************/

/**
 Builtin function table
*/

struct Builtin {
    char* name;
    size_t minargs;
    size_t maxargs;
    enum FcnSV sv;
    TTMFCN fcn;
};

/* TODO: fix the minargs values */

/* Define a subset of the original TTM functions */

/* Define some temporary macros */
#define ARB MAXARGS

static struct Builtin builtin_orig[] = {
    /* Dictionary Operations */
    {"ap",2,2,SV_S,ttm_ap}, /* Append to a string */
    {"cf",2,2,SV_S,ttm_cf}, /* Copy a function */
    {"cr",2,2,SV_S,ttm_cr}, /* Mark for creation */
    {"ds",2,2,SV_S,ttm_ds}, /* Define string */
    {"es",1,ARB,SV_S,ttm_es}, /* Erase string */
    {"sc",2,63,SV_SV,ttm_sc}, /* Segment and count */
    {"ss",2,2,SV_S,ttm_ss}, /* Segment a string */
    /* Stateful String Selection */
    {"cc",1,1,SV_SV,ttm_cc}, /* Call one character */
    {"cn",2,2,SV_SV,ttm_cn}, /* Call n characters */
    {"sn",2,2,SV_S,ttm_sn}, /* Skip n characters */ /*Batch*/
    {"cp",1,1,SV_SV,ttm_cp}, /* Call parameter */
    {"cs",1,1,SV_SV,ttm_cs}, /* Call segment */
    {"isc",4,4,SV_SV,ttm_isc}, /* Initial character scan */
    {"rrp",1,1,SV_S,ttm_rrp}, /* Reset residual pointer */
    {"scn",3,3,SV_SV,ttm_scn}, /* Character scan */
    /* Stateful String Scanning Operations */
    {"gn",2,2,SV_V,ttm_gn}, /* Give n characters */
    {"zlc",1,1,SV_V,ttm_zlc}, /* Zero-level commas */
    {"zlcp",1,1,SV_V,ttm_zlcp}, /* Zero-level commas and parentheses */
    {"flip",1,1,SV_V,ttm_flip}, /* Flip a string */ /*Batch*/
    {"trl",1,1,SV_V,ttm_trl}, /* Translate to lowercase */ /*Batch*/
//    {"thd",1,1,SV_V,ttm_thd}, /* Convert a hexidecimal value to decimal */ /*Batch*/
    /* Character Class Operations */
    {"ccl",2,2,SV_SV,ttm_ccl}, /* Call class */
    {"dcl",2,2,SV_S,ttm_dcl}, /* Define a class */
    {"dncl",2,2,SV_S,ttm_dncl}, /* Define a negative class */
    {"ecl",1,ARB,SV_S,ttm_ecl}, /* Erase a class */
    {"scl",2,2,SV_S,ttm_scl}, /* Skip class */
    {"tcl",4,4,SV_V,ttm_tcl}, /* Test class */
    /* Arithmetic Operations */
    {"abs",1,1,SV_V,ttm_abs}, /* Obtain absolute value */
    {"ad",2,ARB,SV_V,ttm_ad}, /* Add */
    {"dv",2,2,SV_V,ttm_dv}, /* Divide and give quotient */
    {"dvr",2,2,SV_V,ttm_dvr}, /* Divide and give remainder */
    {"mu",2,ARB,SV_V,ttm_mu}, /* Multiply */
    {"su",2,2,SV_V,ttm_su}, /* Substract */
    /* Numeric Comparisons */
    {"eq",4,4,SV_V,ttm_eq}, /* Compare numeric equal */
    {"gt",4,4,SV_V,ttm_gt}, /* Compare numeric greater-than */
    {"lt",4,4,SV_V,ttm_lt}, /* Compare numeric less-than */
    /* Logical Comparisons */
    {"eq?",4,4,SV_V,ttm_eql}, /* ? Compare logical equal */
    {"gt?",4,4,SV_V,ttm_gtl}, /* ? Compare logical greater-than */
    {"lt?",4,4,SV_V,ttm_ltl}, /* ? Compare logical less-than */
    /* Peripheral Input/Output Operations */
    {"cm",1,1,SV_S,ttm_cm}, /*Change Meta Character*/
    {"ps",1,ARB,SV_S,ttm_ps}, /* Print a sequence of strings */
    {"psr",1,1,SV_SV,ttm_psr}, /* Print string and then read */
    {"rs",0,0,SV_V,ttm_rs}, /* Read a string */
    /*Formated Output Operations*/
    {"names",0,1,SV_V,ttm_names}, /* Obtain name strings */
    /* Utility Operations */
    {"exit",0,0,SV_S,ttm_exit}, /* Return from TTM */
    {"ndf",3,3,SV_V,ttm_ndf}, /* Determine if a name is defined */
    {"norm",1,1,SV_V,ttm_norm}, /* Obtain the norm (length) of a string */
    {"time",0,0,SV_V,ttm_time}, /* Obtain time of day (modified) */
    {"xtime",0,0,SV_V,ttm_xtime}, /* Obtain execution time */ /*Batch*/
    {"tf",0,0,SV_S,ttm_tf}, /* Turn Trace Off */
    {"tn",0,0,SV_S,ttm_tn}, /* Turn Trace On */
    {"eos",3,3,SV_V,ttm_eos}, /* Test for end of string */ /*Batch*/
    {NULL,0,0,SV_SV,NULL} /* end of builtins list */
};

/* Functions new to this implementation */
static struct Builtin builtin_new[] = {
    {"argv",1,1,SV_V,ttm_argv}, /* Get ith command line argument; 0<=i<argc */
    {"argc",0,0,SV_V,ttm_argc}, /* no. of command line arguments */
    {"classes",0,0,SV_V,ttm_classes}, /* Obtain character class names */
    {"ctime",1,1,SV_V,ttm_ctime}, /* Convert time to printable string */
    {"include",1,1,SV_S,ttm_include}, /* Include text of a file */
    {"lf",0,ARB,SV_S,ttm_lf}, /* Lock functions */
    {"pf",0,1,SV_S,ttm_pf}, /* flush stderr and/or stdout */
    {"uf",0,ARB,SV_S,ttm_uf}, /* Unlock functions */
    {"ttm",1,ARB,SV_SV,ttm_ttm}, /* Misc. combined actions */
    {"ge",4,4,SV_V,ttm_ge}, /* Compare numeric greater-than */
    {"le",4,4,SV_V,ttm_le}, /* Compare numeric less-than */
    {"void",0,ARB,SV_S,ttm_void}, /* throw away all arguments and return an empty string */
    {"comment",0,ARB,SV_S,ttm_void}, /* alias for ttm_void */
    {"setprop",1,2,SV_SV,ttm_setprop}, /* Set property */
    {"resetprop",1,2,SV_SV,ttm_resetprop}, /* set property to default */
    {"pse",1,ARB,SV_S,ttm_ps}, /* Print a sequence of strings, but charified control chars */
    {"printf",1,ARB,SV_S,ttm_ps}, /* Emulate printf() using a format as arg 1 */
//    {"tru",1,1,SV_V,ttm_trl}, /* Translate to uppercase */
    {NULL,0,0,SV_SV,NULL} /* end of builtins list */
};

#if 0
/* Functions not implemented; they are irrelevant to this implementation*/
static struct Builtin builtin_irrelevant[] = {
    {"rcd",2,2,SV_S,ttm_rcd}, /* Set to Read from cards */
    {"fm",1,ARB,SV_S,ttm_fm}, /* Format a Line or Card */
    {"tabs",1,8,SV_S,ttm_tabs}, /* Declare Tab Positions */
    {"scc",2,2,SV_S,ttm_scc}, /* Set Continuation Convention */
    {"icc",1,1,SV_S,ttm_icc}, /* Insert a Control Character */
    {"outb",0,3,SV_S,ttm_outb}, /* Output the Buffer */
    /* Library Operations */
    {"store",2,2,SV_S,ttm_store}, /* Store a Program */
    {"delete",1,1,SV_S,ttm_delete}, /* Delete a Program */
    {"copy",1,1,SV_S,ttm_copy}, /* Copy a program */
    {"show",0,1,SV_S,ttm_show}, /* Show program strings */
    {"libs",2,2,SV_S,ttm_libs}, /* Declare standard qualifiers */ /*Batch*/
    {"break",0,1,SV_S,ttm_break}, /* Program Break */
    /* Batch Functions */
    {"insw",2,2,SV_S,ttm_insw}, /* Control output of input monitor */ /*Batch*/
    {"ttmsw",2,2,SV_S,ttm_ttmsw}, /* Control handling of ttm programs */ /*Batch*/
    {"cd",0,0,SV_V,ttm_cd}, /* Input one card */ /*Batch*/
    {"cdsw",2,2,SV_S,ttm_cdsw}, /* Control cd input */ /*Batch*/
    {"for",0,0,SV_V,ttm_for}, /* Input next complete fortran statement */ /*Batch*/
    {"forsw",2,2,SV_S,ttm_forsw}, /* Control for input */ /*Batch*/
    {"pk",0,0,SV_V,ttm_pk}, /* Look ahead one card */ /*Batch*/
    {"pksw",2,2,SV_S,ttm_pksw}, /* Control pk input */ /*Batch*/
    {"ps",1,1,SV_S,ttm_ps}, /* Print a string */ /*Batch*/ /*Modified*/
    {"page",1,1,SV_S,ttm_page}, /* Specify page length */ /*Batch*/
    {"sp",1,1,SV_S,ttm_sp}, /* Space before printing */ /*Batch*/
    {"fmsw",2,2,SV_S,ttm_fmsw}, /* Control fm output */ /*Batch*/
    {"des",1,1,SV_S,ttm_des}, /* Define error string */ /*Batch*/
    {NULL,0,0,NULL} /* terminator */
};
#endif /*0*/

static void
defineBuiltinFunction1(TTM* ttm, struct Builtin* bin)
{
    Function* fcn;

    /* Make sure we did not define builtin twice */
    fcn = dictionaryLookup(ttm,(utf8*)bin->name);
    if(fcn != NULL) FAIL(ttm,TTM_EDUPNAME);
    /* create a new function object */
    fcn = newFunction(ttm);
    fcn->fcn.builtin = 1;
    fcn->fcn.locked = 1;
    fcn->fcn.minargs = bin->minargs;
    fcn->fcn.maxargs = bin->maxargs;
    fcn->fcn.sv = bin->sv;
    switch (bin->sv) {
    case SV_S:	fcn->fcn.novalue = 1; break;
    case SV_V:	fcn->fcn.novalue = 0; break;
    case SV_SV: fcn->fcn.novalue = 0; break;
    }
    fcn->fcn.fcn = bin->fcn;
    /* Convert name to utf8 */
    fcn->entry.name = (utf8*)strdup(bin->name);
    if(!dictionaryInsert(ttm,fcn)) {
	freeFunction(ttm,fcn);
	FAIL(ttm,TTM_ETTM);
    }
}

static void
defineBuiltinFunctions(TTM* ttm)
{
    struct Builtin* bin;
    for(bin=builtin_orig;bin->name != NULL;bin++)
	defineBuiltinFunction1(ttm,bin);
    for(bin=builtin_new;bin->name != NULL;bin++)
	defineBuiltinFunction1(ttm,bin);
}

/**************************************************/

/* Reset the ttm state to execute the next full line of text. */
static void
ttmreset(TTM* ttm)
{
    vsclear(ttm->vs.active);
    vsclear(ttm->vs.passive);
    vsclear(ttm->vs.result);
    vsclear(ttm->vs.tmp);
    ttm->flags.lineno = 0;
}

/**************************************************/
/* Lock all the names in the dictionary */
static void
lockup(TTM* ttm)
{
    int i;
    for(i=0;i<HASHSIZE;i++) {
	struct HashEntry* entry = ttm->tables.dictionary.table[i].next;
	while(entry != NULL) {
	    Function* str = (Function*)entry;
	    str->fcn.locked = 1;
	    entry = entry->next;
	}
    }
}

/**************************************************/
/* Utility functions */

/**
Bash (and maybe other shells) has the unfortunate quirk
of adding escape characters to command line arguments when
they are significant to bash.
This function tries to undo that quirk.
@param arg
@return de-bashed arg
*/
static char*
debash(const char* s)
{
    char* deb = NULL;
    size_t len;
    const char* p = NULL;
    char* q = NULL;

    if(s == NULL) return NULL;
    len = strlen(s);
    deb = (char*)calloc(sizeof(char),len+1); /*+1 for nul terminator*/
    for(p=s,q=deb;*p;) {
	if(*p == '\\') p++;
	*q++ = *p++;
    }
    *q = '\0';
    return deb;
}

/* Determine SV_S vs SV_V vs SV_SV */
static const char*
sv(Function* f)
{
    switch (f->fcn.sv) {
    case SV_S: return "S";
    case SV_V: return "V";
    case SV_SV: return "SV";
    }
    return "?";
}

#if 0
/**
Remove the ith arg from the frame and set argc appropriately.
If ith >= argc, then return NULL;
@param frame to modify
@param ith 0<=ith<frame->argc
@return the removed arg
*/
static void
compressframe(Frame* frame, size_t ith)
{
    utf8* removed = NULL;
    size_t i;

    if(ith < frame->argc) {
	removed = frame->argv[ith];
	for(i=ith;i<(frame->argc - 1);i++) frame->argv[i] = frame->argv[i+1];
	frame->argc--;
    }
    return removed;
}
#endif

/**
Peek at the nth char past the index.
Return a pointer to that char.
Leave index unchanged.
If EOS is encountered, then no advancement occurs
@param vs
@param n number of codepoint to peek ahead
@param cpa store the peek'd codepoint
@return TTMERR
*/
static TTMERR
peek(VString* vs, size_t n, utf8* cpa)
{
    TTMERR err = TTM_NOERR;
    size_t saveindex = vsindex(vs);
    size_t i;
    utf8* p = NULL;

    p = (utf8*)vsindexp(vs);
    for(i=0;i<n;i++) {
	if(isnul(p)) break;
	p = (utf8*)vsindexskip(vs,(size_t)u8size(p));
    }
    memcpy(cpa,p,u8size(p));
    vsindexset(vs,saveindex);
    return err;
}

static TTMFILE*
ttmopen(TTM* ttm, const char* fname, const char* mode)
{
    int eno = 0;
    TTMFILE* tfile = NULL;
    tfile = (TTMFILE*)calloc(1,sizeof(TTMFILE));
    if(tfile == NULL) FAIL(ttm,TTM_EMEMORY);
    tfile->file = fopen(fname,mode);
    if(tfile->file == NULL) {eno = errno; goto done;}
    tfile->npushed = -1;

done:;
    errno = eno;
    return tfile;
}

static int
ttmclose(TTM* ttm, TTMFILE* tfile)
{
    int ret = 0;
    int eno = 0;
    if(tfile != NULL) {
	ret = fclose(tfile->file);
	if(ret == EOF) eno = errno;
	free(tfile);
    }
    errno = eno;
    return ret;
}

#ifdef GDB
int
ttmflush(TTM* ttm, TTMFILE* tfile)
{
    int ret = 0;
    if(tfile == NULL) FAIL(ttm,TTM_ETTM);
    ret = fflush(tfile->file);
    return ret;
}
#endif

static int
ttmerror(TTM* ttm, TTMFILE* tfile)
{
    int ret = 0;
    if(tfile == NULL) FAIL(ttm,TTM_ETTM);
    ret = ferror(tfile->file);
    clearerr(tfile->file);
    return ret;
}

static int
ttmeof(TTM* ttm, TTMFILE* tfile)
{
    int ret = 0;
    if(tfile == NULL) FAIL(ttm,TTM_ETTM);
    ret = feof(tfile->file);
    return ret;
}

/**************************************************/
/* Main() Support functions */

static void
initglobals()
{
    eoptions = vlnew();
    argoptions = vlnew();
}

static void
reclaimglobals()
{
    vlfreeall(eoptions);
    vlfreeall(argoptions);
}

static void
usage(const char* msg)
{
    if(msg != NULL)
	fprintf(stderr,"%s\n",msg);
    fprintf(stderr,"%s\n",
"usage: ttm <options> where the options are as follows:\n"
"[-d [t]]	  -- set debug flags:\n"
"		     't' -- turn on tracing\n"
"		     'v' -- turn on verbose output\n"
"[-e <command>]	  -- execute command\n"
"[-f inputfile]	  -- use this file instead of stdin\n"
"[-i]		  -- operate in interactive mode (default is on if isatty() is true)\n"
"[-o file]	  -- use this file instead of stdout\n"
"[-p programfile] -- main program to execute\n"
"[-q]		  -- operate in quiet mode\n"
"[-B]		  -- bare executionl; suppress startup commands\n"
"[-V]		  -- print version\n"
"[-P tag=value]	  -- set interpreter properties\n"
"[--]		  -- stop processing command line options\n"
"[arg...]	  -- arbitrary string arguments"
);
    fprintf(stderr,"Options may be repeated\n");
    if(msg != NULL) exit(1); else exit(0);
}

/**
Read one full line of iput from stdin.
Note for each line read if it ends in an escapechar+newline
then another line is read and appended to current line.
This repeats until a line is read that does not end in escapechar+newline.
Note that encountering a bare EOF/EOS also terminates the line.
@param ttm
@param f file from which to read
@param buf - hold line(s) read from stdin
@return TTM_NOERR if data; TTM_EXXX if error
*/
static TTMERR
readline(TTM* ttm, TTMFILE* f, VString* buf)
{
    TTMERR err = TTM_NOERR;
    int ncp;
    utf8cpa cp8;

    vssetlength(buf,0);
    for(;;) { /* Read thru next \n or \0 (EOF) */
	if((ncp=ttmnonl(ttm,f,cp8)) <= 0) {err = TTM_EUTF8; goto done;}
	if(isnul(cp8)) {err = TTM_EEOF; goto done;}
	if(isescape(cp8)) {
	    /* peek to see if this escape at end of line: '\''\n' */
	    if((ncp=ttmnonl(ttm,f,cp8)) <= 0) {err = TTM_EUTF8; goto done;}
	    if(u8equal(cp8,(utf8*)"\n")) {
		/* escape of \n => elide escape and \n and continue reading */
		continue;
	    } else { /* pass the escape and the escaped char */
		vsappendn(buf,(const char*)ttm->meta.escapec,u8size(ttm->meta.escapec));
		vsappendn(buf,(const char*)cp8,ncp);
	    }
	} if(u8equal(cp8,(const utf8*)"\n")) {
	    vsappendn(buf,(const char*)cp8,ncp);
	    break;
	} else { /* char other than nul or escape */
	    vsappendn(buf,(const char*)cp8,ncp);
	}
    }
done:
    return err;
}

#if 0
/**
Read from input until the read characters form a balanced string
with respect to the current open/close characters.
Read past the final balancing character to the next end of line.
Discard those extra characters.
Return 0 if the read was terminated by EOF. 1 otherwise.
*/

static int
readbalanced(TTM* ttm)
{
    Buffer* bb;
    utf8* content;
    size_t depth,i;
    size_t buffersize;

    resetBuffer(ttm,bb);
    buffersize = ttm->alloc;
    content = ttm->content;

    /* Read character by character until EOF; take escapes and open/close
       into account; keep outer <...> */
    for(depth=0,i=0;i<buffersize-1;i++) {
	c32 = fgetc32(stdin);
	if(c32 == EOF) break;
	if(c32 == ttm->meta.escapec) {
	    *content++ = c32;
	    c32 = fgetc32(stdin);
	}
	*content++ = c32;
	if(c32 == ttm->meta.openc) {
	    depth++;
	} else if(c32 == ttm->meta.closec) {
	    depth--;
	    if(depth == 0) break;
	}
    }
    /* skip to end of line */
    while(c32 != EOF && c32 != '\n') {c32 = fgetc32(stdin);}
    setBufferLength(ttm,bb,i);
    return (i == 0 && c32 == EOF ? 0 : 1);
}
#endif

/**
Read a file to EOF
@param ttn
@param file to read
@param buf hold text of file
@return TTM_NOERR or TTM_EXXX if error
*/
static TTMERR
readfile(TTM* ttm, const char* fname, VString* buf)
{
    TTMERR err = TTM_NOERR;
    TTMFILE* f = NULL;
    VString* oneline = vsnew();
    f = ttmopen(ttm,(const char*)fname,"rb");
    if(f == NULL) {err = errno; goto done;}
    for(;;) {
	vsclear(oneline);
	switch (err=readline(ttm,f,oneline)) {
	case TTM_NOERR: vsappendn(buf,vscontents(oneline),vslength(oneline)); break;
	case TTM_EEOF: break; /* no more input */
	default: goto done;
	}
    }
    ttmclose(ttm,f);
    vsfree(oneline);
done:
    return err;
}

/* This will be more complex if/when some fields are allocated space
   e.g. char*
*/
static struct Properties
propertiesclone(struct Properties* props)
{
    struct Properties clone = *props;
    return clone;
}

static void
setproperty(const char* key, const char* value, struct Properties* props)
{
    size_t nvalue;
    int tf;

    switch (propenumdetect((const utf8*)key)) {
    case PE_STACKSIZE:
	if(value == NULL) nvalue = 0;
	else if((1!=sscanf(value,"%zu",&nvalue))) goto fail;
	if(nvalue == 0) nvalue = DFALTSTACKSIZE;
	props->stacksize = nvalue;
	break;
    case PE_EXECCOUNT:
	if(value == NULL) nvalue = 0;
	else if((1!=sscanf(value,"%zu",&nvalue))) goto fail;
	if(nvalue == 0) nvalue = DFALTEXECCOUNT;
	props->execcount = nvalue;
	break;
    case PE_SHOWFINAL:
	tf = (value == NULL ? 0 : 1);
	props->showfinal = tf;
	break;
    default:
	goto fail;
    }
fail:
    fprintf(stderr,"SetProperty: Illegal Property argument or value: %s=%s\n",
	    (key == NULL ? "NULL" : key), (value == NULL ? "NULL" : value));
    exit(1);
}

static void
resetproperty(struct Properties* props, const char* key, const struct Properties* dfalts)
{
    switch (propenumdetect((const utf8*)key)) {
    case PE_STACKSIZE:
	props->stacksize = dfalts->stacksize;
	break;
    case PE_EXECCOUNT:
	props->execcount = dfalts->execcount;
	break;
    case PE_SHOWFINAL:
	props->showfinal = dfalts->showfinal;
	break;
    default:
	goto fail;
    }
fail:
    fprintf(stderr,"ResetProperty: Illegal Property key: %s\n",
	    (key == NULL ? "NULL" : key));
    exit(1);
}

static void
processdebugargs(TTM* ttm, const char* debugargs)
{
    const char* p;
    ttm->debug = dfalt_debug;
    for(p=debugargs;*p;p++) {
	switch (*p) {
	case 't':
	    ttm->debug.trace = 1;
	    break;
	case 'v':
	    ttm->debug.verbose = 1;
	    break;
	default:
	    fprintf(stderr,"Unknown debug flag: '%c'\n",*p);
	    exit(1);
	}
    }
}


/**************************************************/
/* Helper functions for main */

static int
setupio(TTM* ttm, const char* outfile, const char* infile, int forceinteractive)
{
    TTMERR err = TTM_NOERR;
    TTMFILE* io = NULL;

    assert(ttm != NULL);
    if((io = (TTMFILE*)calloc(sizeof(TTMFILE),1))==NULL) FAIL(ttm,TTM_EMEMORY);
    if(infile == NULL) {
	io->file = stdin;
	io->isstd = 1;
    } else {
	io->file = fopen(infile,"wb");
	if(io->file == NULL) {
	    fprintf(stderr,"File cannot be accessed: %s\n",infile);
	    err = errno;
	    free(io);
	    goto done;
	}
	io->isstd = 0;
    }
    if(forceinteractive || isatty(fileno(io->file))) io->interactive = 1;
    ttm->io.stdin = io; io = NULL;

    if((io = (TTMFILE*)calloc(sizeof(TTMFILE),1))==NULL) FAIL(ttm,TTM_EMEMORY);
    if(outfile == NULL) {
	io->file = stdout;
	io->isstd = 1;
    } else {
	io->file = fopen(outfile,"wb");
	if(io->file == NULL) {
	    fprintf(stderr,"File cannot be accessed: %s\n",outfile);
	    err = errno;
	    free(io);
	    goto done;
	}
	io->isstd = 0;
    }
    if(forceinteractive || isatty(fileno(io->file))) io->interactive = 1;
    ttm->io.stdout = io; io = NULL;

    if((io = (TTMFILE*)calloc(sizeof(TTMFILE),1))==NULL) FAIL(ttm,TTM_EMEMORY);
    if(1) {
	io->file = stderr;
	io->isstd = 1;
    } else {
	io->file = fopen(NULL,"wb");
	if(io->file == NULL) {
	    fprintf(stderr,"File cannot be accessed: %s\n",outfile);
	    err = errno;
	    free(io);
	    goto done;
	}
	io->isstd = 0;
    }
    if(forceinteractive || isatty(fileno(io->file))) io->interactive = 1;
    ttm->io.stderr = io; io = NULL;

done:
    errno = 0; /* reset */
    return err;
}

static void
closeio(TTM* ttm)
{
    if(ttm != NULL && ttm->io.stdin != NULL && !ttm->io.stdin->isstd) ttmclose(ttm,ttm->io.stdin);
    if(ttm != NULL && ttm->io.stdout != NULL && !ttm->io.stdout->isstd) ttmclose(ttm,ttm->io.stdout);\
    if(ttm != NULL && ttm->io.stderr != NULL && !ttm->io.stderr->isstd) ttmclose(ttm,ttm->io.stderr);
}

/**************************************************/

/**
Execute a string for side effects and throw away any result.
*/
static TTMERR
execcmd(TTM* ttm, const char* cmd)
{
    TTMERR err = TTM_NOERR;
    int savetrace = ttm->debug.trace;
    size_t cmdlen = strlen(cmd);
    utf8* cp8;

#ifdef GDB
    ttm->debug.trace = 1;
#else
    ttm->debug.trace = 0;
#endif
    ttmreset(ttm);
    vsinsertn(ttm->vs.active,0,cmd,cmdlen);
    vsindexset(ttm->vs.active,0);
    TTMINIT(cp8);
    if((err = scan(ttm))) goto done;
    ttmreset(ttm);
    ttm->debug.trace = savetrace;
done:
    return THROW(err);
}

#if 0
static void
startupcommands(TTM* ttm)
{
    char* cmd;
    char** cmdp;

#ifdef GDB
    fprintf(stderr,"Begin startup commands\n");
#endif
    for(cmdp=startup_commands;*cmdp != NULL;cmdp++) {
	cmd = *cmdp;
	execstring(ttm,cmd);
    }
#ifdef GDB
    fprintf(stderr,"End startup commands\n");
    fflush(stderr);
#endif
}
#endif

static TTMERR
execall(TTM* ttm)
{
    TTMERR err = TTM_NOERR;
    char* cmd = NULL;

    if(!ttm->opts.bare) {
	char** cmdp;
#ifdef GDB
	fprintf(stderr,"Begin startup commands\n");
#endif
	for(cmdp=startup_commands;*cmdp != NULL;cmdp++) {
	    cmd = strdup(*cmdp);
	    if((err = execcmd(ttm,cmd))) goto done;
	    nullfree(cmd); cmd = NULL;
	}
#ifdef GDB
	fprintf(stderr,"End startup commands\n");
	fflush(stderr);
#endif
    }

    /* Execute the -e strings in turn */
    while(vllength(eoptions)> 0) {
	cmd = vlget(eoptions,0);
	if((err=execcmd(ttm,cmd))) goto done;
	nullfree(cmd); cmd = NULL;
	if(ttm->flags.exit) goto done;
    }

    /* Now execute the executefile, if any, and discard output */
    if(ttm->opts.executefilename != NULL) {
        size_t cmdlen = 0;
	readfile(ttm,ttm->opts.executefilename,ttm->vs.tmp); /* read whole execute file */
	/* Remove '\\' escaped */
	cmd = (char*)deescape((const utf8*)vscontents(ttm->vs.tmp),&cmdlen);
	if(cmd == NULL) {err = TTM_EUTF8; goto done;}
#if DEBUG > 0
	if(ttm->debug.trace) {
	    char* tmp = (char*)printclean((const utf8*)cmd,"\t",NULL);
	    xprintf(ttm,"scan: %s\n",tmp);
	    nullfree(tmp); tmp = NULL;
	}
#endif
	if((err=execcmd(ttm,cmd))) goto done;
        if(ttm->flags.exit) goto done;
    }

    /* If interactive, start read-eval loop */
    if(ttm->opts.interactive) {
	int quit = 0;
	while(!quit) {
	    switch (err=readline(ttm,ttm->io.stdin,ttm->vs.tmp)) {
	    case TTM_NOERR: break;
	    case TTM_EEOF: quit = 1; break;
	    default: goto done;
	    }
	    cmd = vsextract(ttm->vs.tmp);
	    if((err=execcmd(ttm,cmd))) goto done;
	    if(!ttm->opts.quiet)
		printstring(ttm,(const utf8*)print2len(ttm->vs.passive),ttm->io.stdout);
	    if(ttm->flags.exit) goto done;
	}
    }
done:
    nullfree(cmd);
    return THROW(err);
}

/**************************************************/
/* Main() */

int
main(int argc, char** argv)
{
    TTMERR err = TTM_NOERR;
    int exitcode;
    long stacksize = 0;
    long execcount = 0;
    char debugargs[16] = {'\0'};
    char* outputfilename = NULL;
    char* inputfilename = NULL; /* This is data for #<rs> */
    int c;
    char* p;
    char* q;
    struct OPTS opts;
    struct Properties option_props;
#ifndef TTMGLOBAL
    TTM* ttm = NULL;
#endif

    memset(&opts,0,sizeof(struct OPTS));

    if(argc == 1)
	usage(NULL);

    initglobals();

    option_props = dfalt_properties; /* initial properties */

    /* Stash argv[0] */
    vlpush(argoptions,strdup(argv[0]));

    /* Special/Platform initializations */
#ifdef MSWINDOWS
    ttm->opts.interactive = (_isatty(fileno(stdin)) ? 1 : 0);
#else /*!MSWINDOWS*/
//    ttm->opts.interactive = (isatty(fileno(stdin)) ? 1 : 0);
#endif /*!MSWINDOWS*/

    while ((c = getopt(argc, argv, "d:e:f:io:p:qVB-")) != EOF) {
	switch(c) {
	case 'd':
	    strcat(debugargs,optarg);
	    break;
	case 'e':
	    if(strlen(optarg) > 0) {
		/* We have to deescape the argument because of a bash quirk */
		char* s= debash(optarg);
		/* Now convert any '\\' escaped chars */
		s = (char*)deescape((const utf8*)s,NULL);
		vlpush(eoptions,s);
	    }
	    break;
	case 'f':
	    if(inputfilename == NULL)
		inputfilename = strdup(optarg);
	    ttm->opts.interactive = 0;
	    break;
	case 'i':
	    ttm->opts.interactive = 1;
	    break;
	case 'o':
	    if(outputfilename == NULL)
		outputfilename = strdup(optarg);
	    break;
	case 'p':
	    if(ttm->opts.executefilename == NULL)
		ttm->opts.executefilename = strdup(optarg);
	    break;
	case 'q': ttm->opts.quiet = 1; break;
	case 'P': /* Set properties*/
	    if(optarg == NULL) usage("Illegal -P key");
	    if(strlen(optarg) == 0) {
		usage("Illegal -P key");
	    } else {
		char* debkey = NULL;
		char* debval = NULL;
		p = strchr(optarg,'=');
		/* get pointer to value */
		if(p == NULL) q = optarg+strlen(optarg); else q = p+1;
		debkey = debash(p);
		debval = debash(q);
		setproperty(debkey,debval,&option_props);
	    }
	    break;
	case 'B': ttm->opts.bare = 1; break;
	case 'V':
	    printf("ttm version: %s\n",VERSION);
	    exit(0);
	    break;
	case '-':
	    break;
	case '?':
	default:
	    usage("Illegal option");
	}
    }

    /* Collect any args for #<arg> */
    if(optind < argc) {
	for(;optind < argc;optind++)
	    vlpush(argoptions,strdup(argv[optind]));
    }

    /* Complain if interactive and output file name specified */
    if(outputfilename != NULL && ttm->opts.interactive) {
	fprintf(stderr,"Interactive is illegal if output file specified\n");
	exit(1);
    }

    if(stacksize < DFALTSTACKSIZE)
	stacksize = DFALTSTACKSIZE;
    if(execcount < DFALTEXECCOUNT)
	execcount = DFALTEXECCOUNT;

    /* Create the ttm state */
    /* Modify from various options */
    ttm = newTTM(&option_props);
    processdebugargs(ttm,debugargs);
    defineBuiltinFunctions(ttm);

    /* Lock up all the currently defined functions */
    lockup(ttm);

    if((err=setupio(ttm,outputfilename,inputfilename,ttm->opts.interactive))) {
	fprintf(stderr,"IO setup failure: (%d) %s\n",(int)err,errstring(err));
	exit(1);
    }

    if((err = execall(ttm))) goto fail;

#if 0
    /* Dump any output left in the buffer */
    if(!quiet && vslength(ttm->vs.passive) > 0 && vscontents(ttm->vs.passive)[0] != '\n') {
	printstring(ttm,(const utf8*)print2len(ttm->vs.passive),ttm->io.stdout);
    }
#endif

done:
    exitcode = ttm->flags.exitcode;

    if(err) FAIL(ttm,err);

    /* cleanup */
    closeio(ttm);

    /* Clean up misc state */
    nullfree(outputfilename);
    nullfree(inputfilename);

    freeTTM(ttm);

    reclaimglobals();

    return (exitcode?1:0); // exit(exitcode);

fail:
    exitcode = 1;
    goto done;
}

static int
u8sizec(utf8 c)
{
    if(c == SEGMARK0) return SEGMARKSIZE; /* segment|create mark */
    if((c & 0x80) == 0x00) return 1; /* us-ascii */
    if((c & 0xE0) == 0xC0) return 2;
    if((c & 0xF0) == 0xE0) return 3;
    if((c & 0xF8) == 0xF0) return 4;
    return -1;
}

static int
u8size(const utf8* cp)
{
    return u8sizec(*cp);
}

/* return no. bytes in codepoint or -1 if codepoint invalid */
/* We use a simplified test for validity */
static int
u8validcp(utf8* cp)
{
    int ncp = u8size(cp);
    int i;
    if(ncp == 0) return -1;
    /* Do validity check */
    /* u8size checks validity of first byte */
    for(i=1;i<ncp;i++) {
	if((cp[i] & 0xC0) != 0x80) return -1; /* bad continuation char */
    }
    return ncp;
}

static void
ascii2u8(char c, utf8* u8)
{
    *u8 = (utf8)(0x7F & (unsigned)c);
}

static int
u8equal(const utf8* c1, const utf8* c2)
{
    int l1 = u8size(c1);
    int l2 = u8size(c2);
    if(l1 != l2) return 0;
    if(memcmp((void*)c1,(void*)c2,l1)!=0) return 0;
    return 1;
}

/**
Copy a single codepoint from src to dst
@param dst target for the codepoint
@param src src for the codepoint
@return no. of bytes in codepoint
*/
static int
memcpycp(utf8* dst, const utf8* src)
{
    int count = u8size(src);
    memcpy(dst,src,count);
    return count;
}

/**
Count no. of codepoints in src to src+n or EOS.
This is sort of the inverse of u8ith().
@param dst target for the codepoint
@param src src for the codepoint
@param n check in src to src+n
@return no. of codepoints or -1 if malformed
*/
static int
u8cpcount(const utf8* src, size_t n)
{
    size_t nbytes = strlen((const char*)src); /* bytes as opposed to codepoints */
    int cpcount = 0;
    size_t offset = 0;
    if(nbytes > n) nbytes = n;
    for(offset=0;offset < nbytes || isnul(src);) {
	int ncp = u8size(src);
	if(ncp < 0) return -1;
	if((offset + (unsigned)ncp) > nbytes) return -1; /* bad codepoint */
	src += ncp;
	offset += ncp;
	cpcount++;
    }
    if(offset < nbytes) return -1; /* extraneous extra bytes at end */
    return cpcount;
}

/**
Compute a pointer to base[n], where base is in units
of codepoints.	if length of base in codepoints is less than n
(i.e. we hit EOS), then return pointer to the EOS.
@param base start of the codepoint vector
@param n offset from base in units of codepoints
@return pointer to base[n] in units of codepoints or NULL if malformed
*/
static const utf8*
u8ithcp(const utf8* base, size_t n)
{
    const utf8* p;
    size_t nbytes = strlen((const char*)base); /* bytes as opposed to codepoints */
    for(p=base;n > 0; n--) {
	int ncp = u8size(p);
	if(isnul(p)) break; /* hit EOS */
	if(ncp < 0 || nbytes < (size_t)ncp) return NULL; /* bad codepoint */
	p += ncp; /* skip across this codepoint */
	nbytes -= ncp; /* track no. of remaining bytes in src */
    }
    return p;
}

/**
Compute the position of base[n] in bytes, where base is in units
of codepoints.	if length of base in codepoints is less than n
(i.e. we hit EOS), then return that lesser count.
@param base start of the codepoint vector
@param n offset from base in units of codepoints
@return offset (in bytes) of base[n] from base in units of codepoints or -1 if malformed
*/
static int
u8ith(const utf8* base, size_t n)
{
    const utf8* p = u8ithcp(base,n);
    return (int)(p - base);
}


/**
Given a utf8 pointer, backup one codepoint.  This is doable
because we can recognize the start of a codepoint.

Special case is required when backing up over a segment mark since it assumes
that the raw SEGMARK is at the beginning of the mark.

@param p0 pointer to back up
@return pointer to codepoint before p0
*/
static const utf8*
u8backup(const utf8* p)
{
    while((*p & 0xB0) == 0xB0) p--; /* backup over all continuation bytes */
    /* we should be at the start of the codepoint or segmark */
    return p;
}

/**************************************************/
/* Character Input/Output */

/* Unless you know that you are outputing ASCII,
   all I/O should go thru these procedures.
*/

/*
Reads one codepoint
All reading of characters should go thru this procedure.
Provides an upto 4 character backup for peeking.
Note that '\r\n' is converted to just '\n' to simplify
code for higher levels.
@param ttm
@param f FILE* from which to read.
@param cp8 dst for the codepoint (not nul terminated)
@return size of the codepoint
*/
static int
ttmgetc8(TTM* ttm, TTMFILE* f, utf8* cp8)
{
    int c;
    int i, cplen;

    if(f->npushed >= 0) {
	memcpycp(cp8,f->stack[f->npushed--]);
	cplen = u8size(cp8);
    } else {
	c = fgetc(f->file); /* read first char of codepoint */
	if(ttmeof(ttm,f)) c = '\0';
	if(ttmerror(ttm,f)) FAIL(ttm,errno);
	cp8[0] = c;
	cplen = u8sizec(c);
	for(i=1;i<cplen;i++) { /* read rest of the codepoint */
	    c=fgetc(f->file);
	    cp8[i] = (char)c;
	    if(c == EOF) {cp8[0] = NUL8; cplen = 1; break;}
	}	    
	if(ttmerror(ttm,f)) FAIL(ttm,TTM_EIO);
	cplen = u8size(cp8);
	if(cplen == 0) FAIL(ttm,TTM_EUTF8);
	if(u8validcp(cp8) < 0) FAIL(ttm,TTM_EUTF8);
    }
    return cplen;
}

/**
Push back a codepoint; the pushed codepoints form a stack.
@param ttm
@param cp the codepoint to push
@return void
*/
static void
ttmpushbackc(TTM* ttm, TTMFILE* f, utf8* cp8)
{
    if(f->npushed >= (MAXPUSHBACK-1)) FAIL(ttm,TTM_EIO); /* too many pushes */
    memcpycp(f->stack[++f->npushed],cp8); /* push to stack */
}

/**
Convert occurrences of \r\n -> \n.
A wrapper arount ttmgetc8, so has same signature.
Uses pushback.
@param ttm
@param f FILE* from which to read.
@param cp8 dst for the codepoint (not nul terminated)
@return size of the codepoint
*/
static int
ttmnonl(TTM* ttm, TTMFILE* f, utf8* cp8)
{
    size_t ncp1,ncp2;
    utf8cpa char1;
    utf8cpa char2;
    ncp1 = ttmgetc8(ttm,f,char1);
    if(ncp1 == 1 && char1[0] == '\r') {
	/* check for following '\n' */
	ncp2 = ttmgetc8(ttm,f,char2);	
	if(ncp2 != 1 || char2[0] != '\n') {
	   ttmpushbackc(ttm,f,char2); /* pushback second char if not \n and let \r pass	 thru */
	}
    }
    /* send the original char */
    memcpy(cp8,char1,ncp1); /* send the \r or \n */
    return ncp1;
}

/*
Writes one codepoint
All writing of characters should go thru this procedure.
@param f FILE* to which to write.
@param cp8 src for the codepoint (not nul terminated)
@return size of the codepoint
*/
static int
ttmputc8(TTM* ttm, const utf8* c8, TTMFILE* f)
{
    int i, cplen;
    int c;

    cplen = u8size(c8);
    if(cplen == 0) FAIL(ttm,TTM_EUTF8);
    for(i=0;i<cplen;i++) {
	c = (c8[i] & 0xFF);
	/* Track if current output is at newline */
	fputc(c,f->file);
    }
    return cplen;
}

/**************************************************/
/**
Implement functions that deal with Linux versus Windows
differences.
*/
/**************************************************/

#ifdef MSWINDOWS

static long long
getRunTime(void)
{
    long long runtime;
    FILETIME ftCreation,ftExit,ftKernel,ftUser;

    GetProcessTimes(GetCurrentProcess(),
		    &ftCreation, &ftExit, &ftKernel, &ftUser);

    runtime = ftUser.dwHighDateTime;
    runtime = runtime << 32;
    runtime = runtime | ftUser.dwLowDateTime;
    /* Divide by 10000 to get milliseconds from 100 nanosecond ticks */
    runtime /= 10000;
    return runtime;
}

#define DELTA_EPOCH_IN_MICROSECS  11644473600000000ULL

static int tzflag = 0;

static int
timeofday(struct timeval *tv)
{
    /* Define a structure to receive the current Windows filetime */
    FILETIME ft;

    /* Initialize the present time to 0 and the timezone to UTC */
    unsigned long long tmpres = 0;

    if(NULL != tv) {
	GetSystemTimeAsFileTime(&ft);

	/*
	The GetSystemTimeAsFileTime returns the number of 100 nanosecond
	intervals since Jan 1, 1601 in a structure. Copy the high bits to
	the 64 bit tmpres, shift it left by 32 then or in the low 32 bits.
	*/
	tmpres |= ft.dwHighDateTime;
	tmpres <<= 32;
	tmpres |= ft.dwLowDateTime;

	/* Convert to microseconds by dividing by 10 */
	tmpres /= 10;

	/* The Unix epoch starts on Jan 1 1970.	 Need to subtract the difference
	   in seconds from Jan 1 1601.*/
	tmpres -= DELTA_EPOCH_IN_MICROSECS;

	/* Finally change microseconds to seconds and place in the seconds value.
	   The modulus picks up the microseconds. */
	tv->tv_sec = (long)(tmpres / 1000000UL);
	tv->tv_usec = (long)(tmpres % 1000000UL);
    }
    return 0;
}

#else /*!MSWINDOWS */

static long frequency = 0;

static long long
getRunTime(void)
{
    long long runtime;
    struct tms timers;
    clock_t tic;

    (void)tic; /* unused */
    if(frequency == 0)
	frequency = sysconf(_SC_CLK_TCK);

    tic=times(&timers);
    runtime = timers.tms_utime; /* in clock ticks */
    runtime = ((runtime * 1000) / frequency) ; /* runtime in milliseconds */
    return runtime;
}


static int
timeofday(struct timeval *tv)
{
    return gettimeofday(tv,NULL);
}

#endif /*!MSWINDOWS */


/**************************************************/
/* Getopt */

#ifdef MSWINDOWS
/**
 XGetopt.h  Version 1.2

 Author:  Hans Dietrich
	  hdietrich2@hotmail.com

 This software is released into the public domain.
 You are free to use it in any way you like.

 This software is provided "as is" with no expressed
 or implied warranty.  I accept no liability for any
 damage or loss of business that this software may cause.
*/

static int
getopt(int argc, char **argv, char *optstring)
{
    static char *next = NULL;
    char c;
    char *cp = calloc(sizeof(char),1024);

    if(optind == 0)
	next = NULL;
    optarg = NULL;
    if(next == NULL || *next == ('\0')) {
	if(optind == 0)
	    optind++;
	if(optind >= argc
	    || argv[optind][0] != ('-')
	    || argv[optind][1] == ('\0')) {
	    optarg = NULL;
	    if(optind < argc)
		optarg = argv[optind];
	    return EOF;
	}
	if(strcmp(argv[optind], "--") == 0) {
	    optind++;
	    optarg = NULL;
	    if(optind < argc)
		optarg = argv[optind];
	    return EOF;
	}
	next = argv[optind];
	next++;	    /* skip past - */
	optind++;
    }
    c = *next++;
    cp = strchr(optstring, c);
    if(cp == NULL || c == (':'))
	return ('?');
    cp++;
    if(*cp == (':')) {
	if(*next != ('\0')) {
	    optarg = next;
	    next = NULL;
	} else if(optind < argc) {
	    optarg = argv[optind];
	    optind++;
	} else {
	    return ('?');
	}
    }
    return c;
}
#endif /*MSWINDOWS*/
