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
#define nulldup(x) ((x)?strdup(x):(x))
#define UNUSED(x) (void)x

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

/* Must be powers of two; simulated enum */
typedef unsigned IOMODE;
#define IOM_UNDEF  0
#define IOM_READ   1
#define IOM_WRITE  2
#define IOM_APPEND 4

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
TTM_EACCESS		= (-117),  /* File not accessible or wrong mode */

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

#define segmarkindex(cp) ((utf8)(*((cp)+1) & SEGMARKINDEXUNMASK))
#define segmarkindexbyte(index) ((utf8)(((index)&SEGMARKINDEXUNMASK)|SEGMARKINDEXMASK))
#define issegmark(cp) ((*(cp) == SEGMARK0)?1:0)
#define iscreateindex(idx) (((idx) == CREATEINDEXONLY)?1:0)
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

typedef TTMERR (*TTMFCN)(TTM*, Frame*, VString*);

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

/**************************************************/
/**
  Define a ttm frame
*/

struct Frame {
  utf8* argv[MAXARGS+1]; /* Allow for final NULL arg as signal; not counted in argc */
  size_t argc;
  int active; /* 1 => # 0 => ## */
  VString* result; /* Dual duty: (1) collect each arg in turn and (2) collect function call result */
};

/**************************************************/
typedef struct TTMFILE {
    char* name;
    IOMODE mode;
    FILE* file;
    int isstd; /* => do not close this file */
    /* Provide a stack of pushed codepoints */
    int npushed; /* number of pushed codepoints 0..(MAXPUSHBACK_*/
    utf8cpa stack[MAXPUSHBACK]; /* support pushback of several full codepoints */
} TTMFILE;

/**************************************************/
/**
TTM state object
*/

struct TTM {
    /* TTM Execution Properties */
    struct Properties { /* WARN: reflect changes to PropEnum and its uses */
	int stacksize;
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
	int exit; /* Stop scanning, cleanup, and exit ttm */
	unsigned exitcode;
	unsigned crcounter; /* for cr marks */
	int lineno; /* Estimate of current line no */
    } flags;
    struct OPTS { /* non-debug options */
        int quiet;
	int bare;
	char* programfilename;
    } opts;
    struct MetaChars {
	utf8cpa sharpc; /* sharp-like char */
	utf8cpa semic; /* ;-like char */
	utf8cpa escapec; /* escape-like char */
	utf8cpa metac; /* read meta (end of line) char */
	utf8cpa openc; /* <-like char */
	utf8cpa closec; /* >-like char */
	utf8cpa lbrc; /* escaped string bracket open char */
	utf8cpa rbrc; /* escaped string bracket close char */
    } meta;
    struct Buffers {
	VString* active; /* string being processed */
	VString* passive; /* already processed part of active */
	VString* tmp; /* misc text */
    } vs;
    struct FrameStack {
	int top; /* |stack| == (top) */
	Frame stack[MAXFRAMEDEPTH];
    } frames;
    struct IO {
	/* stdin, stdout, and stderr are the unix equivalent */
	TTMFILE* stdin;
	TTMFILE* stdout;
	TTMFILE* stderr;
    } io;
    /* Following 2 fields are hashtables indexed by low order 7 bits of some character */
    struct Tables {
	struct HashTable dictionary;
	struct HashTable charclasses;
    } tables;
};

/* Convenience */
typedef struct TTMFILE TTMFILE;

/**************************************************/

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
	size_t nextsegindex; /* highest segment index number in use in this string */
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

