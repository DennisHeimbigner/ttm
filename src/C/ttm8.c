/**
This software is released under the terms of the Apache License version 2.
For details of the license, see http://www.apache.org/licenses/LICENSE-2.0.
*/

/**************************************************/

#define VERSION "1.0"

/**************************************************/

#undef DEBUG
#undef GDB

#ifdef DEBUG
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

#if  defined _WIN32 || defined _MSC_VER
#define MSWINDOWS 1
#endif

/* Reduce visual studio verbosity */
#ifdef MSWINDOWS
#define _CRT_SECURE_NO_WARNINGS 1
#endif

/**************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <assert.h>

#ifdef MSWINDOWS
#include <windows.h>  /* To get GetProcessTimes() */
#else /*!MSWINDOWS*/
#include <unistd.h> /* This defines getopt */
#include <sys/times.h> /* to get times() */
#include <sys/time.h> /* to get gettimeofday() */
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
static char* optarg;            /* global argument pointer */
static int   optind = 0;        /* global argv index */
static int getopt(int argc, char **argv, char *optstring);
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

We use the "char_t" type to indicate the
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

typedef char char_t;

typedef unsigned char utf8; /* 8-bit utf char type. */

/* To simplify passing of a codepoint,
   we declare it as an unsigned int.
   This works since a utf8 codepoint is 1-4
   bytes in length. Note that this is
   mapped where needed to the utf8cpa type
   using the utf8cpu union.
*/
typedef unsigned utf8cp;
#define empty_u8cp ((unsigned)0)

typedef utf8 utf8cpa[MAXCP8SIZE];
#define empty_u8cpa {0,0,0,0}

/* cp converter union */
typedef union utf8cpu {utf8cp cp; utf8cpa cpa;} utf8cpu;

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
Define an internal form of memmove it not
defined by platform.
@param dst where to store bytes
@param src source of bytes
@param len no. of bytes to move
@return void
*/
static void
memmovex(utf8* dst, utf8* src, size_t len)
{
#ifdef HAVE_MEMMOVE
    memmove((void*)dst,(void*)src,len*sizeof(utf8));
#else   
    src += len;
    dst += len;
    for(;len>0;len--) {*dst-- = *src--;}
#endif
}

/**************************************************/
/**
Constants
*/

/**
This code takes over part of the UTF8 space to store segment
marks and create marks. Specifically the illegal continuation
byte space is used: the two byte value range 0x80 through 0xBF.
This is divided as follows:
1. The (singleton) create mark sequenceL 0xB0,0xBF
2. The 63 segment marks: 0xB0,0x80...0xBE
*/

#define SEGSIZE     2 /*bytes*/
#define SEGMARK     ((utf8)0xB0)
#define CREATEMARK  SEGMARK
/* Mask off the top two bits of the index part */
#define SEGMARKMASK ((utf8)0x3F)
#define CREATEINDEX 0xBF
/* Macros to set/clear/test flags */
#define setFlag(w,flag) ((w) | (flag))
#define clearFlag(w,flag) ((w) & ~(flag))
#define testFlag(w,flag) (((w) & (flag)) == 0 ? 0 : 1)

/* Segment mark contants (term "mark" comes from origin CalTech TTM). */
#define MAXMARKS 62
#define MAXARGS 63
#define MAXINCLUDES 1024
#define MAXEOPTIONS 1024
#define MAXINTCHARS 32

#define NUL8 '\0'

#define COMMA ','
#define LPAREN '('
#define RPAREN ')'
#define LBRACKET '['
#define RBRACKET ']'

#define DFALTBUFFERSIZE (1<<20)
#define DFALTSTACKSIZE 64
#define DFALTEXECCOUNT (1<<20)

#define CONTEXTLEN 20

#define CREATELEN 4 /* # of characters for a create mark */

/*Mnemonics*/
#define TRACING 1
#define TOEOS ((size_t)0xffffffffffffffff)

#ifdef DEBUG
#define PASSIVEMAX 20
#define ACTIVEMAX 20
#endif

/* TTM Flags */
#define FLAG_EXIT  1
#define FLAG_TRACE 2
#define FLAG_BARE 4 /* Do not do startup initializations */

/**************************************************/
/* Error Numbers */
/* Renamed TTM_EXXX because of multiple Windows conflicts */
typedef enum TTMERR {
TTM_ENOERR          =  0, /* No error; for completeness */
TTM_ENONAME         =  1, /* Dictionary Name or Character Class Name Not Found */
TTM_ENOPRIM         =  2, /* Primitives Not Allowed */
TTM_EFEWPARMS       =  3, /* Too Few Parameters Given */
TTM_EFORMAT         =  4, /* Incorrect Format */
TTM_EQUOTIENT       =  5, /* Quotient Is Too Large */
TTM_EDECIMAL        =  6, /* Decimal Integer Required */
TTM_EMANYDIGITS     =  7, /* Too Many Digits */
TTM_EMANYSEGMARKS   =  8, /* Too Many Segment Marks */
TTM_EMEMORY         =  9, /* Dynamic Storage Overflow */
TTM_EPARMROLL       = 10, /* Parm Roll Overflow */
TTM_EINPUTROLL      = 11, /* Input Roll Overflow */
#ifdef IMPLEMENTED
TTM_EDUPLIBNAME     = 12, /* Name Already On Library */
TTM_ELIBNAME        = 13, /* Name Not On Library */
TTM_ELIBSPACE       = 14, /* No Space On Library */
TTM_EINITIALS       = 15, /* Initials Not Allowed */
TTM_EATTACH         = 16, /* Could Not Attach */
#endif
TTM_EIO             = 17, /* An I/O Error Occurred */
#ifdef IMPLEMENTED
TTM_ETTM            = 18, /* A TTM Processing Error Occurred */
TTM_ESTORAGE        = 19, /* Error In Storage Format */
#endif
TTM_ENOTNEGATIVE    = 20, 
/* Error messages new to this implementation */
TTM_ESTACKOVERFLOW  = 30, /* Leave room */
TTM_ESTACKUNDERFLOW = 31, 
TTM_EBUFFERSIZE     = 32, /* Buffer overflow */
TTM_EMANYINCLUDES   = 33, /* Too many includes (obsolete)*/
TTM_EINCLUDE        = 34, /* Cannot read Include file */
TTM_ERANGE          = 35, /* index out of legal range */
TTM_EMANYPARMS      = 36, /* # parameters > MAXARGS */
TTM_EEOS            = 37, /* Unexpected end of string */
TTM_EASCII          = 38, /* ASCII characters only */
TTM_ECHAR8          = 39, /* Illegal 8-bit character set value */
TTM_ETTMCMD         = 40, /* Illegal #<ttm> command */
TTM_ETIME           = 41, /* gettimeofday failed */
TTM_EEXECCOUNT	    = 42, /* too many execution calls */
/* Default case */
TTM_EOTHER	    = 99
} TTMERR;

/**************************************************/
/* "inline" functions */

#define isescape(cpa) u8equal(cpa,ttm->escapec)

#define ismark(cp)(*(cp) == SEGMARK)||*(cp),CREATE)?1:0)
#define issegmark(cp)(*(cp) == SEGMARK)?1:0)
#define iscreateindex(idx)((idx) == CREATEINDEX);

#define ismultibyte(c) (((c) & 0x80) == 0x80 ? 1 : 0)

#define iswhitespace(c) ((c) <= ' ' ? 1 : 0)

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

/**************************************************/
/**
Structure Type declarations
*/

typedef struct TTM TTM;
typedef struct Function Function;
typedef struct Charclass Charclass;
typedef struct Frame Frame;
typedef struct Buffer Buffer;

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

/* Generic operations */
static int hashLocate(struct HashTable* table, utf8* name, struct HashEntry** prevp);
static void hashRemove(struct HashTable* table, struct HashEntry* prev, struct HashEntry* entry);
static void hashInsert(struct HashTable* table, struct HashEntry* prev, struct HashEntry* entry);

/**************************************************/
/**
TTM state object
*/

struct TTM {
    struct Limits {
        size_t buffersize;
        size_t stacksize;
        size_t execcount;
    } limits;
    unsigned flags;
    unsigned exitcode;
    unsigned crcounter; /* for cr marks */
    utf8cpa sharpc; /* sharp-like char */
    utf8cpa openc; /* <-like char */
    utf8cpa closec; /* >-like char */
    utf8cpa semic; /* ;-like char */
    utf8cpa escapec; /* escape-like char */
    utf8cpa metac; /* read eof char */
    Buffer* buffer; /* contains the string being processed */
    Buffer* result; /* contains result strings from functions */
    size_tstacknext; /* |stack| == (stacknext) */
    Frame* stack;    
    FILE* output;    
    int   isstdout;
    FILE* input;
    int   isstdin;
    /* Following 2 fields are hashtables indexed by low order 7 bits of some character */
    struct HashTable dictionary;
    struct HashTable charclasses;
};

/**
Define a fixed size byte buffer
for holding the current state of the expansion.
Buffer always has an extra terminating NUL ('\0').
Note that by using a buffer that is allocated once,
we can use pointers into the buffer space in e.g. struct Function.
 */

struct Buffer {
    size_t alloc;  /* including trailing NUL */
    size_t length;  /* including trailing NUL; defines what of
                       the allocated space is actual content. */
    utf8* active; /* characters yet to be scanned */
    utf8* passive; /* characters that will never be scanned again */
    utf8* end; /* into content */
    utf8* content; 
};

/**
  Define a ttm frame
*/

struct Frame {
  utf8* argv[MAXARGS+1];
  size_t argc;
  int active; /* 1 => # 0 => ## */
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
    int trace;
    int locked;
    int builtin;
    size_t minargs;
    size_t maxargs;
    int novalue; /* must always return no value */
    size_t residual;
    size_t maxsegmark; /* highest segment mark number
                                in use in this string */
    TTMFCN fcn; /* builtin == 1 */
    utf8* body; /* builtin == 0 */
};

/**
Character Classes  and the Charclass table
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

/**************************************************/
/* Forward */

static TTM* newTTM(long,long,long);
static void freeTTM(TTM*);
static Buffer* newBuffer(TTM*, size_t buffersize);
static void freeBuffer(TTM*, Buffer* bb);
static void expandBuffer(TTM*, Buffer* bb, size_t len);
static void resetBuffer(TTM*, Buffer* bb);
static void setBufferLength(TTM*, Buffer* bb, size_t len);
static void clearHashEntry(struct HashEntry* entry);
static Frame* pushFrame(TTM*);
static void popFrame(TTM*);
static void clearFrame(TTM* ttm, Frame* frame);
static void freeFramestack(TTM* ttm, Frame* stack, size_t stacksize);
static Function* newFunction(TTM*);
static void freeFunction(TTM*, Function* f);
static void clearDictionary(TTM*,struct HashTable* dict);
static int dictionaryInsert(TTM*, Function* fcn);
static Function* dictionaryLookup(TTM*, utf8* name);
static Function* dictionaryRemove(TTM*, utf8* name);
static Charclass* newCharclass(TTM*);
static void freeCharclass(TTM*, Charclass* cl);
static void clearCharclasses(TTM*,struct HashTable* classes);
static int charclassInsert(TTM*, Charclass* cl);
static Charclass* charclassLookup(TTM*, utf8* name);
static Charclass* charclassRemove(TTM*, utf8* name);
static int charclassMatch(utf8* cp, utf8* charclass);
static void scan(TTM*);
static void exec(TTM*, Buffer* bb);
static void parsecall(TTM*, Frame*);
static void call(TTM*, Frame*, utf8* body);
static void printstring(TTM*, FILE* output, utf8* s8);
static void ttm_ap(TTM*, Frame*);
static void ttm_cf(TTM*, Frame*);
static void ttm_cr(TTM*, Frame*);
static void ttm_ds(TTM*, Frame*);
static void ttm_es(TTM*, Frame*);
static int ttm_ss0(TTM*, Frame*);
static void ttm_sc(TTM*, Frame*);
static void ttm_ss(TTM*, Frame*);
static void ttm_cc(TTM*, Frame*);
static void ttm_cn(TTM*, Frame*);
static void ttm_cp(TTM*, Frame*);
static void ttm_cs(TTM*, Frame*);
static void ttm_isc(TTM*, Frame*);
static void ttm_rrp(TTM*, Frame*);
static void ttm_scn(TTM*, Frame*);
static void ttm_sn(TTM*, Frame*);
static void ttm_eos(TTM*, Frame*);
static void ttm_gn(TTM*, Frame*);
static void ttm_zlc(TTM*, Frame*);
static void ttm_zlcp(TTM*, Frame*);
static void ttm_flip(TTM*, Frame*);
static void ttm_ccl(TTM*, Frame*);
static void ttm_dcl0(TTM*, Frame*, int negative);
static void ttm_dcl(TTM*, Frame*);
static void ttm_dncl(TTM*, Frame*);
static void ttm_ecl(TTM*, Frame*);
static void ttm_scl(TTM*, Frame*);
static void ttm_tcl(TTM*, Frame*);
static void ttm_abs(TTM*, Frame*);
static void ttm_ad(TTM*, Frame*);
static void ttm_dv(TTM*, Frame*);
static void ttm_dvr(TTM*, Frame*);
static void ttm_mu(TTM*, Frame*);
static void ttm_su(TTM*, Frame*);
static void ttm_eq(TTM*, Frame*);
static void ttm_gt(TTM*, Frame*);
static void ttm_lt(TTM*, Frame*);
static void ttm_eql(TTM*, Frame*);
static void ttm_gtl(TTM*, Frame*);
static void ttm_ltl(TTM*, Frame*);
static void ttm_ps(TTM*, Frame*);
static void ttm_psr(TTM*, Frame*);
static void ttm_rs(TTM*, Frame*);
static void ttm_pf(TTM*, Frame*);
static void ttm_cm(TTM*, Frame*);
static void ttm_classes(TTM*, Frame*);
static void ttm_names(TTM*, Frame*);
static void ttm_exit(TTM*, Frame*);
static void ttm_ndf(TTM*, Frame*);
static void ttm_norm(TTM*, Frame*);
static void ttm_time(TTM*, Frame*);
static void ttm_xtime(TTM*, Frame*);
static void ttm_ctime(TTM*, Frame*);
static void ttm_tf(TTM*, Frame*);
static void ttm_tn(TTM*, Frame*);
static void ttm_argv(TTM*, Frame*);
static void ttm_argc(TTM*, Frame*);
static void ttm_include(TTM*, Frame*);
static void ttm_lf(TTM*, Frame*);
static void ttm_uf(TTM*, Frame*);
static void fail(TTM*, TTMERR eno);
static void fatal(TTM*, const char* msg);
static const char* errstring(TTMERR err);
static int int2string(utf8* dst, long long n);
static TTMERR toInt64(utf8* s, long long* lp);
static utf8cp convertEscapeChar(utf8* cp);
static void trace(TTM*, int entering, int tracing);
static void trace1(TTM*, int depth, int entering, int tracing);
static void traceframe(TTM* ttm, Frame* frame, int traceargs);
static void dumpstack(TTM*);
#ifdef DEBUG
static void dbgprint(utf8* s, char quote);
static void dbgprintc(utf8* c, char quote);
static const char* dbgsframe(Frame* frame);
#endif
static int getOptionNameLength(char** list);
static int pushOptionName(char* option, size_t max, char** list);
static void initglobals();
static void usage(const char*);
static void readinput(TTM*, const char* filename,Buffer* bb);
static int readbalanced(TTM*);
static void printbuffer(TTM*);
static int readfile(TTM*, FILE* file, Buffer* bb);

/* Read/Write Management to convert external data to/from utf8 codepoints */
static void fputc8(utf8* c, FILE* f);
static utf8cp fgetc8(FILE* f);

/* UTF8 <-> char_t management */
static int u8size(utf8* cp); /* figure out no. bytes in a codepoint */
static const utf8cp ascii2u8(char c);
static int u8equal(utf8* c1, utf8* c2);
static utf8cp u8cp(utfcpa c8);
static void u8cpa(utfcpa c8a,utf8cp c8);
static int memcpycp(utf8cpa dst, utf8cpa src);
static int copycpincr(utf8cpa* dstp, utf8cpa* srcp);
static utf8* u8ith(utf8* p, size_t n);
static utf8* u8incr(utf8** p0);
static utf8* u8decr(utf8* p0);

static size_t strlcatx(char* dst, const char* src, size_t size);

#if 0
static int streq8ascii(utf8* s32, char_t* s8);
static int toChar8(char_t* dst, utf32 codepoint);
static int toString8(char_t* dst, utf8* src, int srclen, int dstlen);
static int toString32(utf8* dst, char_t* src, int len);
#endif

/**************************************************/
/* Global variables */

static char* eoptions[MAXEOPTIONS+1]; /* null terminated */
static char* argoptions[MAXARGS+1]; /* null terminated */

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
computehash(utf8* name)
{
    unsigned hash;
    utf8* p;
    for(hash=0,p=name;*p!=NUL8;p++) hash = hash + (*p <<1);
    if(hash==0) hash=1;
    return hash;        
}

/* Locate a named entry in the hashtable;
   return 1 if found; 0 otherwise.
   prev is a pointer to HashEntry "before" the found entry.
*/

static int
hashLocate(struct HashTable* table, utf8* name, struct HashEntry** prevp)
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
	   && strcmp((char_t*)name,(char_t*)next->name)==0)
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

/**************************************************/
/* Provide subtype specific wrappers for the HashTable operations. */

static Function*
dictionaryLookup(TTM* ttm, utf8* name)
{
    struct HashTable* table = &ttm->dictionary;
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
dictionaryRemove(TTM* ttm, utf8* name)
{
    struct HashTable* table = &ttm->dictionary;
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
    struct HashTable* table = &ttm->dictionary;
    struct HashEntry* prev;    

    if(hashLocate(table,fcn->entry.name,&prev)) return 0;
    /* Does not already exist */
    fcn->entry.hash = computehash(fcn->entry.name);/*make sure*/
    hashInsert(table,prev,(struct HashEntry*)fcn);
    return 1;
}

static Charclass*
charclassLookup(TTM* ttm, utf8* name)
{
    struct HashTable* table = &ttm->charclasses;
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
charclassRemove(TTM* ttm, utf8* name)
{
    struct HashTable* table = &ttm->charclasses;
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
    struct HashTable* table = &ttm->charclasses;
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
newTTM(long buffersize, long stacksize, long execcount)
{
    TTM* ttm = (TTM*)calloc(1,sizeof(TTM));
    if(ttm == NULL) return NULL;
    ttm->limits.buffersize = buffersize;
    ttm->limits.stacksize = stacksize;
    ttm->limits.execcount = execcount;
    ttm->sharpc = ascii2u8('#');
    ttm->openc = ascii2u8('<');
    ttm->closec = ascii2u8('>');
    ttm->semic = ascii2u8(';');
    ttm->escapec = ascii2u8('\\');
    ttm->metac = ascii2u8('\n');
    ttm->buffer = newBuffer(ttm,buffersize);
    ttm->result = newBuffer(ttm,buffersize);
    ttm->stacknext = 0;
    ttm->stack = (Frame*)calloc(sizeof(Frame),stacksize);
    if(ttm->stack == NULL) fail(ttm,TTM_EMEMORY);
    memset((void*)&ttm->dictionary,0,sizeof(ttm->dictionary));
    memset((void*)&ttm->charclasses,0,sizeof(ttm->charclasses));
#ifdef DEBUG
    ttm->flags |= FLAG_TRACE;
#endif
    return ttm;
}

static void
freeTTM(TTM* ttm)
{
    freeBuffer(ttm,ttm->buffer);
    freeBuffer(ttm,ttm->result);
    freeFramestack(ttm,ttm->stack,ttm->stacknext);
    clearDictionary(ttm,&ttm->dictionary);
    clearCharclasses(ttm,&ttm->charclasses);
    free(ttm);
}

/**************************************************/

static Buffer*
newBuffer(TTM* ttm, unsigned buffersize)
{
    Buffer* bb;
    bb = (Buffer*)calloc(1,sizeof(Buffer));
    if(bb == NULL) fail(ttm,TTM_EMEMORY);
    bb->content = (utf8*)calloc(buffersize,sizeof(utf8));
    if(bb->content == NULL) fail(ttm,TTM_EMEMORY);
    bb->alloc = buffersize;
    bb->length = 0;
    bb->active = bb->content;
    bb->passive = bb->content;
    bb->end = bb->active;
    return bb;
}

static void
freeBuffer(TTM* ttm, Buffer* bb)
{
    nullfree(bb->content);
    free(bb);
}

/* Make room for a string of length n at current active position. */
static void
expandBuffer(TTM* ttm, Buffer* bb, unsigned len)
{
    assert(bb != NULL);
    if((bb->alloc - bb->length) < len) fail(ttm,TTM_EBUFFERSIZE);
    if(bb->active < bb->end) {
        /* make room for len characters by moving bb->active and up*/
        unsigned tomove = (bb->end - bb->active);
        memmovex(bb->active+len,bb->active,tomove);
    }
    bb->active += len;
    bb->length += len;
    bb->end = bb->content+bb->length;
    *(bb->end) = NUL8;
}

#if 0
/* Remove len characters at current position */
static void
compressBuffer(TTM* ttm, Buffer* bb, unsigned len)
{
    assert(bb != NULL);
    if(len > 0 && bb->active < bb->end) {
        memcpy32(bb->active,bb->active+len,len);        
    }    
    bb->length -= len;
    bb->end = bb->content+bb->length;
    *(bb->end) = NUL;
}
#endif

/* Reset buffer and set content */
static void
resetBuffer(TTM* ttm, Buffer* bb)
{
    assert(bb != NULL);
    bb->length = 0;
    bb->active = bb->content;
    bb->end = bb->content;
    *bb->end = NUL8;
}

/* Change the buffer length without disturbing
   any existing content (unless shortening)
   If space is added, its content is undefined.
*/
static void
setBufferLength(TTM* ttm, Buffer* bb, unsigned len)
{
    if(len >= bb->alloc) fail(ttm,TTM_EBUFFERSIZE);
    bb->length = len;
    bb->end = bb->content+bb->length;
    *(bb->end) = NUL8; /* make sure */    
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
    if(ttm->stacknext >= ttm->limits.stacksize)
        fail(ttm,TTM_ESTACKOVERFLOW);
    frame = &ttm->stack[ttm->stacknext];
    frame->argc = 0;
    frame->active = 0;
    ttm->stacknext++;
    return frame;
}

static void
popFrame(TTM* ttm)
{
    Frame* frame;
    if(ttm->stacknext == 0)
        fail(ttm,TTM_ESTACKUNDERFLOW);
    ttm->stacknext--;
    if(ttm->stacknext == 0)
        frame = NULL;
    else
        frame = &ttm->stack[ttm->stacknext-1];    
    clearFrame(ttm,frame);
}

static void
clearFrame(TTM* ttm, Frame* frame)
{
    if(frame == NULL) return;
    /* Do not reclaim the argv because it points into ttm->buffer */
}

static void
freeFramestack(TTM* ttm, Frame* stack, unsigned stacksize)
{
    unsigned i;
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
    if(f == NULL) fail(ttm,TTM_EMEMORY);
    return f;
}

static void
freeFunction(TTM* ttm, Function* f)
{
    assert(f != NULL);
    if(!f->builtin) nullfree(f->body);
    clearHashEntry(&f->entry);
    free(f);
}

static void
clearDictionary(TTM* ttm, struct HashTable* dict)
{
    unsigned i;
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
    if(cl == NULL) fail(ttm,TTM_EMEMORY);
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
    unsigned i;
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
    for(p=charclass;*p;u8incr(&p) {if(u8equal(cp,p)) return 1;
    return 0;
}

/**************************************************/

/**
This is basic top level scanner.
*/
static void
scan(TTM* ttm)
{
    Buffer* bb = ttm->buffer;
    int ncp;

    for(;;) {
        c = u8cp(bb->active); /* NOTE that we do not bump here */
        if(isnul(bb->active)) { /* End of buffer */
            break;
        } else if(isescape(bb->active)) {
            u8incr(&bb->active); /* skip the escape */
	    copycpincr(&bb->passive,&bb->active);
        } else if(u8equal(bb->active,ttm->sharpc) {/* Start of call? */
            if(u8equal(u8ith(bb->active,1),ttm->openc)
               || (u8equal(u8ith(bb->active,1),ttm->sharpc)
                    && u8equal(u8ith(bb->active,2),ttm->openc))) {
                /* It is a real call */
                exec(ttm,bb);
                if(ttm->flags & FLAG_EXIT) goto exiting;
            } else {/* not an call; just pass the '#' along passively */
                copycpincr(&bb->passive,&bb->active);
            }
        } else if(u8equal(bb->active,ttm->openc) { /* Start of <...> escaping */
            /* skip the leading lbracket */
            int depth = 1;
            u8incr(&bb->active);
            for(;;) {
                if(isnul(bb->active)) fail(ttm,TTM_EEOS); /* Unexpected EOF */
                copycpincr(&bb->passive,&bb->active);
                if(isescape(bb->active)) {
                    copycpincr(&bb->passive,&bb->active);
                } else if(u8equal(bb->active,ttm->openc)) {
                    depth++;
                } else if(u8equal(bb->active,ttm->closec)) {
                    if(--depth == 0) {bb->passive = u8decr(bb->passive); break;} /* we are done */
                } /* else keep moving */
            }/*<...> for*/
        } else { /* non-signficant character */
            copycpincr(&bb->passive,&bb->active);
        }
    } /*scan for*/

    /* When we get here, we are finished, so clean up */
    { 
        unsigned newlen;
        /* reset the buffer length using bb->passive.*/
        newlen = bb->passive - bb->content;
        setBufferLength(ttm,bb,newlen);
        /* reset bb->active */
        bb->active = bb->passive;
    }
exiting:
    return;
}

static void
exec(TTM* ttm, Buffer* bb)
{
    Frame* frame;
    Function* fcn;
    utf8* savepassive;

    if(ttm->limits.execcount-- <= 0)
	fail(ttm,TTM_EEXECCOUNT);	
    frame = pushFrame(ttm);
    /* Skip to the start of the function name */
    if(u8equal(u8ith(bb->active,1),ttm->openc)) {
        bb->active = u8ith(bb->active,2);
        frame->active = 1;
    } else {
        bb->active = u8ith(bb->active,3);
        frame->active = 0;
    }
    /* Parse and store relevant pointers into frame. */
    savepassive = bb->passive;
    parsecall(ttm,frame);
    bb->passive = savepassive;
    if(ttm->flags & FLAG_EXIT) goto exiting;

    /* Now execute this function, which will leave result in bb->result */
    if(frame->argc == 0) fail(ttm,TTM_ENONAME);
    if(strlen(frame->argv[0])==0) fail(ttm,TTM_ENONAME);
    /* Locate the function to execute */
    fcn = dictionaryLookup(ttm,frame->argv[0]);
    if(fcn == NULL) fail(ttm,TTM_ENONAME);
    if(fcn->minargs > (frame->argc - 1)) /* -1 to account for function name*/
        fail(ttm,TTM_EFEWPARMS);
    /* Reset the result buffer */
    resetBuffer(ttm,ttm->result);
    if(ttm->flags & FLAG_TRACE || fcn->trace)
        trace(ttm,1,TRACING);
    if(fcn->builtin) {
        fcn->fcn(ttm,frame);
        if(fcn->novalue) resetBuffer(ttm,ttm->result);
        if(ttm->flags & FLAG_EXIT) goto exiting;
    } else /* invoke the pseudo function "call" */
        call(ttm,frame,fcn->body);

#ifdef DEBUG
fprintf(stderr,"result: ");
dbgprint(ttm->result->content,'|');
fprintf(stderr,"\n");
#endif

    if(ttm->flags & FLAG_TRACE || fcn->trace)
        trace(ttm,0,TRACING);

    /* Now, put the result into the buffer */
    if(!fcn->novalue && ttm->result->length > 0) {
        utf8* insertpos;
        unsigned resultlen = ttm->result->length;
        /*Compute the space avail between bb->passive and bb->active */
        unsigned avail = (bb->active - bb->passive); 
        /* Compute amount we need to expand, if any */
        if(avail < resultlen)
            expandBuffer(ttm,bb,(resultlen - avail));/*will change bb->active*/
        /* We insert the result as follows:
           frame->passive => insert at bb->passive (and move bb->passive)
           frame->active => insert at bb->active - (ttm->result->length)
        */
        if(frame->active) {
            insertpos = (bb->active - resultlen);
            bb->active = insertpos;         
        } else { /*frame->passive*/
            insertpos = bb->passive;
            bb->passive += resultlen;
        }
        memcpy((void*)insertpos,ttm->result->content,ttm->result->length);
#ifdef DEBUG
fprintf(stderr,"context:\n\tpassive=|");
/* Since passive is not normally null terminated, we need to fake it */
  {int i; utf8* p;
    for(p=ttm->buffer->passive,i=0;i<PASSIVEMAX && *p != NUL8;i++,u8incr(&p))
      dbgprint32c(u8cp(p),'|');
    fprintf(stderr,"...|\n");
    fprintf(stderr,"\tactive=|");
    for(p=ttm->buffer->active,i=0;i<ACTIVEMAX && *p != NUL8;i++,u8incr(&p))
      dbgprint32c(u8cp(p),'|');
    fprintf(stderr,"...|\n");
  }
#endif

    }
exiting:
    popFrame(ttm);
}

/**
Construct a frame; leave bb->active pointing just
past the call.
*/
static void
parsecall(TTM* ttm, Frame* frame)
{
    int done,depth;
    Buffer* bb = ttm->buffer;

    done = 0;
    do {
        utf8* arg = bb->passive;/* start of ith argument */
        while(!done) {
            if(isnul(bb->active)) fail(ttm,TTM_EEOS); /* Unexpected end of buffer */
            if(isescape(bb->active)) {
                copycpincr(&bb->passive,&bb->active);
            } else if(u8equal(bb->active,ttm->semic) || u8equal(bb->active,ttm->closec)) {
                /* End of an argument */
                *bb->passive++ = NUL8; /* null terminate the argument */
#ifdef DEBUG
fprintf(stderr,"parsecall: argv[%d]=",frame->argc);
dbgprint(arg,'|');
fprintf(stderr,"\n");
#endif
                u8incr(&bb->active); /* skip the semi or close */
                /* move to next arg */
                frame->argv[frame->argc++] = arg;
                if(u8equal(bb->active,ttm->closec)) done=1;
                else if(frame->argc >= MAXARGS) fail(ttm,TTM_EMANYPARMS);
                else arg = bb->passive;
            } else if(u8equal(bb->active,ttm->sharpc)) {
                /* check for call within call */
                if(u8equal(u8ith(bb->active,1)m,ttm->openc)
                   || (u8equal(u8ith(bb->active,1),ttm->sharpc)
                        && u8equal(u8ith(bb->active,2),ttm->openc))) {
                    /* Recurse to compute inner call */
                    exec(ttm,bb);
                    if(ttm->flags & FLAG_EXIT) goto exiting;
                }
            } else if(u8equal(bb->active,ttm->openc)) {/* <...> nested brackets */
                u8incr(&bb->active); /* skip leading lbracket */
                depth = 1;
                for(;;) {
                    if(isnul(bb->active)) fail(ttm,TTM_EEOS); /* Unexpected EOF */
                    if(u8equal(bb->active,ttm->escapec)) {
                        copycpincr(&bb->passive,&bb->active);
                        copycpincr(&bb->passive,&bb->active);
                    } else if(u8equal(bb->active,ttm->openc)) {
                        copycpincr(&bb->passive,&bb->active);
			u8incr(&bb->active);
                        depth++;
                    } else if(u8equal(bb->active,ttm->closec)) {
                        depth--;
                        u8incr(&bb->active);
                        copycpincr(&bb->passive,&bb->active);
                        if(depth == 0) break; /* we are done */
                    } else {
                        copycpincr(&bb->passive,&bb->active);
                    }
                }/*<...> for*/
            } else {
                /* keep moving */
                copycpincr(&bb->passive,&bb->active);
            }
        } /* collect argument for */
    } while(!done);
exiting:
    return;
}

/**************************************************/
/**
Execute a non-builtin function
*/

static void
call(TTM* ttm, Frame* frame, utf8* body)
{
    utf8* p;
    unsigned len;
    utf8* result;
    utf8* dst;
    char crformat[16];
    char crval[CREATELEN+1];
    int crlen;

    crformat[0] = NUL;

    /* Compute the size of the output */
    for(len=0,p=body;p+=count) {
	int count = u8size(*p);
        if(issegmark(p)) { /* Includes create mark */
            size_t segindex = (size_t)(p[1] & SEGMARKMASK);
            if((segindex != CREATEINDEX && segindex < frame->argc)
                len += strlen(frame->argv[segindex]);
            /* else treat as empty string */
        } else
            len += count;
    }
    /* Compute the body using ttm->result  */
    resetBuffer(ttm,ttm->result);
    setBufferLength(ttm,ttm->result,len);
    result = ttm->result->content;
    dst = result;
    dst[0] = NUL8; /* so we can use strcat */
    for(p=body;;) {
        if(issegmark(p)) {
            size_t segindex = (size_t)(p[1] & SEGMARKMASK);
	    if(segindex == CREATEINDEX) {
		if(strlen(crformat) == 0) {
                    /* Construct the format string on the fly */
                    snprintf(crformat,sizeof(crformat),"%%0%dd",CREATELEN);
                    /* Construct the cr value */
                    ttm->crcounter++;
                    snprintf(crval,sizeof(crval),crformat,ttm->crcounter);
                }
		crlen = strlen(crval);
		dst += crlen;
	    } else if((segindex < frame->argc) {
                utf8* arg = frame->argv[segindex];
                strcpy32(dst,arg);
                dst += strlen(arg);
            } /* else treat as null string */
        } else
            copycpincr(&dst,&p);
    }
    *dst = NUL8;
    setBufferLength(ttm,ttm->result,(dst-result));
}

/**************************************************/
/* Built-in Support Procedures */
static void
printstring(TTM* ttm, FILE* output, utf8* s8)
{
    int slen = strlen(s8);
    utfcpu u;
    u.cp = empty_cp;

    if(slen == 0) return;
    while(*s8) {
	u.cp = u8cp(s8);
        if(isescape(s8)) {
	    s8 += u8size(*s8);
	    u.cp = convertEscapeChar(*s8)
	    s8 += u8size(u.cpa[0]);
        }
        if(u.cp != 0) {
            if(issegmark(u.cpa)) {
                char_t* p;
                char_t info[16];
		size_t segindex = (size_t)(u.cpa[1] & SEGMARKMASK);
                if(iscreateindex(segindex))
		    snprintf(info,sizeof(info),"^FF");
                else /* segmark */
		    snprintf(info,sizeof(info),"^%02x",(unsigned)segindex)
		fputs(info,output);
            } else /* some utf8 codepoint */
		for(i=0;i<u8size(u.cpa[0]);i++) fputc(u.cpa[i],output);
         }
    }
    fflush(output);
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
    Function* str = dictionaryLookup(ttm,frame->argv[1]);
    unsigned aplen, bodylen, totallen;

    if(str == NULL) {/* Define the string */
        ttm_ds(ttm,frame);
        return;
    }
    if(str->builtin) fail(ttm,TTM_ENOPRIM);
    apstring = frame->argv[2];
    aplen = strlen(apstring);
    body = str->body;
    bodylen = strlen(body);
    totallen = bodylen + aplen;
    /* Fake realloc because windows realloc is flawed */
    newbody = calloc(sizeof(utf8),(totallen+1));
    if(newbody == NULL) fail(ttm,TTM_EMEMORY);
    if(bodylen > 0) {
        memcpy(newbody,body,bodylen);
        newbody[bodylen] = NUL8;
    }
    strlcat(newbody,apstring,totallen+1);
    str->residual = totallen;
    nullfree(str->body);
    str->body = newbody; newbody = NULL;
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

    if(oldfcn == NULL) fail(ttm,TTM_ENONAME);
    if(newfcn == NULL) {
        /* create a new string object */
        newfcn = newFunction(ttm);
	assert(newfcn->entry.name == NULL);
        newfcn->entry.name = strdup(newname);
        dictionaryInsert(ttm,newfcn);
    }
    saveentry = newfcn->entry;
    *newfcn = *oldfcn;
    /* Keep new hash entry */
    newfcn->entry = saveentry;
    /* Do pointer fixup */
    if(newfcn->body != NULL)
        newfcn->body = strdup(newfcn->body);
}

static void
ttm_cr(TTM* ttm, Frame* frame) /* Mark for creation */
{
    Function* str;
    size_t crlen;
    utf8* body;
    utf8* crstring;

    str = dictionaryLookup(ttm,frame->argv[1]);
    if(str == NULL) fail(ttm,TTM_ENONAME);
    if(str->builtin) fail(ttm,TTM_ENOPRIM);

    body = str->body;
    crstring = frame->argv[2];
    crlen = strlen(crstring);

    if(crlen > 0) {/* search only if possible success */
        utf8* p;
        /* Search for occurrences of arg starting at residual */
        p = body + str->residual;
        while(*p) {
            if(strncmp(p,crstring,crlen) != 0)
                {p++; continue;}
            /* we have a match, replace match by a create marker */
            *p = CREATE;
            /* compress out all but 1 char of crstring match */
            if(crlen > 1)
                strcpy32(p+1,p+crlen);
        }
    }
}

static void
ttm_ds(TTM* ttm, Frame* frame)
{
    Function* str = NULL;
    str = dictionaryLookup(ttm,frame->argv[1]);
    if(str == NULL) {
        /* create a new string object */
        str = newFunction(ttm);
	assert(str->entry.name == NULL);
        str->entry.name = strdup32(frame->argv[1]);
        dictionaryInsert(ttm,str);
    } else {
        /* reset as needed */
        str->builtin = 0;
        str->minargs = 0;
        str->maxargs = 0;
        str->residual = 0;
        str->maxsegmark = 0;
        str->fcn = NULL;
	nullfree(str->body);
    }
    nullfree(str->body);
    str->body = strdup32(frame->argv[2]);
}

static void
ttm_es(TTM* ttm, Frame* frame) /* Erase string */
{
    unsigned i;
    for(i=1;i<frame->argc;i++) {
        utf8* strname = frame->argv[i];
        Function* str = dictionaryLookup(ttm,strname);
        if(str != NULL && !str->locked) {
            dictionaryRemove(ttm,strname);
            freeFunction(ttm,str); /* reclaim the string */
        }
    }
}

/* Helper function for #<sc> and #<ss> */
static int
ttm_ss0(TTM* ttm, Frame* frame)
{
    Function* str;
    unsigned i,segcount,startseg,bodylen;
    Buffer* result = ttm->result; /* store accumulated segmented string */

    str = dictionaryLookup(ttm,frame->argv[1]);
    if(str == NULL)
        fail(ttm,TTM_ENONAME);
    if(str->builtin)
        fail(ttm,TTM_ENOPRIM);

    resetBuffer(ttm,result);

    bodylen = strlen32(str->body);
    if(str->residual >= bodylen)
        return 0; /* no substitution possible */
    segcount = 0;
    startseg = str->maxsegmark;
    for(i=2;i<frame->argc;i++) {
        utf8* arg = frame->argv[i];
        unsigned arglen = strlen32(arg);
        if(arglen > 0) { /* search only if possible success */
            int found;
            utf8* p;
            /* Search for occurrences of arg */
            p = str->body + str->residual;
            found = 0;
            while(*p) {
                if(strncmp32(p,arg,arglen) != 0)
                    {p++; continue;}
                /* we have a match, replace match by a segment marker */
                if(!found) {/* first match */
                    startseg++;
                    found++;
                }
                *p = (SEGMARK | startseg);
                /* compress out all but 1 char of match */
                if(arglen > 1)
                    strcpy32(p+1,p+arglen);
                segcount++;
            }
        }
    }
    str->maxsegmark = startseg;
    return segcount;
}

static void
ttm_sc(TTM* ttm, Frame* frame) /* Segment and count */
{
    char count[64];
    int n,nsegs = ttm_ss0(ttm,frame);
    snprintf(count,sizeof(count),"%d",nsegs);
    /* Insert into ttm->result */
    setBufferLength(ttm,ttm->result,strlen(count));
    n = toString32(ttm->result->content,count,TOEOS);
    setBufferLength(ttm,ttm->result,n);
}

static void
ttm_ss(TTM* ttm, Frame* frame) /* Segment and count */
{
    (void)ttm_ss0(ttm,frame);
}

/* String Selection */

static void
ttm_cc(TTM* ttm, Frame* frame) /* Call one character */
{
    Function* fcn;

    fcn = dictionaryLookup(ttm,frame->argv[1]);
    if(fcn == NULL)
        fail(ttm,TTM_ENONAME);
    if(fcn->builtin)
        fail(ttm,TTM_ENOPRIM);
    /* Check for pointing at trailing NUL */
    if(fcn->residual < strlen32(fcn->body)) {
        utf32 c32 = *(fcn->body+fcn->residual);
        *ttm->result->content = c32;
        setBufferLength(ttm,ttm->result,1);
        fcn->residual++;
    }
}

static void
ttm_cn(TTM* ttm, Frame* frame) /* Call n characters */
{
    Function* fcn;
    long long ln;
    unsigned n;
    TTMERR err;
    unsigned bodylen,startn;
    unsigned avail;

    fcn = dictionaryLookup(ttm,frame->argv[2]);
    if(fcn == NULL)
        fail(ttm,TTM_ENONAME);
    if(fcn->builtin)
        fail(ttm,TTM_ENOPRIM);

    /* Get number of characters to extract */
    err = toInt64(frame->argv[1],&ln);
    if(err != TTM_ENOERR) fail(ttm,err);
    if(ln < 0) fail(ttm,TTM_ENOTNEGATIVE);   

    n = (unsigned )ln;

    /* See if we have enough space */
    bodylen = strlen32(fcn->body);
    if(fcn->residual >= bodylen)
	avail = 0;
    else
        avail = (bodylen - fcn->residual);
   
    if(n == 0 || avail == 0) goto nullreturn;
    if(avail < n) n = avail; /* return what is available */

    /* We want n characters starting at residual */
    startn = fcn->residual;
        
    /* ok, copy n characters from startn into the return buffer */
    setBufferLength(ttm,ttm->result,n);
    strncpy32(ttm->result->content,fcn->body+startn,n);
    /* increment residual */
    fcn->residual += n;
    return;

nullreturn:
    setBufferLength(ttm,ttm->result,0);
    ttm->result->content[0] = NUL;
    return;
}

static void
ttm_cp(TTM* ttm, Frame* frame) /* Call parameter */
{
    Function* fcn;
    unsigned delta;
    utf8* rp;
    utf8* rp0;
    utf32 c32;
    int depth;

    fcn = dictionaryLookup(ttm,frame->argv[1]);
    if(fcn == NULL)
        fail(ttm,TTM_ENONAME);
    if(fcn->builtin)
        fail(ttm,TTM_ENOPRIM);

    rp0 = (fcn->body + fcn->residual);
    rp = rp0;
    depth = 0;
    ttm->result->content[0] = NUL8; /* so we can strcat */
    for(;(c32=*rp);rp++) {
        if(c32 == ttm->semic) {
            if(depth == 0) break; /* reached unnested semicolon*/
        } else if(c32 == ttm->openc) {
            depth++;
        } else if(c32 == ttm->closec) {
            depth--;
        }
    }
    delta = (rp - rp0);
    setBufferLength(ttm,ttm->result,delta);
    strncpy32(ttm->result->content,rp0,delta);
    fcn->residual += delta;
    if(c32 != NUL8) fcn->residual++;
}

static void
ttm_cs(TTM* ttm, Frame* frame) /* Call segment */
{
    Function* fcn;
    utf32 c32;
    utf8* p;
    utf8* p0;
    unsigned delta;

    fcn = dictionaryLookup(ttm,frame->argv[1]);
    if(fcn == NULL)
        fail(ttm,TTM_ENONAME);
    if(fcn->builtin)
        fail(ttm,TTM_ENOPRIM);

    /* Locate the next segment mark */
    /* Unclear if create marks also qualify; assume yes */
    p0 = fcn->body + fcn->residual;
    p = p0;
    for(;(c32=*p);p++) {
        if(c32 == NUL8 || testMark(c32,SEGMARK) || testMark(c32,CREATE))
            break;
    }
    delta = (p - p0);
    if(delta > 0) {
        setBufferLength(ttm,ttm->result,delta);
        strncpy32(ttm->result->content,p0,delta);
    }
    /* set residual pointer correctly */
    fcn->residual += delta;
    if(c32 != NUL8) fcn->residual++;
}

static void
ttm_isc(TTM* ttm, Frame* frame) /* Initial character scan; moves residual pointer */
{
    Function* str;
    utf8* t;
    utf8* f;
    utf8* result;
    utf8* arg;
    unsigned arglen;
    unsigned slen;

    str = dictionaryLookup(ttm,frame->argv[2]);
    if(str == NULL)
        fail(ttm,TTM_ENONAME);
    if(str->builtin)
        fail(ttm,TTM_ENOPRIM);

    arg = frame->argv[1];
    arglen = strlen32(arg);
    t = frame->argv[3];
    f = frame->argv[4];

    /* check for initial string match */
    if(strncmp32(str->body+str->residual,arg,arglen)==0) {
        result = t;
        str->residual += arglen;
        slen = strlen32(str->body);
        if(str->residual > slen) str->residual = slen;
    } else
        result = f;
    setBufferLength(ttm,ttm->result,strlen32(result));
    strcpy32(ttm->result->content,result);    
}

static void
ttm_rrp(TTM* ttm, Frame* frame) /* Reset residual pointer */
{
    Function* str = dictionaryLookup(ttm,frame->argv[1]);
    if(str == NULL)
        fail(ttm,TTM_ENONAME);
    if(str->builtin)
        fail(ttm,TTM_ENOPRIM);
    str->residual = 0;
}

static void
ttm_scn(TTM* ttm, Frame* frame) /* Character scan */
{
    Function* str;
    unsigned arglen;
    unsigned bodylen;
    utf8* f;
    utf8* result;
    utf8* arg;
    utf8* p;
    utf8* p0;

    str = dictionaryLookup(ttm,frame->argv[2]);
    if(str == NULL)
        fail(ttm,TTM_ENONAME);
    if(str->builtin)
        fail(ttm,TTM_ENOPRIM);

    arg = frame->argv[1];
    arglen = strlen32(arg);
    f = frame->argv[3];

    /* check for sub string match */
    p0 = str->body+str->residual;
    p = p0;
    result = NULL;
    for(;*p;p++) {
        if(strncmp32(p,arg,arglen)==0) {result = p; break;}
    }    
    if(result == NULL) {/* no match; return argv[3] */
        setBufferLength(ttm,ttm->result,strlen32(f));
        strcpy32(ttm->result->content,f);    
    } else {/* return from residual ptr to location of string */
        unsigned len = (p - p0);
        setBufferLength(ttm,ttm->result,len);
        strncpy32(ttm->result->content,p0,len);
	if(len == 0) {/* if the match is at the residual ptr, mv ptr */
	    str->residual += (arglen);
            bodylen = strlen32(str->body);
            if(str->residual > bodylen) str->residual = bodylen;
	}
    }
}

static void
ttm_sn(TTM* ttm, Frame* frame) /* Skip n characters */
{
    TTMERR err;
    long long num;
    Function* str;
    unsigned bodylen;

    str = dictionaryLookup(ttm,frame->argv[2]);
    if(str == NULL)
        fail(ttm,TTM_ENONAME);
    if(str->builtin)
        fail(ttm,TTM_ENOPRIM);

    err = toInt64(frame->argv[1],&num);
    if(err != TTM_ENOERR) fail(ttm,err);
    if(num < 0) fail(ttm,TTM_ENOTNEGATIVE);   

    str->residual += (int)num;
    bodylen = strlen32(str->body);
    if(str->residual > bodylen)
        str->residual = bodylen;
}

static void
ttm_eos(TTM* ttm, Frame* frame) /* Test for end of string */
{
    Function* str;
    unsigned bodylen;
    utf8* t;
    utf8* f;
    utf8* result;

    str = dictionaryLookup(ttm,frame->argv[1]);
    if(str == NULL)
        fail(ttm,TTM_ENONAME);
    if(str->builtin)
        fail(ttm,TTM_ENOPRIM);
    bodylen = strlen32(str->body);
    t = frame->argv[2];
    f = frame->argv[3];
    result = (str->residual >= bodylen ? t : f);
    setBufferLength(ttm,ttm->result,strlen32(result));
    strcpy32(ttm->result->content,result);
}

/* String Scanning Operations */

static void
ttm_gn(TTM* ttm, Frame* frame) /* Give n characters from argument string*/
{
    utf8* snum = frame->argv[1];
    utf8* s = frame->argv[2];
    unsigned slen = strlen32(s);
    TTMERR err;
    long long num;
    utf8* startp;

    err = toInt64(snum,&num);
    if(err != TTM_ENOERR) fail(ttm,err);
    if(num > 0) {
        if(slen < num) num = slen;
        startp = s;
    } else if(num < 0) {
        num = -num;
        startp = s + num;
        num = (slen - num);
    }
    if(num != 0) {
        setBufferLength(ttm,ttm->result,(unsigned )num);
        strncpy32(ttm->result->content,startp,(unsigned )num);
    }
}

static void
ttm_zlc(TTM* ttm, Frame* frame) /* Zero-level commas */
{
    utf8* s;
    utf8* q;
    int slen,c,depth;

    s = frame->argv[1];
    slen = strlen32(s);
    setBufferLength(ttm,ttm->result,slen); /* result will be same length */
    for(depth=0,q=ttm->result->content;(c=*s);s++) {
        if(isescape(c)) {
            *q++ = c;
            *q++ = *s++; /* skip escaped character */
        } else if(c == COMMA && depth == 0) {
            *q++ = ttm->semic;  
        } else if(c == LPAREN) {
            depth++;
            *q++ = c;
        } else if(c == RPAREN) {
            depth--;
            *q++ = c;
        } else {
            *q++ = c;
        }
    }
    *q = NUL8; /* make sure it is terminated */
    setBufferLength(ttm,ttm->result,(q - ttm->result->content));
}

static void
ttm_zlcp(TTM* ttm, Frame* frame) /* Zero-level commas and parentheses;
                                    exact algorithm is unknown */
{
    /* A(B) and A,B will both give A;B and (A),(B),C will give A;B;C */
    utf8* s;
    utf8* p;
    utf8* q;
    utf32 c;
    int slen,depth;

    s = frame->argv[1];
    slen = strlen32(s);
    setBufferLength(ttm,ttm->result,slen); /* result may be shorter; handle below */
    q = ttm->result->content;
    p = s;
    for(depth=0;(c=*p);p++) {
        if(isescape(c)) {
            *q++ = c;
            p++;
            *q++ = *p;
        } else if(depth == 0 && c == COMMA) {
            if(p[1] != LPAREN) {*q++ = ttm->semic;}
        } else if(c == LPAREN) {
            if(depth == 0 && p > s) {*q++ = ttm->semic;}
            if(depth > 0) *q++ = c;
            depth++;
        } else if(c == RPAREN) {
            depth--;
            if(depth == 0 && p[1] == COMMA) {
            } else if(depth == 0 && p[1] == NUL8) {/* do nothing */
            } else if(depth == 0) {
                *q++ = ttm->semic;
            } else {/* depth > 0 */
                *q++ = c;
            }
        } else {
            *q++ = c;      
        }
    }
    *q = NUL8; /* make sure it is terminated */
    setBufferLength(ttm,ttm->result,(q-ttm->result->content));
}

static void
ttm_flip(TTM* ttm, Frame* frame) /* Flip a string */
{
    utf8* s;
    utf8* q;
    utf8* p;
    int slen,i;

    s = frame->argv[1];
    slen = strlen32(s);
    setBufferLength(ttm,ttm->result,slen);
    p = s + slen;
    q=ttm->result->content;
    for(i=0;i<slen;i++) {*q++ = *(--p);}
    *q = NUL8;
}

static void
ttm_ccl(TTM* ttm, Frame* frame) /* Call class */
{
    Charclass* cl = charclassLookup(ttm,frame->argv[1]);
    Function* str = dictionaryLookup(ttm,frame->argv[2]);
    utf32 c;
    utf8* p;
    utf8* start;
    int len;

    if(cl == NULL || str == NULL)
        fail(ttm,TTM_ENONAME);
    if(str->builtin)
        fail(ttm,TTM_ENOPRIM);

    /* Starting at str->residual, locate first char not in class */
    start = str->body+str->residual;
    for(p=start;(c=*p);p++) {
        if(cl->negative && charclassMatch(c,cl->characters)) break;
        if(!cl->negative && !charclassMatch(c,cl->characters)) break;
    }
    len = (p - start);
    if(len > 0) {
        setBufferLength(ttm,ttm->result,len);
        strncpy32(ttm->result->content,start,len);
        ttm->result->content[len] = NUL8;
        str->residual += len;
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
        cl->entry.name = strdup32(frame->argv[1]);
        charclassInsert(ttm,cl);
    }
    nullfree(cl->characters);
    cl->characters = strdup32(frame->argv[2]);
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
    unsigned i;
    for(i=1;i<frame->argc;i++) {
        utf8* clname = frame->argv[i];
        Charclass* cl = charclassRemove(ttm,clname);
        if(cl != NULL) {
            freeCharclass(ttm,cl); /* reclaim the character class */
        }
    }
}

static void
ttm_scl(TTM* ttm, Frame* frame) /* Skip class */
{
    Charclass* cl = charclassLookup(ttm,frame->argv[1]);
    Function* str = dictionaryLookup(ttm,frame->argv[2]);
    utf32 c;
    utf8* p;
    utf8* start;
    int len;

    if(cl == NULL || str == NULL)
        fail(ttm,TTM_ENONAME);
    if(str->builtin)
        fail(ttm,TTM_ENOPRIM);

    /* Starting at str->residual, locate first char not in class */
    start = str->body+str->residual;
    for(p=start;(c=*p);p++) {
        if(cl->negative && charclassMatch(c,cl->characters)) break;
        if(!cl->negative && !charclassMatch(c,cl->characters)) break;
    }
    len = (p - start);
    str->residual += len;
}

static void
ttm_tcl(TTM* ttm, Frame* frame) /* Test class */
{
    Charclass* cl = charclassLookup(ttm,frame->argv[1]);
    Function* str = dictionaryLookup(ttm,frame->argv[2]);
    utf8* retval;
    int retlen;
    utf8* t;
    utf8* f;

    if(cl == NULL)
        fail(ttm,TTM_ENONAME);
    if(str->builtin)
        fail(ttm,TTM_ENOPRIM);
    t = frame->argv[3];
    f = frame->argv[4];
    if(str == NULL)
        retval = f;
    else {
        /* see if char at str->residual is in class */
        utf32 c32 = *(str->body + str->residual);
        if(cl->negative && !charclassMatch(c32,cl->characters)) 
            retval = t;
        else if(!cl->negative && charclassMatch(c32,cl->characters))
            retval = t;
        else
            retval = f;
    }
    retlen = strlen32(retval);
    if(retlen > 0) {
        setBufferLength(ttm,ttm->result,retlen);
        strcpy32(ttm->result->content,retval);
    }    
}

/* Arithmetic Operators */

static void
ttm_abs(TTM* ttm, Frame* frame) /* Obtain absolute value */
{
    utf8* slhs;
    long long lhs;
    TTMERR err;
    char result[32];
    int count;

    slhs = frame->argv[1];    
    err = toInt64(slhs,&lhs);
    if(err != TTM_ENOERR) fail(ttm,err);
    if(lhs < 0) lhs = -lhs;
    snprintf(result,sizeof(result),"%lld",lhs);
    setBufferLength(ttm,ttm->result,strlen(result)); /*overkill*/
    count = toString32(ttm->result->content,result,TOEOS);
    setBufferLength(ttm,ttm->result,count);/*fixup*/
}

static void
ttm_ad(TTM* ttm, Frame* frame) /* Add */
{
    utf8* snum;
    long long num;
    long long total;
    TTMERR err;
    unsigned i,count;

    total = 0;
    for(i=1;i<frame->argc;i++) {
        snum = frame->argv[i];    
        err = toInt64(snum,&num);
        if(err != TTM_ENOERR) fail(ttm,err);
        total += num;
    }
    setBufferLength(ttm,ttm->result,MAXINTCHARS);
    count = int2string(ttm->result->content,total);
    setBufferLength(ttm,ttm->result,count); /* fixup */
}

static void
ttm_dv(TTM* ttm, Frame* frame) /* Divide and give quotient */
{
    utf8* slhs;
    utf8* srhs;
    long long lhs,rhs;
    TTMERR err;
    int count;

    slhs = frame->argv[1];    
    srhs = frame->argv[2];

    err = toInt64(slhs,&lhs);
    if(err != TTM_ENOERR) fail(ttm,err);
    err = toInt64(srhs,&rhs);
    if(err != TTM_ENOERR) fail(ttm,err);
    lhs = (lhs / rhs);
    setBufferLength(ttm,ttm->result,MAXINTCHARS);
    count = int2string(ttm->result->content,lhs);
    setBufferLength(ttm,ttm->result,count); /* fixup */
}

static void
ttm_dvr(TTM* ttm, Frame* frame) /* Divide and give remainder */
{
    utf8* slhs;
    utf8* srhs;
    long long lhs,rhs;
    TTMERR err;
    int count;

    slhs = frame->argv[1];    
    srhs = frame->argv[2];

    err = toInt64(slhs,&lhs);
    if(err != TTM_ENOERR) fail(ttm,err);
    err = toInt64(srhs,&rhs);
    if(err != TTM_ENOERR) fail(ttm,err);
    lhs = (lhs % rhs);
    setBufferLength(ttm,ttm->result,MAXINTCHARS);
    count = int2string(ttm->result->content,lhs);
    setBufferLength(ttm,ttm->result,count); /* fixup */
}

static void
ttm_mu(TTM* ttm, Frame* frame) /* Multiply */
{
    utf8* snum;
    long long num;
    long long total;
    TTMERR err;
    unsigned i,count;

    total = 1;
    for(i=1;i<frame->argc;i++) {
        snum = frame->argv[i];    
        err = toInt64(snum,&num);
        if(err != TTM_ENOERR) fail(ttm,err);
        total *= num;
    }
    setBufferLength(ttm,ttm->result,MAXINTCHARS);
    count = int2string(ttm->result->content,total);
    setBufferLength(ttm,ttm->result,count); /* fixup */
}

static void
ttm_su(TTM* ttm, Frame* frame) /* Substract */
{
    utf8* slhs;
    utf8* srhs;
    long long lhs,rhs;
    TTMERR err;
    int count;

    slhs = frame->argv[1];    
    srhs = frame->argv[2];
    err = toInt64(slhs,&lhs);
    if(err != TTM_ENOERR) fail(ttm,err);
    err = toInt64(srhs,&rhs);
    if(err != TTM_ENOERR) fail(ttm,err);
    lhs = (lhs - rhs);
    setBufferLength(ttm,ttm->result,MAXINTCHARS);
    count = int2string(ttm->result->content,lhs);
    setBufferLength(ttm,ttm->result,count); /* fixup */
}

static void
ttm_eq(TTM* ttm, Frame* frame) /* Compare numeric equal */
{
    utf8* slhs;
    utf8* srhs;
    utf8* t;
    utf8* f;
    long long lhs,rhs;
    TTMERR err;
    utf8* result;

    slhs = frame->argv[1];    
    srhs = frame->argv[2];
    t = frame->argv[3];    
    f = frame->argv[4];

    err = toInt64(slhs,&lhs);
    if(err != TTM_ENOERR) fail(ttm,err);
    err = toInt64(srhs,&rhs);
    if(err != TTM_ENOERR) fail(ttm,err);

    result = (lhs == rhs ? t : f);
    setBufferLength(ttm,ttm->result,strlen32(result));
    strcpy32(ttm->result->content,result);
}

static void
ttm_gt(TTM* ttm, Frame* frame) /* Compare numeric greater-than */
{
    utf8* slhs;
    utf8* srhs;
    utf8* t;
    utf8* f;
    long long lhs,rhs;
    TTMERR err;
    utf8* result;

    slhs = frame->argv[1];    
    srhs = frame->argv[2];
    t = frame->argv[3];    
    f = frame->argv[4];

    err = toInt64(slhs,&lhs);
    if(err != TTM_ENOERR) fail(ttm,err);
    err = toInt64(srhs,&rhs);
    if(err != TTM_ENOERR) fail(ttm,err);

    result = (lhs > rhs ? t : f);
    setBufferLength(ttm,ttm->result,strlen32(result));
    strcpy32(ttm->result->content,result);
}

static void
ttm_lt(TTM* ttm, Frame* frame) /* Compare numeric less-than */
{
    utf8* slhs;
    utf8* srhs;
    utf8* t;
    utf8* f;
    long long lhs,rhs;
    TTMERR err;
    utf8* result;

    slhs = frame->argv[1];    
    srhs = frame->argv[2];
    t = frame->argv[3];    
    f = frame->argv[4];

    err = toInt64(slhs,&lhs);
    if(err != TTM_ENOERR) fail(ttm,err);
    err = toInt64(srhs,&rhs);
    if(err != TTM_ENOERR) fail(ttm,err);

    result = (lhs < rhs ? t : f);
    setBufferLength(ttm,ttm->result,strlen32(result));
    strcpy32(ttm->result->content,result);
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

    result = (strcmp32(slhs,srhs) == 0 ? t : f);
    setBufferLength(ttm,ttm->result,strlen32(result));
    strcpy32(ttm->result->content,result);
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

    result = (strcmp32(slhs,srhs) > 0 ? t : f);
    setBufferLength(ttm,ttm->result,strlen32(result));
    strcpy32(ttm->result->content,result);
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

    result = (strcmp32(slhs,srhs) < 0 ? t : f);
    setBufferLength(ttm,ttm->result,strlen32(result));
    strcpy32(ttm->result->content,result);
}

/* Peripheral Input/Output Operations */

/**
In order to a void spoofing,
the string to be output is
modified to remove all control
characters except '\n', and a final
'\n' is forced.
*/
static void
ttm_ps(TTM* ttm, Frame* frame) /* Print a Function/String */
{
    utf8* s = frame->argv[1];
    utf8* stdxx = (frame->argc == 2 ? NULL : frame->argv[2]);
    FILE* target;
    if(stdxx != NULL && streq32ascii(stdxx,"stderr"))
        target=stderr;
    else
        target = stdout;
    printstring(ttm,target,s);
}

static void
ttm_rs(TTM* ttm, Frame* frame) /* Read a Function/String */
{
    int len;
    utf32 c;
    for(len=0;;len++) {
        c=fgetc32(ttm->input);
        if(c == EOF) break;
        if(c == ttm->metac) break;
        setBufferLength(ttm,ttm->result,len+1);
        ttm->result->content[len] = c;
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
    if(strlen32(smeta) > 0) {
        if(smeta[0] > 127) fail(ttm,TTM_EASCII);
        ttm->metac = smeta[0];
    }
}

static void
ttm_pf(TTM* ttm, Frame* frame) /* Flush stdout and/or stderr */
{
    utf8* stdxx = (frame->argc == 1 ? NULL : frame->argv[1]);
    if(stdxx == NULL || streq32ascii(stdxx,"stdout"))
        fflush(stderr);
    if(stdxx == NULL || streq32ascii(stdxx,"stderr"))
        fflush(stderr);
}

/* Library Operations */

static int
stringveccmp32(const void* a, const void* b)
{
    const utf8** sa = (const utf8**)a;
    const utf8** sb = (const utf8**)b;
    return strcmp32(*sa,*sb);
}

static void
ttm_names(TTM* ttm, Frame* frame) /* Obtain all dictionary instance names in sorted order */
{
    int i,nnames,index,allnames;
    utf8** names;
    unsigned len;
    utf8* p;

    allnames = (frame->argc > 1 ? 1 : 0);

    /* First, figure out the number of names and the total size */
    len = 0;
    for(nnames=0,i=0;i<HASHSIZE;i++) {
	struct HashEntry* entry = ttm->dictionary.table[i].next;
        while(entry != NULL) {
	    Function* name = (Function*)entry;
	    if(allnames || !name->builtin) {
		len += strlen32(name->entry.name);
                nnames++;
            }
	    entry = entry->next;
        }
    }

    if(nnames == 0)
        return;

    /* Now collect all the names */
    names = (utf8**)calloc(sizeof(utf8*),nnames);
    if(names == NULL) fail(ttm,TTM_EMEMORY);
    index = 0;
    for(i=0;i<HASHSIZE;i++) {
	struct HashEntry* entry = ttm->dictionary.table[i].next;
        while(entry != NULL) {
	    Function* name = (Function*)entry;
            if(allnames || !name->builtin) {
                names[index++] = name->entry.name;                
            }
            entry = entry->next;
        }
    }
    /* Quick sort using strcmp32 as the comparator */
    qsort((void*)names, nnames, sizeof(char*),stringveccmp32);

    /* Return the set of names separated by commas */    
    setBufferLength(ttm,ttm->result,len+(nnames-1));
    p = ttm->result->content;
    for(i=0;i<nnames;i++) {
        if(i > 0) {*p++ = ',';}
        strcpy32(p,names[i]);
        p += strlen32(names[i]);
    }
    if(nnames > 0) free(names);
}

static void
ttm_exit(TTM* ttm, Frame* frame) /* Return from TTM */
{
    long long exitcode = 0;
    ttm->flags |= FLAG_EXIT;
    if(frame->argc > 1) {
        TTMERR err = toInt64(frame->argv[1],&exitcode);
        if(err != TTM_ENOERR) exitcode = 1;
        else if(exitcode < 0) exitcode = - exitcode;
    }   
    ttm->exitcode = (int)exitcode;
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
    setBufferLength(ttm,ttm->result,strlen32(result));
    strcpy32(ttm->result->content,result);
}

static void
ttm_norm(TTM* ttm, Frame* frame) /* Obtain the Norm of a string */
{
    utf8* s;
    char result[32];
    int count;

    s = frame->argv[1];
    snprintf(result,sizeof(result),"%u",strlen32(s));
    setBufferLength(ttm,ttm->result,strlen(result)); /*temp*/
    count = toString32(ttm->result->content,result,TOEOS);
    setBufferLength(ttm,ttm->result,count);
}

static void
ttm_time(TTM* ttm, Frame* frame) /* Obtain time of day */
{
    char result[MAXINTCHARS+1];
    int count;
    struct timeval tv;
    long long time;

    if(timeofday(&tv) < 0)
        fail(ttm,TTM_ETIME);
    time = (long long)tv.tv_sec;
    time *= 1000000; /* convert to microseconds */
    time += tv.tv_usec;
    time = time / 10000; /* Need time in 100th second */
    snprintf(result,sizeof(result),"%lld",time);
    count = toString32(ttm->result->content,result,TOEOS);
    setBufferLength(ttm,ttm->result,count);
}

static void
ttm_xtime(TTM* ttm, Frame* frame) /* Obtain Execution Time */
{
    long long time;
    char result[MAXINTCHARS+1];
    int count;

    time = getRunTime();
    snprintf(result,sizeof(result),"%lld",time);
    setBufferLength(ttm,ttm->result,strlen(result));/*temp*/
    count = toString32(ttm->result->content,result,TOEOS);
    setBufferLength(ttm,ttm->result,count);
}

static void
ttm_ctime(TTM* ttm, Frame* frame) /* Convert ##<time> to printable string */
{
    utf8* stod;
    TTMERR err;
    long long tod;
    char_t result[1024];
    time_t ttod;
    unsigned count;
    int i;

    stod = frame->argv[1];
    err = toInt64(stod,&tod);
    if(err != TTM_ENOERR) fail(ttm,err);
    tod = tod/100; /* need seconds */
    ttod = (time_t)tod;
    snprintf(result,sizeof(result),"%s",ctime(&ttod));
    /* ctime adds a trailing new line; remove it */
    i = strlen(result);
    for(i--;i >= 0;i--) {
	if(result[i] != '\n' && result[i] != '\r') break;
    }
    result[i+1] = NUL;
    count = toString32(ttm->result->content,result,TOEOS);
    setBufferLength(ttm,ttm->result,count);
}

static void
ttm_tf(TTM* ttm, Frame* frame) /* Turn Trace Off */
{
    if(frame->argc > 1) {/* trace off specific*/
        unsigned i;
        for(i=1;i<frame->argc;i++) {
            Function* fcn = dictionaryLookup(ttm,frame->argv[i]);
            if(fcn == NULL) fail(ttm,TTM_ENONAME);      
            fcn->trace = 0;
        }
    } else { /* turn off all tracing */
        int i;
        for(i=0;i<HASHSIZE;i++) {
	    struct HashEntry* entry = ttm->dictionary.table[i].next;
            while(entry != NULL) {
		Function* name = (Function*)entry;
                name->trace = 0;
                entry = entry->next;
            }
        }
        ttm->flags &= ~(FLAG_TRACE);
    }
}

static void
ttm_tn(TTM* ttm, Frame* frame) /* Turn Trace On */
{
    if(frame->argc > 1) {/* trace specific*/
        unsigned i;
        for(i=1;i<frame->argc;i++) {
            Function* fcn = dictionaryLookup(ttm,frame->argv[i]);
            if(fcn == NULL) fail(ttm,TTM_ENONAME);      
            fcn->trace = 1;
        }
    } else 
        ttm->flags |= (FLAG_TRACE); /*trace all*/
}

/* Functions new to this implementation */

/* Get ith command line argument; zero is command */
static void
ttm_argv(TTM* ttm, Frame* frame)
{
    long long index = 0;
    TTMERR err;
    int count,arglen;
    char* arg;

    err = toInt64(frame->argv[1],&index);
    if(err != TTM_ENOERR) fail(ttm,err);
    if(index < 0 || index >= getOptionNameLength(argoptions))
        fail(ttm,TTM_ERANGE);
    arg = argoptions[index];
    arglen = strlen(arg);
    setBufferLength(ttm,ttm->result,arglen);/*temp*/
    count = toString32(ttm->result->content,arg,arglen);
    setBufferLength(ttm,ttm->result,count);
}

/* Get the length of argoptions */
static void
ttm_argc(TTM* ttm, Frame* frame)
{
    char result[MAXINTCHARS+1];
    int argc,count;

    argc = getOptionNameLength(argoptions);
    snprintf(result,sizeof(result),"%d",argc);
    setBufferLength(ttm,ttm->result,strlen(result));/*temp*/
    count = toString32(ttm->result->content,result,TOEOS);
    setBufferLength(ttm,ttm->result,count);
}

static void
ttm_classes(TTM* ttm, Frame* frame) /* Obtain all character class names */
{
    int i,nclasses,index;
    utf8** classes;
    unsigned len;
    utf8* p;

    /* First, figure out the number of classes */
    for(nclasses=0,i=0;i<HASHSIZE;i++) {
	struct HashEntry* entry = ttm->charclasses.table[i].next;
	while(entry != NULL) {
            nclasses++;
            entry = entry->next;
        }
    }

    if(nclasses == 0)
        return;

    /* Now collect all the class and their total size */
    classes = (utf8**)calloc(sizeof(utf8*),nclasses);
    if(classes == NULL) fail(ttm,TTM_EMEMORY);
    for(len=0,index=0,i=0;i<HASHSIZE;i++) {
	struct HashEntry* entry = ttm->charclasses.table[i].next;
	while(entry != NULL) {
            Charclass* charclass = (Charclass*)entry;
            classes[index++] = charclass->entry.name;
            len += strlen32(charclass->entry.name);
            entry = entry->next;
        }
    }

    qsort((void*)classes, nclasses, sizeof(char*),stringveccmp32);

    /* Return the set of classes separated by commas */    
    setBufferLength(ttm,ttm->result,len+(nclasses-1));
    p = ttm->result->content;
    for(i=0;i<nclasses;i++) {
        if(i > 0) {*p++ = ',';}
        strcpy32(p,classes[i]);
        p += strlen32(classes[i]);
    }

    /* Cleanup */
    nullfree(classes);
}

static void
ttm_lf(TTM* ttm, Frame* frame) /* Lock a function from being deleted */
{
    unsigned i;
    for(i=1;i<frame->argc;i++) {
        Function* fcn = dictionaryLookup(ttm,frame->argv[i]);
        if(fcn == NULL) fail(ttm,TTM_ENONAME);          
        fcn->locked = 1;
    }
}

static void
ttm_uf(TTM* ttm, Frame* frame) /* Un-Lock a function from being deleted */
{
    unsigned i;
    for(i=1;i<frame->argc;i++) {
        Function* fcn = dictionaryLookup(ttm,frame->argv[i]);
        if(fcn == NULL) fail(ttm,TTM_ENONAME);          
        fcn->locked = 0;
    }
}

static void
ttm_include(TTM* ttm, Frame* frame)  /* Include text of a file */
{
    utf8* path;
    FILE* finclude;
    Buffer* bb = ttm->result;
    char filename[8192];
    int count;

    path = frame->argv[1];
    if(strlen32(path) == 0)
        fail(ttm,TTM_EINCLUDE);
    count = toString8(filename,path,TOEOS,sizeof(filename));
    if(count < 0)
	fail(ttm,TTM_EINCLUDE);
    finclude = fopen(filename,"rb");
    if(finclude == NULL)
        fail(ttm,TTM_EINCLUDE);
    readfile(ttm,finclude,bb);
}

/**
Helper functions for all the ttm commands
and subcommands
*/

/**
#<ttm;meta;newmetachars>
*/
static void
ttm_ttm_meta(TTM* ttm, Frame* frame)
{
    utf8* arg = frame->argv[2];
    if(strlen32(arg) != 5) fail(ttm,TTM_ETTMCMD);
    ttm->sharpc = arg[0];
    ttm->openc = arg[1];
    ttm->semic = arg[2];
    ttm->closec = arg[3];
    ttm->escapec = arg[4];
}

/**
#<ttm;info;name;...>
*/
static void
ttm_ttm_info_name(TTM* ttm, Frame* frame)
{
    Function* str;
    Buffer* result = ttm->result;
    char info[8192];
    utf8* q;
    utf8* p;
    utf32 c32;
    unsigned namelen,count,i;

    setBufferLength(ttm,result,result->alloc-1);
    q = result->content;
    *q = NUL8;
    for(i=3;i<frame->argc;i++) {
        str = dictionaryLookup(ttm,frame->argv[i]);
        if(str == NULL) { /* not defined*/
            strcpy32(q,frame->argv[i]);
            q += strlen32(frame->argv[i]);
            count = toString32(q,"-,-,-",TOEOS);
            *q++ = '\n';
            continue;       
        }
        namelen = strlen32(str->entry.name);
        strcpy32(q,str->entry.name);
        q += namelen;
        if(str->builtin) {
            snprintf(info,sizeof(info),",%d,",str->minargs);
            count = toString32(q,info,TOEOS);
            q += count;
            if(str->maxargs == MAXARGS)
                *q++ = '*';
            else {
                snprintf(info,sizeof(info),"%d",str->maxargs);
                count = toString32(q,info,TOEOS);
                q += count;
            }
            *q++ = ',';
            *q++ = (str->novalue?'S':'V');
        } else {
            snprintf(info,sizeof(info),",0,%d,V",str->maxsegmark);
            count = toString32(q,info,TOEOS);
            q += count;
        }
        if(!str->builtin) {
            snprintf(info,sizeof(info)," residual=%u body=|",str->residual);
            count = toString32(q,info,TOEOS);
            q += count;
            /* Walk the body checking for segment and creation marks */
            p=str->body;
            while((c32=*p++)) {
                if(ismark(c32)) {
                    if(iscreate(c32))
                        strcpy(info,"^00");
                    else /* segmark */
                        snprintf(info,sizeof(info),"^%02d",(int)(c32 & 0xFF));
                    count = toString32(q,info,TOEOS);
                    q += count;
                } else
                    *q++ = c32;
            }
            *q++ = '|';
        }
        *q++ = '\n';
    }
    setBufferLength(ttm,result,(q - result->content));
#ifdef DEBUG
    fprintf(stderr,"info.name: ");
    dbgprint32(result->content,'"');
    fprintf(stderr,"\n");
    fflush(stderr);
#endif
}

/**
#<ttm;info;class;...>
*/
static void
ttm_ttm_info_class(TTM* ttm, Frame* frame) /* Misc. combined actions */
{
    Charclass* cl;
    utf8* q;
    utf8* p;
    utf32 c32;
    unsigned i,len;
    Buffer* result = ttm->result;

    q = result->content;
    *q = NUL;
    setBufferLength(ttm,result,result->alloc-1); /* max space avail */
    for(i=3;i<frame->argc;i++) {
        cl = charclassLookup(ttm,frame->argv[i]);
        if(cl == NULL) fail(ttm,TTM_ENONAME);
        len = strlen32(cl->entry.name);
        strcpy32(q,cl->entry.name);
        q += len;
        *q++ = ' ';
        *q++ = LBRACKET;
        if(cl->negative) *q++ = '^';
        for(p=cl->characters;(c32=*p++);) {
            if(c32 == LBRACKET || c32 == RBRACKET)
                *q++ = '\\';
            *q++ = c32;
        }
        *q++ = '\n';
    }
    setBufferLength(ttm,result,(q - result->content));
#ifdef DEBUG
    fprintf(stderr,"info.class: ");
    dbgprint32(result->content,'"');
    fprintf(stderr,"\n");
    fflush(stderr);
#endif
}

/**
#<ttm;info;class;...>
*/

static void
ttm_ttm(TTM* ttm, Frame* frame) /* Misc. combined actions */
{
    /* Get the discriminate string */
    char discrim[(4*255)+1]; /* upper bound for 255 characters + nul term */
    int count;
    
    count = toString8(discrim,frame->argv[1],TOEOS,sizeof(discrim));
    discrim[count] = NUL;

    if(frame->argc >= 3 && strcmp("meta",discrim)==0) {
        ttm_ttm_meta(ttm,frame);
    } else if(frame->argc >= 4 && strcmp("info",discrim)==0) {
        count = toString8(discrim,frame->argv[2],TOEOS,sizeof(discrim));
        discrim[count] = NUL;
        if(strcmp("name",discrim)==0) {
            ttm_ttm_info_name(ttm,frame);
        } else if(strcmp("class",discrim)==0) {
            ttm_ttm_info_class(ttm,frame);
        } else
            fail(ttm,TTM_ETTMCMD);
    } else {
        fail(ttm,TTM_ETTMCMD);
    }

}

/**************************************************/

/**
 Builtin function table
*/

struct Builtin {
    char* name;
    unsigned minargs;
    unsigned maxargs;
    char* sv;
    TTMFCN fcn;
};

/* TODO: fix the minargs values */

/* Define a subset of the original TTM functions */

/* Define some temporary macros */
#define ARB MAXARGS

static struct Builtin builtin_orig[] = {
    /* Dictionary Operations */
    {"ap",2,2,"S",ttm_ap}, /* Append to a string */
    {"cf",2,2,"S",ttm_cf}, /* Copy a function */
    {"cr",2,2,"S",ttm_cr}, /* Mark for creation */
    {"ds",2,2,"S",ttm_ds}, /* Define string */
    {"es",1,ARB,"S",ttm_es}, /* Erase string */
    {"sc",2,63,"SV",ttm_sc}, /* Segment and count */
    {"ss",2,2,"S",ttm_ss}, /* Segment a string */
    /* Stateful String Selection */
    {"cc",1,1,"SV",ttm_cc}, /* Call one character */
    {"cn",2,2,"SV",ttm_cn}, /* Call n characters */
    {"sn",2,2,"S",ttm_sn}, /* Skip n characters */ /*Batch*/
    {"cp",1,1,"SV",ttm_cp}, /* Call parameter */
    {"cs",1,1,"SV",ttm_cs}, /* Call segment */
    {"isc",4,4,"SV",ttm_isc}, /* Initial character scan */
    {"rrp",1,1,"S",ttm_rrp}, /* Reset residual pointer */
    {"scn",3,3,"SV",ttm_scn}, /* Character scan */
    /* Stateful String Scanning Operations */
    {"gn",2,2,"V",ttm_gn}, /* Give n characters */
    {"zlc",1,1,"V",ttm_zlc}, /* Zero-level commas */
    {"zlcp",1,1,"V",ttm_zlcp}, /* Zero-level commas and parentheses */
    {"flip",1,1,"V",ttm_flip}, /* Flip a string */ /*Batch*/
    /* Character Class Operations */
    {"ccl",2,2,"SV",ttm_ccl}, /* Call class */
    {"dcl",2,2,"S",ttm_dcl}, /* Define a class */
    {"dncl",2,2,"S",ttm_dncl}, /* Define a negative class */
    {"ecl",1,ARB,"S",ttm_ecl}, /* Erase a class */
    {"scl",2,2,"S",ttm_scl}, /* Skip class */
    {"tcl",4,4,"V",ttm_tcl}, /* Test class */
    /* Arithmetic Operations */
    {"abs",1,1,"V",ttm_abs}, /* Obtain absolute value */
    {"ad",2,ARB,"V",ttm_ad}, /* Add */
    {"dv",2,2,"V",ttm_dv}, /* Divide and give quotient */
    {"dvr",2,2,"V",ttm_dvr}, /* Divide and give remainder */
    {"mu",2,ARB,"V",ttm_mu}, /* Multiply */
    {"su",2,2,"V",ttm_su}, /* Substract */
    /* Numeric Comparisons */
    {"eq",4,4,"V",ttm_eq}, /* Compare numeric equal */
    {"gt",4,4,"V",ttm_gt}, /* Compare numeric greater-than */
    {"lt",4,4,"V",ttm_lt}, /* Compare numeric less-than */
    /* Logical Comparisons */
    {"eq?",4,4,"V",ttm_eql}, /* ? Compare logical equal */
    {"gt?",4,4,"V",ttm_gtl}, /* ? Compare logical greater-than */
    {"lt?",4,4,"V",ttm_ltl}, /* ? Compare logical less-than */
    /* Peripheral Input/Output Operations */
    {"cm",1,1,"S",ttm_cm}, /*Change Meta Character*/
    {"ps",1,2,"S",ttm_ps}, /* Print a string */
    {"psr",1,1,"SV",ttm_psr}, /* Print string and then read */
#ifdef IMPLEMENTED
    {"rcd",2,2,"S",ttm_rcd}, /* Set to Read from cards */
#endif
    {"rs",0,0,"V",ttm_rs}, /* Read a string */
    /*Formated Output Operations*/
#ifdef IMPLEMENTED
    {"fm",1,ARB,"S",ttm_fm}, /* Format a Line or Card */
    {"tabs",1,8,"S",ttm_tabs}, /* Declare Tab Positions */
    {"scc",2,2,"S",ttm_scc}, /* Set Continuation Convention */
    {"icc",1,1,"S",ttm_icc}, /* Insert a Control Character */
    {"outb",0,3,"S",ttm_outb}, /* Output the Buffer */
#endif
    /* Library Operations */
#ifdef IMPLEMENTED
    {"store",2,2,"S",ttm_store}, /* Store a Program */
    {"delete",1,1,"S",ttm_delete}, /* Delete a Program */
    {"copy",1,1,"S",ttm_copy}, /* Copy a program */
    {"show",0,1,"S",ttm_show}, /* Show program strings */
    {"libs",2,2,"S",ttm_libs}, /* Declare standard qualifiers */ /*Batch*/
#endif
    {"names",0,1,"V",ttm_names}, /* Obtain name strings */
    /* Utility Operations */
#ifdef IMPLEMENTED
    {"break",0,1,"S",ttm_break}, /* Program Break */
#endif
    {"exit",0,0,"S",ttm_exit}, /* Return from TTM */
    {"ndf",3,3,"V",ttm_ndf}, /* Determine if a name is defined */
    {"norm",1,1,"V",ttm_norm}, /* Obtain the norm (length) of a string */
    {"time",0,0,"V",ttm_time}, /* Obtain time of day (modified) */
    {"xtime",0,0,"V",ttm_xtime}, /* Obtain execution time */ /*Batch*/
    {"tf",0,0,"S",ttm_tf}, /* Turn Trace Off */
    {"tn",0,0,"S",ttm_tn}, /* Turn Trace On */
    {"eos",3,3,"V",ttm_eos}, /* Test for end of string */ /*Batch*/

#ifdef IMPLEMENTED
/* Batch Functions */
    {"insw",2,2,"S",ttm_insw}, /* Control output of input monitor */ /*Batch*/
    {"ttmsw",2,2,"S",ttm_ttmsw}, /* Control handling of ttm programs */ /*Batch*/
    {"cd",0,0,"V",ttm_cd}, /* Input one card */ /*Batch*/
    {"cdsw",2,2,"S",ttm_cdsw}, /* Control cd input */ /*Batch*/
    {"for",0,0,"V",ttm_for}, /* Input next complete fortran statement */ /*Batch*/
    {"forsw",2,2,"S",ttm_forsw}, /* Control for input */ /*Batch*/
    {"pk",0,0,"V",ttm_pk}, /* Look ahead one card */ /*Batch*/
    {"pksw",2,2,"S",ttm_pksw}, /* Control pk input */ /*Batch*/
    {"ps",1,1,"S",ttm_ps}, /* Print a string */ /*Batch*/ /*Modified*/
    {"page",1,1,"S",ttm_page}, /* Specify page length */ /*Batch*/
    {"sp",1,1,"S",ttm_sp}, /* Space before printing */ /*Batch*/
    {"fm",0,ARB,"S",ttm_fm}, /* Format a line or card */ /*Batch*/
    {"tabs",1,10,"S",ttm_tabs}, /* Declare tab positions */ /*Batch*/ /*Modified*/
    {"scc",3,3,"S",ttm_scc}, /* Set continuation convention */ /*Batch*/
    {"fmsw",2,2,"S",ttm_fmsw}, /* Control fm output */ /*Batch*/
    {"time",0,0,"V",ttm_time}, /* Obtain time of day */ /*Batch*/ /*Modified*/
    {"des",1,1,"S",ttm_des}, /* Define error string */ /*Batch*/
#endif

    {NULL,0,0,NULL} /* terminator */
    };
    
/* Functions new to this implementation */
static struct Builtin builtin_new[] = {
    {"argv",1,1,"V",ttm_argv}, /* Get ith command line argument; 0<=i<argc */
    {"argc",0,0,"V",ttm_argc}, /* no. of command line arguments */
    {"classes",0,0,"V",ttm_classes}, /* Obtain character class names */
    {"ctime",1,1,"V",ttm_ctime}, /* Convert time to printable string */
    {"include",1,1,"S",ttm_include}, /* Include text of a file */
    {"lf",0,ARB,"S",ttm_lf}, /* Lock functions */
    {"pf",0,1,"S",ttm_pf}, /* flush stderr and/or stdout */
    {"uf",0,ARB,"S",ttm_uf}, /* Unlock functions */
    {"ttm",1,ARB,"SV",ttm_ttm}, /* Misc. combined actions */
    {NULL,0,0,NULL} /* terminator */
};

static void
defineBuiltinFunction1(TTM* ttm, struct Builtin* bin)
{
    Function* fcn;
    utf32 binname[8192];
    int count;

    count = toString32(binname,bin->name,strlen(bin->name));
    binname[count] = NUL8;
    /* Make sure we did not define builtin twice */
    fcn = dictionaryLookup(ttm,binname);
    if(fcn != NULL)
	fatal(ttm,"Duplicate builtin function");
    /* create a new function object */
    fcn = newFunction(ttm);
    fcn->builtin = 1;
    fcn->minargs = bin->minargs;
    fcn->maxargs = bin->maxargs;
    if(strcmp(bin->sv,"S")==0)
        fcn->novalue = 1;
    fcn->fcn = bin->fcn;
    /* Convert name to utf32 */
    assert(fcn->entry.name == NULL);
    fcn->entry.name = strdup32(binname);
    if(!dictionaryInsert(ttm,fcn)) {
	freeFunction(ttm,fcn);
	fatal(ttm,"Dictionary insertion failed");
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
/**
Startup commands: execute before
any -e or -f arguments.
Beware that only
the defaults instance variables are defined.
 */

static char* startup_commands[] = {
"#<ds;comment;>",
"#<ds;def;<##<ds;name;<text>>##<ss;name;subs>>>#<ss;def;name;subs;text>",
NULL
};

static void
startupcommands(TTM* ttm)
{
    int count,cmdlen;
    char* cmd;
    char** cmdp;
    int saveflags = ttm->flags;
    ttm->flags &= ~FLAG_TRACE;

    for(cmdp=startup_commands;*cmdp != NULL;cmdp++) {
	cmd = *cmdp;
        cmdlen = strlen(cmd);
        resetBuffer(ttm,ttm->buffer);
        setBufferLength(ttm,ttm->buffer,cmdlen); /* temp */
        count = toString32(ttm->buffer->content,cmd,cmdlen);     
        setBufferLength(ttm,ttm->buffer,count);
        scan(ttm);
        resetBuffer(ttm,ttm->buffer); /* throw away any result */
    }
    ttm->flags = saveflags;
}

/**************************************************/
/* Lock all the names in the dictionary */
static void
lockup(TTM* ttm)
{
    int i;
    for(i=0;i<HASHSIZE;i++) {
	struct HashEntry* entry = ttm->dictionary.table[i].next;
        while(entry != NULL) {
	    Function* name = (Function*)entry;
	    name->locked = 1;
            entry = entry->next;
        }
    }
}


/**************************************************/
/* Error reporting */

static void
fail(TTM* ttm, TTMERR eno)
{
    char msg[4096];
    snprintf(msg,sizeof(msg),"(%d) %s",eno,errstring(eno));
    fatal(ttm,msg);
}

static void
fatal(TTM* ttm, const char* msg)
{
    fprintf(stderr,"Fatal error: %s\n",msg);
    if(ttm != NULL) {
        /* Dump the frame stack */
        dumpstack(ttm);
        /* Dump passive and active strings*/
        fprintf(stderr,"context: ");
        /* Since passive is not normally null terminated, we need to fake it */
        {
            utf32 save = *ttm->buffer->passive;
            *ttm->buffer->passive = NUL8;
            dbgprint32(ttm->buffer->content,'|');
            *ttm->buffer->passive = save;
        }
        fprintf(stderr," ... ");
        dbgprint32(ttm->buffer->active,'|');
        fprintf(stderr,"\n");
    }
    fflush(stderr);
    exit(1);
}

static const char*
errstring(TTMERR err)
{
    const char* msg = NULL;
    switch(err) {
    case TTM_ENOERR: msg="No error"; break;
    case TTM_ENONAME: msg="Dictionary Name or Character Class Name Not Found"; break;
    case TTM_ENOPRIM: msg="Primitives Not Allowed"; break;
    case TTM_EFEWPARMS: msg="Too Few Parameters Given"; break;
    case TTM_EFORMAT: msg="Incorrect Format"; break;
    case TTM_EQUOTIENT: msg="Quotient Is Too Large"; break;
    case TTM_EDECIMAL: msg="Decimal Integer Required"; break;
    case TTM_EMANYDIGITS: msg="Too Many Digits"; break;
    case TTM_EMANYSEGMARKS: msg="Too Many Segment Marks"; break;
    case TTM_EMEMORY: msg="Dynamic Storage Overflow"; break;
    case TTM_EPARMROLL: msg="Parm Roll Overflow"; break;
    case TTM_EINPUTROLL: msg="Input Roll Overflow"; break;
#ifdef IMPLEMENTED
    case TTM_EDUPLIBNAME: msg="Name Already On Library"; break;
    case TTM_ELIBNAME: msg="Name Not On Library"; break;
    case TTM_ELIBSPACE: msg="No Space On Library"; break;
    case TTM_EINITIALS: msg="Initials Not Allowed"; break;
    case TTM_EATTACH: msg="Could Not Attach"; break;
#endif
    case TTM_EIO: msg="An I/O Error Occurred"; break;
#ifdef IMPLEMENTED
    case TTM_ETTM: msg="A TTM Processing Error Occurred"; break;
    case TTM_ESTORAGE: msg="Error In Storage Format"; break;
#endif
    case TTM_ENOTNEGATIVE: msg="Only unsigned decimal integers"; break;
    /* messages new to this implementation */
    case TTM_ESTACKOVERFLOW: msg="Stack overflow"; break;
    case TTM_ESTACKUNDERFLOW: msg="Stack Underflow"; break;
    case TTM_EBUFFERSIZE: msg="Buffer overflow"; break;
    case TTM_EMANYINCLUDES: msg="Too many includes"; break;
    case TTM_EINCLUDE: msg="Cannot read Include file"; break;
    case TTM_ERANGE: msg="index out of legal range"; break;
    case TTM_EMANYPARMS: msg="Number of parameters greater than MAXARGS"; break;
    case TTM_EEOS: msg="Unexpected end of string"; break;
    case TTM_EASCII: msg="ASCII characters only"; break;
    case TTM_ECHAR8: msg="Illegal utf-8 character set"; break;
    case TTM_EUTF32: msg="Illegal utf-32 character set"; break;
    case TTM_ETTMCMD: msg="Illegal #<ttm> command"; break;
    case TTM_ETIME: msg="Gettimeofday() failed"; break;
    case TTM_EEXECCOUNT: msg="too many executions"; break;
    case TTM_EOTHER: msg="Unknown Error"; break;
    }
    return msg;
}

/**************************************************/
/* Debug utility functions */

#ifdef GDB

static void
dumpnames(TTM* ttm)
{
    int i;
    for(i=0;i<HASHSIZE;i++) {
	struct HashEntry* entry = ttm->dictionary.table[i].next;
	fprintf(stderr,"[%3d]",i);
        while(entry != NULL) {
	    fprintf(stderr," ");	
	    dbgprint32(entry->name,'|');
            entry = entry->next;
        }
	fprintf(stderr,"\n");	
    }
}

static void
dumpframe(TTM* ttm, Frame* frame)
{
    traceframe(ttm,frame,1);
    fprintf(stderr,"\n");
}

#endif

/**************************************************/
/* Utility functions */


/**
Convert a long long to a utf32 string
*/

static int
int2string(utf8* dst, long long n)
{
    char result[MAXINTCHARS+1];
    int count;

    snprintf(result,sizeof(result),"%lld",n);
    count = toString32(dst,result,TOEOS);
    return count;
}

/**
Convert a string to a signed Long
Use this regular expression:
      [ \t]*[+-]?[0-9]+[MmKk]?[ \t]*
    | [ \t]*0[Xx][0-9a-fA-F]+[ \t]*
In the first case, the value must be in the inclusive range:
    -9223372036854775807..9223372036854775807
Note that this excludes the smallest possible value:
    -9223372036854775808
so that we do can detect overflow.
In the second case, the value must be in the range:
    0..18446744073709551615
but the value will be treated as unsigned when returned; this
allows one to create any arbitrary 8 byte bit pattern.

Return TTM_ENOERR if conversion succeeded, err if failed.

*/

static TTMERR
toInt64(utf8* s, long long* lp)
{
    utf8* p = s;
    utf32 c;
    int negative = 1;
    /* skip white space */
    while((c=*p++)) {if(c != ' ' && c != '\t') break;}
    if(c == NUL) return TTM_EDECIMAL; /* nothing but white space or just a sign*/
    if(c == '-') {negative = -1; c=*p++;} else if(c == '+') {c=*p++;}
    if(c == NUL) return TTM_EDECIMAL; /* just a +|- */
    if(c == '0' && (*p == 'x' || *p == 'X')) { /* hex case */
        unsigned long ul = 0;
        int i;
        for(i=0;i<16;i++) {/* allow no more than 16 hex digits */
            c=*p++;
            if(!ishex(c)) break;
            ul = (ul<<8) | fromhex(c);
        }
        if(i==0)
            return TTM_EDECIMAL; /* it was just "0x" */
        if(i == 16) /* too big */
            return TTM_EMANYDIGITS;
        if(lp) {*lp = *(long long*)&ul;} /* return as signed value */
        return TTM_ENOERR;
    } else if(c >= '0' && c <= '9') { /* decimal */
        unsigned long long ul;
        ul = (c - '0');
        while((c=*p++)) {
            int digit;
            if(c < '0' || c > '9') break;
            digit = (c-'0');
            ul = (ul * 10) + digit;
            if((ul & 0x8000000000000000ULL) != 0) {
                /* Sign bit was set => overflow */
                return TTM_EMANYDIGITS;
            }
        }      
        if(lp) {
            /* convert back to signed */
            long long l;
            l = *(long long*)&ul;
            l *= negative;
            *lp = l;
        }
        return TTM_ENOERR;
    } /* else illegal */
    return TTM_EDECIMAL;
}

/* Given a char, return its escaped value.
Zero indicates it should be elided
*/
static utf8cp
convertEscapeChar(utf8* cp)
{
    utf8cpu u;
    u.cp = empty_cp;

    if(u8size(*cp) == 1) {
        utf8 c = *cp;
	/* de-escape and store */
	switch (c) {
	case 'r': c = '\r'; break;
	case 'n': c = '\n'; break;
	case 't': c = '\t'; break;
	case 'b': c = '\b'; break;
	case 'f': c = '\f'; break;
	case '\n': c = 0; break; /* escaped eol is elided */
	default: break; /* no change */
	}
	u.cpa[0] = c;
    } else
	u.cp = u8cp(c);
    return u.cp;
}

#ifdef GDB
static const char*
dbgsframe32(Frame* frame)
{
    unsigned i = 0;
    static char sf[8192];
    static char sname[128];

    sf[0] = '\0'; sname[0] = '\0';
    dbgsprint32(frame->argv[0],NUL,sname);
    snprintf(sf,sizeof(sf),"Frame{active=%d name=%s argc=%u argv=[",frame->active,sname,frame->argc);
    for(i=1;i<frame->argc;i++) {
	if(i > 1) strcat(sf+strlen(sf),",");
	strcat(sf+strlen(sf),"|");
	dbgsprint32(frame->argv[i],NUL,sf+strlen(sf));
	strcat(sf+strlen(sf),"|");
    }
    strcat(sf+strlen(sf),"]}");
    return sf;
}
#endif

static void
traceframe(TTM* ttm, Frame* frame, int traceargs)
{
    char tag[4];
    unsigned i = 0;

    if(frame->argc == 0) {
	fprintf(stderr,"#<empty frame>");
	return;
    }
    tag[i++] = (char)ttm->sharpc;
    if(!frame->active)
        tag[i++] = (char)ttm->sharpc;
    tag[i++] = (char)ttm->openc;
    tag[i] = NUL;
    fprintf(stderr,"%s",tag);
    dbgprint32(frame->argv[0],NUL);
    if(traceargs) {
        for(i=1;i<frame->argc;i++) {
            fputc32(ttm->semic,stderr);
            dbgprint32(frame->argv[i],NUL);
        }
    }
    fputc32(ttm->closec,stderr);
    fflush(stderr);
}

static void
trace1(TTM* ttm, int depth, int entering, int tracing)
{
    Frame* frame;

    if(tracing && ttm->stacknext == 0) {
        fprintf(stderr,"trace: no frame to trace\n");
	fflush(stderr);
        return;
    }   
    frame = &ttm->stack[depth];
    fprintf(stderr,"[%02d] ",depth);
    if(tracing)
        fprintf(stderr,"%s: ",(entering?"begin":"end"));
    traceframe(ttm,frame,entering);
    /* Dump the contents of result if !entering */
    if(!entering) {
        fprintf(stderr," => ");
        dbgprint32(ttm->result->content,'"');
    } 
    fprintf(stderr,"\n");
    fflush(stderr);
}

/**
Trace a top frame in the frame stack.
*/
static void
trace(TTM* ttm, int entering, int tracing)
{
    trace1(ttm, ttm->stacknext-1, entering, tracing);
}


/**************************************************/
/* Debug Support */
/**
Dump the stack
*/
static void
dumpstack(TTM* ttm)
{
    unsigned i;
    for(i=1;i<=ttm->stacknext;i++) {
        trace1(ttm,i,1,!TRACING);
    }
    fflush(stderr);
}

#ifdef DEBUG
static void
dbgprint32c(utf32 c, char quote)
{
    char sf[8102];
    dbgsprint32c(c,quote,sf,sizeof(sf));
    fprintf(stderr,"%s",sf);
}
#endif

static char*
dbgsprint32c(utf32 c, char quote, char* p)
{
    size_t i;
    if(ismark(c)) {
	char info[8];
        if(iscreate(c)) {
	    snprintf(info,sizeof(info),"%s","^00");
	} else {/* segmark */
            snprintf(info,sizeof(info),"^%02d",(int)(c & 0xFF));
	}
	for(i=0;i<strlen(info);i++) *p++ = info[i];
    } else if(iscontrol(c)) {
	*p++ = '\\';
        switch (c) {
        case '\r': *p++ = 'r'; break;
        case '\n': *p++ = 'n'; break;
        case '\t': *p++ = 't'; break;
        case '\b': *p++ = 'b'; break;
        case '\f': *p++ = 'f'; break;
        default: {
            /* dump as a decimal character */
            char digits[4];
            snprintf(digits,sizeof(digits),"%d",(int)c);
	    for(i=0;i<strlen(digits);i++) *p++ = digits[i];
            } break;
        }
    } else {
	size_t count;
	char u[4]; /* unicode sequence */
        if(c == quote) *p++ = '\\';
	count = (size_t)toChar8(u,c);
	for(i=0;i<count;i++) *p++ = u[i];
    }
    return p;
}

static void
dbgprint32(utf8* s, char quote)
{
    char sf[8192];
    dbgsprint32(s,quote,sf);
    fprintf(stderr,"%s",sf);
}

static void
dbgsprint32(utf8* s, char quote, char* sf)
{
    unsigned i,slen;
    char* p = sf;
    slen = strlen32(s);
    if(quote != NUL) *p++ = quote;
    for(i=0;i<slen;i++) {
	utf32 c = s[i];
	p = dbgsprint32c(c,quote,p);
    }
    if(quote != NUL) *p++ = quote;
    *p++ = '\0';
    fflush(stderr);
}

/**************************************************/
/* Main() Support functions */

static int
getOptionNameLength(char** list)
{
    unsigned i;
    char** p;
    for(i=0,p=list;*p;i++,p++);
    return i;
}

static int
pushOptionName(char* option, unsigned max, char** list)
{
    unsigned i;
    for(i=0;i<max;i++) {
        if(list[i] == NULL) {
            list[i] = (char*)calloc(1,strlen(option)+1);
            strcpy(list[i],option);
            return 1;
        }
    }
    return 0;
}

static void
initglobals()
{
    memset((void*)eoptions,0,sizeof(eoptions));
    memset((void*)argoptions,0,sizeof(argoptions));
}

static void
usage(const char* msg)
{
    if(msg != NULL)
        fprintf(stderr,"%s\n",msg);
    fprintf(stderr,"%s\n",
"usage: ttm "
"[-d string]"
"[-e string]"
"[-p programfile]"
"[-f inputfile]"
"[-o file]"
"[-i]"
"[-V]"
"[-q]"
"[-X tag=value]"
"[--]"
"[arg...]");
    fprintf(stderr,"\tOptions may be repeated\n");
    if(msg != NULL) exit(1); else exit(0);
}

static void
readinput(TTM* ttm, const char* filename,Buffer* bb)
{
    utf8* content = NULL;
    FILE* f = NULL;
    int isstdin = 0;
    unsigned i;
    unsigned buffersize = bb->alloc;

    if(strcmp(filename,"-") == 0) {
        /* Read from stdinput */
        isstdin = 1;
        f = stdin;      
    } else {
        f = fopen(filename,"rb");
        if(f == NULL) {
            fprintf(stderr,"Cannot read file: %s\n",filename);
            exit(1);
        }
    }
    
    resetBuffer(ttm,bb);
    content = bb->content;

    /* Read utf8 character by character until EOF and convert to utf32 */
    for(i=0;i<buffersize-1;i++) {
        utf32 c32;
        c32 = fgetc32(f);
        if(c32 == EOF) break;           
        if(c32 == ttm->escapec) {
            *content++ = c32;
            c32 = fgetc32(f);
        }
        *content++ = c32;
    }
    setBufferLength(ttm,bb,i);
    if(!isstdin) fclose(f);
}

/**
Read from input until the
read characters form a 
balanced string with respect
to the current open/close characters.
Read past the final balancing character
to the next end of line.
Return 0 if the read was terminated
by EOF. 1 otherwise.
*/

static int
readbalanced(TTM* ttm)
{
    Buffer* bb;
    utf8* content;
    utf32 c32;
    unsigned depth,i;
    unsigned buffersize;

    bb = ttm->buffer;
    resetBuffer(ttm,bb);
    buffersize = bb->alloc;
    content = bb->content;

    /* Read character by character until EOF; take escapes and open/close
       into account; keep outer <...> */
    for(depth=0,i=0;i<buffersize-1;i++) {
        c32 = fgetc32(stdin);
        if(c32 == EOF) break;
        if(c32 == ttm->escapec) {
            *content++ = c32;
            c32 = fgetc32(stdin);
        }
        *content++ = c32;
        if(c32 == ttm->openc) {
            depth++;
        } else if(c32 == ttm->closec) {
            depth--;
            if(depth == 0) break;
        }
    }
    /* skip to end of line */
    while(c32 != EOF && c32 != '\n') {c32 = fgetc32(stdin);}
    setBufferLength(ttm,bb,i);
    return (i == 0 && c32 == EOF ? 0 : 1);
}

static void
printbuffer(TTM* ttm)
{
    printstring(ttm,ttm->output,ttm->buffer->content);
}

static int
readfile(TTM* ttm, FILE* file, Buffer* bb)
{
    long filesize;
    utf8* p;
    int count32;

    fseek(file,0,SEEK_SET); /* end of the file */
    filesize = ftell(file); /* get file length */
    rewind(file);
 
    setBufferLength(ttm,bb,filesize); /* temp */
    count32 = 0;
    p = bb->content;
    for(;;) {
        utf32 c = fgetc32(file);
        if(ferror(file)) fail(ttm,TTM_EIO);
        if(c == EOF) break;
        *p++ = c;
    }        
    *p = NUL8;
    count32 = (p - bb->content);
    setBufferLength(ttm,bb,count32);
    return count32;
}

static long
tagvalue(const char* p)
{
    unsigned value;
    int c;
    if(p == NULL || p[0] == NUL)
        return -1;
    value = atol(p);
    c = p[strlen(p)-1];
    switch (c) {
    case 0: break;
    case 'm': case 'M': value *= (1<<20); break;
    case 'k': case 'K': value *= (1<<10); break;
    default: break;
    }
    return value;
}

static int
setdebugflags(const char* flagstring)
{
    const char* p = flagstring;
    int c;
    int flags = 0;
    if(flagstring == NULL) return flags;
    while((c=*p++)) {
	switch (c) {
	case 't': flags |= FLAG_TRACE; break;
	case 'b': flags |= FLAG_BARE; break;
	default: break;
	}
    }
    return flags;
}

/**************************************************/
/* Main() */

static char* options = "d:e:f:io:p:qI:VX:-";

int
main(int argc, char** argv)
{
    int i,exitcode;
    long buffersize = 0;
    long stacksize = 0;
    long execcount = 0;
    char debugargs[16] = {'\0'};
    int interactive = 0;
    char* outputfilename = NULL;
    char* executefilename = NULL; /* This is the ttm file to execute */
    char* inputfilename = NULL; /* This is data for #<rs> */
    int isstdout = 1;
    FILE* outputfile = NULL;
    int isstdin = 1;
    FILE* inputfile = NULL;
    int c;
    char* p;
    int flags;
    int quiet = 0;
#ifndef TTMGLOBAL
    TTM* ttm = NULL;
#endif

    if(argc == 1)
        usage(NULL);

    initglobals();

    /* Stash argv[0] */
    pushOptionName(argv[0],MAXARGS,argoptions);

    while ((c = getopt(argc, argv, options)) != EOF) {
        switch(c) {
        case 'd':
            strcat(debugargs,optarg);
            break;
        case 'e':
            pushOptionName(optarg,MAXEOPTIONS,eoptions);
            break;
        case 'f':
            if(inputfilename == NULL)
                inputfilename = strdup(optarg);
	    interactive = 0;
            break;
        case 'o':
            if(outputfilename == NULL)
                outputfilename = strdup(optarg);
            break;
        case 'p':
            if(executefilename == NULL)
                executefilename = strdup(optarg);
            break;
        case 'q': quiet = 1; break;
        case 'V':
            printf("ttm version: %s\n",VERSION);
            exit(0);
            break;
        case 'X':
            if(optarg == NULL) usage("Illegal -X tag");
            p = optarg;
            c = *p++;
            if(*p++ != '=') usage("Missing -X tag value");
            switch (c) {
            case 'b':
                if(buffersize == 0 && (buffersize = tagvalue(p)) < 0)
                    usage("Illegal buffersize");
                break;
            case 's':
                if(stacksize == 0 && (stacksize = tagvalue(p)) < 0)
                    usage("Illegal stacksize");
                break;
            case 'x':
                if(execcount == 0 && (execcount = tagvalue(p)) < 0)
                    usage("Illegal execcount");
                break;
            default: usage("Illegal -X option");
            }
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
            pushOptionName(argv[optind],MAXARGS,argoptions);
    }

    /* Complain if interactive and output file name specified */
    if(outputfilename != NULL && interactive) {
        fprintf(stderr,"Interactive is illegal if output file specified\n");
        exit(1);
    }

    if(buffersize < DFALTBUFFERSIZE)
        buffersize = DFALTBUFFERSIZE;         
    if(stacksize < DFALTSTACKSIZE)
        stacksize = DFALTSTACKSIZE;           
    if(execcount < DFALTEXECCOUNT)
        execcount = DFALTEXECCOUNT;           

    if(outputfilename == NULL) {
        outputfile = stdout;
        isstdout = 1;
    } else {
        outputfile = fopen(outputfilename,"wb");
        if(outputfile == NULL) {
            fprintf(stderr,"Output file is not writable: %s\n",outputfilename);
            exit(1);
        }           
        isstdout = 0;
    }

    if(inputfilename == NULL) {
        inputfile = stdin;
        isstdin = 1;
    } else {
        inputfile = fopen(inputfilename,"rb");
        if(inputfile == NULL) {
            fprintf(stderr,"-f file is not readable: %s\n",inputfilename);
            exit(1);
        }           
        isstdin = 0;
    }

    /* Create the ttm state */
    ttm = newTTM(buffersize,stacksize,execcount);
    ttm->output = outputfile;
    ttm->isstdout = isstdout;
    ttm->input = inputfile;
    ttm->isstdin = isstdin;    

    defineBuiltinFunctions(ttm);
    
    /* Define flags */
    flags = setdebugflags(debugargs);
    ttm->flags |= flags;

    if(!testMark(ttm->flags,FLAG_BARE))
      startupcommands(ttm);
    /* Lock up all the currently defined functions */
    lockup(ttm);

    /* Execute the -e strings in turn */
    for(i=0;eoptions[i]!=NULL;i++) {
        char* eopt = eoptions[i];
        int count,elen = strlen(eopt);

        resetBuffer(ttm,ttm->buffer);
        setBufferLength(ttm,ttm->buffer,elen); /* temp */
        count = toString32(ttm->buffer->content,eopt,elen);     
        setBufferLength(ttm,ttm->buffer,count);
        scan(ttm);
        if(ttm->flags & FLAG_EXIT)
            goto done;
    }

    /* Now execute the executefile, if any, and if -q, discard output */
    if(executefilename != NULL) {
        readinput(ttm,executefilename,ttm->buffer);
        scan(ttm);
        if(ttm->flags & FLAG_EXIT)
            goto done;
    }    

    /* If interactive, start read-eval loop */
    if(interactive) {
        for(;;) {
            if(!readbalanced(ttm)) break;
            scan(ttm);
            /* make sure passive is null terminated */
            *ttm->buffer->passive = NUL8;
            if(!quiet && ttm->buffer->passive > 0)
	        printbuffer(ttm);
            if(ttm->flags & FLAG_EXIT)
                goto done;
        }
    }

done:
    /* Dump any output left in the buffer */
    if(!quiet && ttm->buffer->passive > 0)
        printbuffer(ttm);

    exitcode = ttm->exitcode;

    /* cleanup */
    if(!ttm->isstdout) fclose(ttm->output);
    if(!ttm->isstdin) fclose(ttm->input);

    /* Clean up misc state */
    nullfree(outputfilename);
    nullfree(executefilename);
    nullfree(inputfilename);

    freeTTM(ttm);

    return (exitcode?1:0); // exit(exitcode);
}

/* Replacments for selected string.h functions */

static size_t
strlcatx(char* dst, const char* src, size_t size)
{
    const char* p = src;
    char* q = dst;
    size_t n = size;
    size_t dstlen,avail;
 
    /* Find the end of dst and adjust bytes left but don't go past end */
    while (n-- != 0 && *q != '\0') q++;
    dstlen = (q - dst);
    avail = (size - dstlen);
    if(avail == 0) return (dstlen + strlen(s));
    for(;*p != '\0';p++) {
	if(n != 1) {*q++ = *p; n--;}
    }
    *q = '\0';
    return (dstlen + (p - src));  /* count does not include NUL */
}

#if 0
static unsigned 
strlen32(const utf8* s)
{
    unsigned len = 0;
    while(*s++)
        len++;
    return len;
}

static void
strcpy32(utf8* dst, const utf8* src)
{
    while((*dst++ = *src++));
}

static void
strncpy32(utf8* dst, const utf8* src, unsigned len)
{
    utf32 c32 = 0;
    for(;len>0 && (c32 = *src++);len--){*dst++ = c32;}
    if(c32 != NUL8) *dst = NUL8;
}

#if 0
static void
strcat32(utf8* dst, const utf8* src)
{
    /* find end of dst */
    while(*dst++);
    while((*dst++ = *src++));
}
#endif

static utf8*
strdup32(const utf8* src)
{
    unsigned len = 0;
    utf8* dup = 0;

    len = strlen32(src);
    dup = (utf8*)calloc((1+len),sizeof(utf32));
#if 0
    if(dup != NULL) {
        utf8* dst = dup;
        while((*dst++ = *src++));
    }
#else
    if(dup != NULL)
 	memcpy(dup,src,(len+1)*sizeof(utf32));
#endif
    return dup;
}

static int
strcmp32(const utf8* s1, const utf8* s2)
{
    while(*s1 && *s2 && *s1 == *s2) {s1++; s2++;}
    /* Look at the last char to decide */
    if(*s1 == *s2) return 0; /* completely identical */
    if(*s1 == NUL8) /*=>*s2!=NUL8*/ return -1; /* s1 is shorter than s2 */
    if(*s2 == NUL8) /*=>*s1!=NUL8*/ return +1; /* s1 is longer than s2 */
    return (*s1 < *s2 ? -1 : +1);
}

static int
strncmp32(const utf8* s1, const utf8* s2, unsigned len)
{
    utf32 c1=0;
    utf32 c2=0;
    if(len == 0) return 0; /* null strings always equal */
    for(;len>0;len--) {/* compare thru the last character*/
        c1 = *s1++;
        c2 = *s2++;
        if(c1 != c2) break; /* mismatch before length characters tested*/
    }
    /* Look at the last char to decide */
    if(c1 == c2) return 0; /* completely identical */
    if(c1 == NUL8) /*=>c2!=NUL8*/ return -1; /* s1 is shorter than s2 */
    if(c2 == NUL8) /*=>c1!=NUL8*/ return +1; /* s1 is longer than s2 */
    return (c1 < c2 ? -1 : +1);
}


static void
memcpy32(utf8* dst, const utf8* src, int len)
{
    memcpy((void*)dst,(void*)src,len*sizeof(utf32));
}

/**************************************************/
/* Manage utf32 versus char_t */

/* Test equality of pure ascii string to utf32 string */
static int
streq32ascii(utf8* s32, char* s)
{
    while(*s && *s32) {
	if(*s != *s32) return 0; /* not equal */
	s++;
	s32++;
    }
    if(*s == *s32) return 1; /* equal */
    return 0;
}

/**
Convert a string of utf32 characters to a string of char_t
characters.  Stop when srclen characters are processed or
end-of-string is encountered, or dstlen dst characters are processed,
whichever comes first.
WARNING: result is not nul-terminated.
Return: -1 if error, # of dst chars produced otherwise.
*/

static int
toString8(char_t* dst, utf8* src, int srclen, int dstlen)
{
    utf8* p32;
    char_t* q8;
    int i;
    int avail = dstlen;

    p32 = src;
    q8 = dst;
    for(i=0;i<srclen;i++) {
        int count;
        utf32 c=*p32++;
        if(c == NUL8) break;
        count = toChar8(q8,c);
        if(count == 0) return -1;
	avail -= count;
	if(avail < 0)
	    return -1;
        q8 += count;
    }
    return (q8 - dst);
}

/**
Convert a string of char_t characters to a string of utf32
characters.  Stop when len src bytes are processed or
end-of-string is encountered, whichever comes first.
WARNING: src length is in bytes, not characters.
WARNING: result is not nul-terminated.
Return: -1 if error, # of dst chars produced otherwise.
*/
static int
toString32(utf8* dst, char_t* src, int len)
{
    utf8* q32;
    char_t* p8;
    int i,count;

    p8 = src;
    q32 = dst;
    for(count=0,i=0;i<len;i+=count) {
        char_t c = *p8;
        if(c == NUL) break;
        count = toChar32(q32, p8);
        if(count == 0) return -1;
        q32++;
        p8 += count;
    }
    return (q32 - dst);
}

/**
Convert a utf32 value to a sequence of char_t bytes.
Return -1 if invalid; # chars in char_t* dst otherwise.
*/
static int
toChar8(char_t* dst, utf32 codepoint)
{
    char_t* p = dst;
    unsigned char uchar;
    /* assert |dst| >= MAXCP8SIZE+1 */
    if(codepoint <= 0x7F) {
        uchar = (unsigned char)codepoint;
       *p++ = (char)uchar;
    } else if(codepoint <= 0x7FF) {
       uchar = (unsigned char) 0xC0 | (codepoint >> 6);
       *p++ = (char)uchar;
       uchar = (unsigned char) 0x80 | (codepoint & 0x3F);
       *p++ = (char)uchar;
     }
     else if(codepoint <= 0xFFFF) {
       uchar = (unsigned char) 0xE0 | (codepoint >> 12);
       *p++ = (char)uchar;
       uchar = (unsigned char) 0x80 | ((codepoint >> 6) & 0x3F);
       *p++ = (char)uchar;
       uchar = (unsigned char) 0x80 | (codepoint & 0x3F);
       *p++ = (char)uchar;
     }
     else if(codepoint <= 0x10FFFF) {
       uchar = (unsigned char) 0xF0 | (codepoint >> 18);
       *p++ = (char)uchar;
       uchar = (unsigned char) 0x80 | ((codepoint >> 12) & 0x3F);
       *p++ = (char)uchar;
       uchar = (unsigned char) 0x80 | ((codepoint >> 6) & 0x3F);
       *p++ = (char)uchar;
       uchar = (unsigned char) 0x80 | (codepoint & 0x3F);
       *p++ = (char)uchar;
     }
     else
        return -1; /*invalid*/
    *p = NUL; /* make sure it is nul terminated. */
    return (p - dst);
}

/**
Convert a char_t multibyte code to utf32;
return -1 if invalid, #bytes processed from src otherwise.
*/
static int
toChar32(utf8* codepointp, char_t* src)
{
    /* assert |src| >= MAXCP8SIZE; not necessarily null terminated */
    char_t* p;
    utf32 codepoint;
    int charsize;
    unsigned c0;
    static int mask[5] = {0x00,0x7F,0x1F,0x0F,0x07};
    unsigned char bytes[4];
    int i;

    p = src;
    c0 = (unsigned )*p;
    charsize = u8size(c0);
    if(charsize == 0) return -1; /* invalid */
    /* Process the 1st char in the char_t codepoint */
    bytes[0] = (c0 & mask[charsize]);
    for(i=1;i<charsize;i++) {
        unsigned char c;
        p++;
        c = *p;
        bytes[i] = (c & 0x3F);
    }
    codepoint = (bytes[0]);
    for(i=1;i<charsize;i++) {
        unsigned char c = bytes[i];
        codepoint <<= 6;
        codepoint |= (c & 0x3F);
    }
    if(codepointp) *codepointp = codepoint;
    return 1;    
}
#endif /*0*/

static int
u8size(utf8 c)
{
    if(((c) == SEGMARK) return 2; /* segment|create mark */
    if(((c) & 0x80) == 0x00) return 1; /* us-ascii */
    if(((c) & 0xE0) == 0xC0) return 2;
    if(((c) & 0xF0) == 0xE0) return 3;
    if(((c) & 0xF8) == 0xF0) return 4;
    return 0;
}

static const utf8cp
ascii2u8(char c)
{
    static utf8cpu u8;
    u8.cp = empty_u8cp;
    u8.cpa[0] = (char)(((unsigned)c) | 0x7F);
    return u8.cp;
}

static int
u8equal(utf8* c1, utf8* c2)
{
    int l1 = u8size(c1);
    int l2 = u8size(c2);
    if(l1 != l2) return 0;
    if(memcmp((void*)c1,(void*)c2,l1)!=0) return 0;
    return 1;
}

/**
Convert a utfcpa codepoint as array to codepoint as unsigned integer.
@param c8a pointer to a 1-4 byte codepoint array
@return a 32 bit unsigned integer computed from c8 via a union.
*/
static utf8cp
u8cp(utf8cpa c8a)
{
    utf8cpu u8;
    u8.cp = empty_cp;
    memcpy((void*)u8.cpa,(void*)c8a,u8size(c8));
    return u8.cp;
}

/**
Convert a utfcp codepoint as integer to codepoint as array
@param dst pointer to a 1-4 byte codepoint array
@param src 32 bit unsigned integer codepoint
@return void
*/
static void
u8cpa(utf8cpa dst, utfcp src)
{
    utf8cpu u8;
    u8.cp = src;
    memcpy((void*)dst,(void)u8.cpa,u8size(u8.cpa));
}

/**
Copy a single codepoint from src to dst
@param dst target for the codepoint
@param src src for the codepoint
@return no. of bytes in codepoint
*/
static int
memcpycp(utf8cpa dst, utf8cpa src)
{
    int count = u8size(src);
    memcpy(dst,src,count);
    return count;    
}

/**
Copy a single codepoint from src to dst
and increment src and dst by size of codepoint.
@param dst target for the codepoint
@param src src for the codepoint
@return no. of bytes in codepoint
*/
static int
copycpincr(utf8cpa* dstp, utf8cpa* srcp)
{
    int count = u8size(*srcp);
    memcpy(*dstp,*srcp,count);
    *srcp += count;
    *dstp += count;
    return count;    
}

static utf8*
u8ith(utf8* base, size_t n)
{
   utf8* p = base;
   while(n-- > 0 && !isnul(p)) p += u8size(p);
   return p;
}

/**
Given a utf8 pointer, backup one codepoint.
This is doable because we can recognize the
start of a codepoint.
Special case required for segment mark.
@param p0 pointer pointer to increment
@return pointer to codepoint after p0
*/
static utf8*
u8incr(utf8** p0)
{
    *p0 += u8size(*p0);
    return *p0;
}

/**
Given a utf8 pointer, backup one codepoint.
This is doable because we can recognize the
start of a codepoint.
Special case required when backing up over a segment mark
since it assumes that the raw SEGMARK is at the beginning
of the mark.
@param p0 pointer to back up
@return pointer to codepoint before p0
*/
static utf8*
u8decr(utf8* p0)
{
    size_t count 0;
    utf8* p = p0;
    while(((*p & 0xB0) == 0xB0) && (*p != SEGMARK)) p--; /* back up over continuation bytes unless pure SEGMARK */
    return p;
}

/**************************************************/
/* Character Input/Output */ 

/* Unless you know that you are outputing ASCII,
   all output should go thru this procedure.
*/
static void
fputc32(utf32 c32, FILE* f)
{
    char_t c8[MAXCP8SIZE+1];
    int count,i;
    if((count = toChar8(c8,c32)) < 0) fail(NOTTM,TTM_EUTF32);
    for(i=0;i<count;i++) {fputc(c8[i],f);}
}

/* All reading of characters should go thru this procedure. */
static utf32
fgetc32(FILE* f)
{
    int c;
    utf32 c32;
        
    c = fgetc(f);
    if(c == EOF) return EOF;
    if(ismultibyte(c)) {
        char_t c8[MAXCP8SIZE+1];
        int count,i;
        count = u8size(c);
        i=0; c8[i++] = (char_t)c;
        while(--count > 0) {
            c=fgetc(f);
            if(c == EOF) fail(NOTTM,TTM_EEOS);
            c8[i++] = (char_t)c;
        }
        c8[i] = NUL;
        count = toChar32(&c32,c8);
        if(count < 0) fail(NOTTM,TTM_ECHAR8);
    } else
        c32 = c;
    return c32;
}

static void
fputc32(utf32 c32, FILE* f)
{
    char_t c8[MAXCP8SIZE+1];
    int count,i;
    count = toChar8(c8,c32);
    for(i=0;i<count;i++) fputc(c8[i],f);
}

/* All reading of characters should go thru this procedure. */
static utf32
fgetc32(FILE* f)
{
    int c = fgetc(f);
    return c;
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
 
        /* The Unix epoch starts on Jan 1 1970.  Need to subtract the difference 
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
        next++;     /* skip past - */
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

