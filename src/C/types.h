/**************************************************/
/* UTF and char Definitions */

/**
We assume that all strings/characters in the external work use utf-8.
This includes at least:
1. files
2. libc

We use the "char" type to indicate the use of external
string/characters.  We also use "char" for utf8.  Techically, char is
a signed chara and utf8 is a unsigned char.  This means that
occasionally some signed<->unsigned casting is required.  Note that,
unless otherwise specified, lengths are in units of bytes, not utf8
codepoints.

Note also that it is assumed that all C string constants
in this file are restricted to US-ASCII, which is a subset
of UTF-8.
*/

/* Maximum codepoint size when using utf8 */
#define MAXCP8SIZE 4

/* Note that we could use char instead of utf8, but
   setting up a typedef helps to clarify the code
   about when using codepoints is important.
*/
typedef unsigned char utf8; /* 8-bit utf char type. */

/* Array for holding a single codepoint; should never be used as C function arg or return value */
typedef char utf8cpa[MAXCP8SIZE];
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

/* Trace state */
typedef enum TRACE {
TR_UNDEF=-1,
TR_OFF=0,
TR_ON=1,
} TRACE;

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
TTM_EBADCALL		= (-118),  /* Malformed function call */

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
/**
Structure Type declarations
*/

typedef struct TTM TTM;
typedef struct Function Function;
typedef struct Charclass Charclass;
typedef struct Property Property;
typedef struct Frame Frame;
typedef struct VArray VArray;
typedef VArray VList;
typedef VArray VString;

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
    char* name;
    unsigned hash;
    struct HashEntry* next;
};

/* Note that the ith entry is actually only used to provide
   a place to store the pointer to the first real entry.
*/
struct HashTable {
    size_t nentries; /* convenience: track no. of entries in table */
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
  char* argv[MAXARGS+1]; /* Allow for final NULL arg as signal; not counted in argc */
  size_t argc;
  int active; /* 1 => # 0 => ## */
  VString* result; /* Dual duty: (1) collect each arg in turn and (2) collect function call result */
};

/**************************************************/
typedef struct TTMFILE {
    char* name;
    size_t fileno; /* WRT ttm->io.allfiles */
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
    struct Debug {
	/*Debug Flags */
	TRACE trace;   /* Forcibly trace all function executions */
	int debug; /* output debug info */
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
	int starting; /* Are we doing startup?*/
	int catchdepth;
    } flags;
    struct OPTS { /* non-debug options */
        int quiet;
	int bare;
	int verbose;
	char* programfilename;
    } opts;
    struct MetaChars {
	utf8cpa sharpc;  /* sharp char */
	utf8cpa semic;   /* semicolon char */
	utf8cpa escapec; /* TTM escape char */
	utf8cpa metac;   /* read meta char; same as end of line char */
	utf8cpa openc;   /* '<' char */
	utf8cpa closec;  /* '>' char */
	utf8cpa lbrc;    /* escaped string bracket open char (==openc) */
	utf8cpa rbrc;    /* escaped string bracket close char (==closec) */
    } meta;
    struct Buffers {
	VString* active; /* string being processed */
	VString* passive; /* already processed part of active */
	VString* tmp; /* misc text */
	VString* result; /* staging area before insertion into active or passive */
    } vs;
    struct FrameStack {
	int top; /* |stack| == (top) */
	Frame stack[MAXFRAMEDEPTH];
    } frames;
    struct IO {
	/* stdin, stdout, and stderr are the unix equivalent */
	TTMFILE* _stdin;
	TTMFILE* _stdout;
	TTMFILE* _stderr;
	TTMFILE* allfiles[MAXOPENFILES]; /* vector of all open files */
    } io;
    /* Following 2 fields are hashtables indexed by low order 7 bits of some character */
    struct Tables {
	struct HashTable dictionary;
	struct HashTable charclasses;
	struct HashTable properties;
    } tables;
    /* TTM Execution Properties; These must be kept consistent with property table entries */
    struct Properties { /* WARN: reflect changes to PropEnum and its uses */
	size_t stacksize;
	size_t execcount;
	size_t showfinal; /* 1=>print contents of passive buffer after scan() finishes; 0=>suppress */
	size_t showcall; /* 1=>print contents of passive buffer after each function call; 0=>suppress */
    } properties;
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
	TRACE trace;
	int locked;
	int builtin;
	size_t minargs;
	size_t maxargs;
	enum FcnSV sv;
	int novalue; /* suppress return value */
	size_t nextsegindex; /* highest segment index number in use in this string */
	TTMFCN fcn; /* builtin == 1 */
	VString* body; /* builtin == NULL; body.index is the residual */
    } fcn;
};

/**
Character Class type
*/

struct Charclass {
    struct HashEntry entry;
    char* characters;
    int negative;
};

/**
Property type
*/

struct Property {
    struct HashEntry entry; /* key is entry->name */
    char* value;
};

/* Current enum of predefined properties */
enum PropEnum {
PE_UNDEF=0,
PE_STACKSIZE,
PE_EXECCOUNT,
PE_SHOWFINAL,
PE_SHOWCALL, /* Show passive output from each function result */
};


enum TableType {TT_DICT, TT_CLASSES, TT_PROPS};
