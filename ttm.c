/**
This software is released under the terms of the Apache License version 2.
For details of the license, see http://www.apache.org/licenses/LICENSE-2.0.
*/

/**************************************************/
/**
(Un)Define these if you have/have-not the specified capability.
This is in lieu of the typical config.h.
*/

#define HAVE_MEMMOVE 

/**************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

/**************************************************/
/* Unix/Linux versus Windows Definitions */

/* snprintf */
#ifdef _MSC_VER
#define snprintf _snprintf /* Microsoft has different name for snprintf */
#endif /*!_MSC_VER*/

/* Getopt */
#ifdef _MSC_VER
static char* optarg;		// global argument pointer
static int   optind = 0; 	// global argv index
static int getopt(int argc, char *argv[], char *optstring)
#else /*!_MSC_VER*/
#include <unistd.h> /* This defines getopt */
#endif /*!_MSC_VER*/

/* Wrap both unix and windows timing in this function */
static unsigned long long getRunTime(void);

#ifdef _MSC_VER
#include <windows.h>  /* To get GetProcessTimes() */
#else /*!_MSC_VER*/
#include <unistd.h> /* This defines getopt */
#include <sys/times.h> /* to get times() */
#endif /*!_MSC_VER*/

/**************************************************/

static void
makespace(char* dst, char* src, unsigned long len)
{
#ifdef HAVE_MEMMOVE
    memmove((void*)dst,(void*)src,len);
#else	
    src += len;
    dst += len;
    while(len-- > 0) *dst-- = *src--;
#endif
}

/**************************************************/
/**
Constants
*/

#define SEGMENT ((char)0xff) /* Not a legal value in utf-8 and not NUL */
#define CREATEMARK ((char)0xfe) /* Not a legal value in utf-8 and not NUL */

#define UNIVERSAL_ESCAPE SEGMENT

#define MAXMARKS 62
#define MAXARGS 63
#define MAXINCLUDES 1024
#define MAXEOPTIONS 1024

#define NUL '\0'
#define LPAREN '('
#define RPAREN ')'
#define COMMA ','

#define MINBUFFERSIZE 1024
#define MINSTACKSIZE 1024

#define CREATELEN 4 /* # of characters for a create mark */

/*Mnemonics*/
#define NESTED 1
#define NOTTM NULL

/* Debug Flags */

#define FLAG_EXIT 1
#define FLAG_TRACE 2

/**************************************************/
/* Error Numbers */
typedef enum ERR {
ENOERR,   /* No error; for completeness */
ENONAME, /* String or Character Class Name Not Found */
ENOPRIM, /* Primitives Not Allowed */
EFEWPARMS, /* Too Few Parameters Given */
EFORMAT, /* Incorrect Format */
EQUOTIENT, /* Quotient Is Too Large */
EDECIMAL, /* Decimal Integer Required */
EMANYDIGITS, /* Too Many Digits */
EMANYSEGMARKS, /* Too Many Segment Marks */
EMEMORY, /* Dynamic Storage Overflow */
EPARMROLL, /* Parm Roll Overflow */
EINPUTROLL, /* Input Roll Overflow */
#ifdef IMPLEMENTED
EDUPLIBNAME, /* Name Already On Library */
ELIBNAME, /* Name Not On Library */
ELIBSPACE, /* No Space On Library */
EINITIALS, /* Initials Not Allowed */
EATTACH, /* Could Not Attach */
#endif
EIO, /* An I/O Error Occurred */
#ifdef IMPLEMENTED
ETTM, /* A TTM Processing Error Occurred */
ESTORAGE, /* Error In Storage Format */
#endif
/* Error messages new to this implementation */
ESTACKOVERFLOW,
ESTACKUNDERFLOW,
EBUFFERSIZE, /* Buffer overflow */
EMANYINCLUDES, /* Too many includes */
EINCLUDE, /* Cannot read Include file */
ERANGE, /* index out of legal range */
EMANYPARMS, /* # parameters > MAXARGS */
EEOS, /* Unexpected end of string */
/* Default case */
EOTHER
} ERR;

static char* errmsgs[] = {
"No error",
"String or Character Class Name Not Found"
"Primitives Not Allowed",
"Too Few Parameters Given",
"Incorrect Format",
"Quotient Is Too Large",
"Decimal Integer Required",
"Too Many Digits",
"Too Many Segment Marks",
"Dynamic Storage Overflow",
"Parm Roll Overflow",
"Input Roll Overflow",
#ifdef IMPLEMENTED
"Name Already On Library",
"Name Not On Library",
"No Space On Library",
"Initials Not Allowed",
"Could Not Attach",
#endif
"An I/O Error Occurred",
#ifdef IMPLEMENTED
"A TTM Processing Error Occurred",
"Error In Storage Format",
#endif
/* Error messages new to this implementation */
"Stack Overflow",
"Stack Underflow",
"Buffer overflow",
"Too many includes",
"Cannot read include file",
"Index out of legal range",
"Too many parameters",
"Unexpected end of string",
"Unknown Error"
};

/**************************************************/
/* "inline" functions */

#define isescape(c)((c) == ttm->escapec || (c) == UNIVERSAL_ESCAPE)

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
typedef struct String String;
typedef struct Charclass Charclass;
typedef struct Frame Frame;
typedef struct Buffer Buffer;

typedef void (*TTMFCN)(TTM*, Frame*);

/**
TTM state object
*/

struct TTM {
    struct Limits {
        unsigned long buffersize;
        unsigned long stacksize;
    } limits;
    unsigned long flags;
    unsigned long exitcode;
    unsigned long crcounter; /* for cr marks */
    int sharpc; /* sharp-like char */
    int openc; /* <-like char */
    int closec; /* >-like char */
    int semic; /* ;-like char */
    int escapec; /* escape-like char */
    int metac; /* read eof char */
    Buffer* buffer; /* contains the string being processed */
    Buffer* result; /* contains result strings from functions */
    String* dictionary[256]; /* indexed by str->name[0] */
    Charclass* charclasses[256]; /* indexed by cl->name[0] */
    int stacknext; /* |stack| == (stacknext) */
    Frame* stack;    
    FILE* output;    
};

/**
Define a fixed size byte buffer
for holding the current state of the expansion.
Buffer always has an extra terminating NUL ('\0').
Note that by using a buffer that is allocated once,
we can use pointers into the buffer space in e.g. struct String.
 */

struct Buffer {
    unsigned long alloc;  /* including trailing NUL */
    unsigned long length;  /* including trailing NUL; defines what of
                             the allocated space is actual content. */
    char* active; /* characters yet to be scanned */
    char* passive; /* characters that will never be scanned again */
    char* end; /* into content */
    char* content;
};

/**
  Define a ttm frame
*/

struct Frame {
  char* argv[MAXARGS+1];
  int argc;
  int active; /* 1 => # 0 => ## */
};

/**
String Storage and the Dictionary
*/

struct String {
    char* name;
    int builtin;
    int minargs;
    int residual; /* residual "pointer" (offset really) */
    TTMFCN fcn; /* builtin == 1 */
    char* body; /* builtin == 0 */
    String* next; /* "hash" chain */
};

/**
Character Classes  and the Charclass table
*/

struct Charclass {
    char* name;
    char* characters;
    int negative;
    Charclass* next;
};

/**************************************************/
/* Forward */

static TTM* newTTM(unsigned long buffersize, unsigned long stacksize);
static void freeTTM(TTM* ttm);
static Buffer* newBuffer(TTM* ttm, unsigned long buffersize);
static void freeBuffer(TTM* ttm, Buffer* bb);
static void expandBuffer(TTM* ttm, Buffer* bb, unsigned long len);
static void resetBuffer(TTM* ttm, Buffer* bb);
static void setBufferLength(TTM* ttm, Buffer* bb, unsigned long len);
static Frame* pushFrame(TTM* ttm);
static Frame* popFrame(TTM* ttm);
static String* newString(TTM* ttm);
static void freeString(TTM* ttm, String* f);
static void dictionaryInsert(TTM* ttm, String* str);
static String* dictionaryLookup(TTM* ttm, char* name);
static String* dictionaryRemove(TTM* ttm, char* name);
static Charclass* newCharclass(TTM* ttm);
static void freeCharclass(TTM* ttm, Charclass* cl);
static void charclassInsert(TTM* ttm, Charclass* cl);
static Charclass* charclassLookup(TTM* ttm, char* name);
static Charclass* charclassRemove(TTM* ttm, char* name);
static int charclassMatch(char* s, char* charclass);
static void scan(TTM* ttm);
static void exec(TTM* ttm, Buffer* bb);
static void parsecall(TTM* ttm, Frame* frame);
static char* call(TTM* ttm, Frame* frame, char* body);
static void printstring(TTM* ttm, FILE* output, char* s);
static void ttm_ap(TTM* ttm, Frame* frame) /* Append to a string */;
static void ttm_cf(TTM* ttm, Frame* frame) /* Copy a function */;
static void ttm_cr(TTM* ttm, Frame* frame) /* Mark for creation */;
static void ttm_ds(TTM* ttm, Frame* frame);
static void ttm_es(TTM* ttm, Frame* frame) /* Erase string */;
static int ttm_ss0(TTM* ttm, Frame* frame);
static void ttm_sc(TTM* ttm, Frame* frame) /* Segment and count */;
static void ttm_ss(TTM* ttm, Frame* frame) /* Segment and count */;
static void ttm_cc(TTM* ttm, Frame* frame) /* Call one character */;
static void ttm_cn(TTM* ttm, Frame* frame) /* Call n characters */;
static void ttm_cp(TTM* ttm, Frame* frame) /* Call parameter */;
static void ttm_cs(TTM* ttm, Frame* frame) /* Call segment */;
static void ttm_isc(TTM* ttm, Frame* frame) /* Initial character scan */;
static void ttm_rrp(TTM* ttm, Frame* frame) /* Reset residual pointer */;
static void ttm_scn(TTM* ttm, Frame* frame) /* Character scan */;
static void ttm_gn(TTM* ttm, Frame* frame) /* Give n characters */;
static void ttm_zlc(TTM* ttm, Frame* frame) /* Zero-level commas */;
static void ttm_zlcp(TTM* ttm, Frame* frame) /* Zero-level commas and parentheses */;
static void ttm_ccl(TTM* ttm, Frame* frame) /* Call class */;
static void ttm_dcl0(TTM* ttm, Frame* frame, int negative);
static void ttm_dcl(TTM* ttm, Frame* frame) /* Define a negative class */;
static void ttm_dncl(TTM* ttm, Frame* frame) /* Define a negative class */;
static void ttm_ecl(TTM* ttm, Frame* frame) /* Erase a class */;
static void ttm_scl(TTM* ttm, Frame* frame) /* Skip class */;
static void ttm_tcl(TTM* ttm, Frame* frame) /* Test class */;
static void ttm_abs(TTM* ttm, Frame* frame) /* Obtain absolute value */;
static void ttm_ad(TTM* ttm, Frame* frame) /* Add */;
static void ttm_dv(TTM* ttm, Frame* frame) /* Divide and give quotient */;
static void ttm_dvr(TTM* ttm, Frame* frame) /* Divide and give remainder */;
static void ttm_mu(TTM* ttm, Frame* frame) /* Multiply */;
static void ttm_su(TTM* ttm, Frame* frame) /* Substract */;
static void ttm_eq(TTM* ttm, Frame* frame) /* Compare numeric equal */;
static void ttm_gt(TTM* ttm, Frame* frame) /* Compare numeric greater-than */;
static void ttm_lt(TTM* ttm, Frame* frame) /* Compare numeric less-than */;
static void ttm_eql(TTM* ttm, Frame* frame) /* ? Compare logical equal */;
static void ttm_gtl(TTM* ttm, Frame* frame) /* ? Compare logical greater-than */;
static void ttm_ltl(TTM* ttm, Frame* frame) /* ? Compare logical less-than */;
static void ttm_ps(TTM* ttm, Frame* frame) /* Print a String */;
static void ttm_psr(TTM* ttm, Frame* frame) /* Print String and Read */;
static void ttm_rs(TTM* ttm, Frame* frame) /* Read a String */;
static void ttm_pserr(TTM* ttm, Frame* frame) /* Print a String to stderr */;
static void ttm_cm(TTM* ttm, Frame* frame) /* Change meta character */;
static void ttm_names(TTM* ttm, Frame* frame) /* Obtain String Names */;
static void ttm_exit(TTM* ttm, Frame* frame) /* Return from TTM */;
static void ttm_ndf(TTM* ttm, Frame* frame) /* Determine if a Name is Defined */;
static void ttm_norm(TTM* ttm, Frame* frame) /* Obtain the Norm of a String */;
static void ttm_time(TTM* ttm, Frame* frame) /* Obtain Execution Time */;
static void ttm_tf(TTM* ttm, Frame* frame) /* Turn Trace Off */;
static void ttm_tn(TTM* ttm, Frame* frame) /* Turn Trace On */;
static void ttm_argv(TTM* ttm, Frame* frame) /* Get ith command line argument */;
static void ttm_include(TTM* ttm, Frame* frame)  /* Include text of a file */;
static void fail(TTM* ttm, ERR eno);
static void fatal(TTM* ttm, const char* msg);
static ERR toInt64(char* s, long long* lp);
static int utf8count(int c);
static int convertEscapeChar(int c);
static void trace(TTM* ttm, int entering, int dumping);
static void trace1(TTM* ttm, int depth, int entering, int dumping);
static void dumpstack(TTM* ttm);
static int getOptionStringLength(char** list);
static int pushOptionString(char* option, int max, char** list);
static void initglobals();
static void usage(void);
static void convertDtoE(const char* def);
static void readinput(TTM* ttm, const char* filename,Buffer* bb);
static int readbalanced(TTM* ttm);
static void printbuffer(TTM* ttm);
static int readfile(TTM* ttm, FILE* file, Buffer* bb);

/**************************************************/
/* Global variables */

static char* includes[MAXINCLUDES+1]; /* null terminated */
static char* eoptions[MAXEOPTIONS+1]; /* null terminated */
static char* argoptions[MAXARGS+1]; /* null terminated */

/**************************************************/
static TTM*
newTTM(unsigned long buffersize, unsigned long stacksize)
{
    TTM* ttm = (TTM*)calloc(1,sizeof(TTM));
    if(ttm == NULL) return NULL;
    ttm->limits.buffersize = buffersize;
    ttm->limits.stacksize = stacksize;
    ttm->sharpc = '#';
    ttm->openc = '<';
    ttm->closec = '>';
    ttm->semic = ';';
    ttm->escapec = '@';
    ttm->metac = EOF;
    ttm->buffer = newBuffer(ttm,buffersize);
    ttm->result = newBuffer(ttm,buffersize);
    ttm->stacknext = 0;
    ttm->stack = (Frame*)malloc(sizeof(Frame)*stacksize);
    if(ttm->stack == NULL) fail(ttm,EMEMORY);
    return ttm;
}

static void
freeTTM(TTM* ttm)
{
    freeBuffer(ttm,ttm->buffer);
    freeBuffer(ttm,ttm->result);
    if(ttm->stack != NULL)
	free(ttm->stack);
    free(ttm);
}

/**************************************************/

static Buffer*
newBuffer(TTM* ttm, unsigned long buffersize)
{
    Buffer* bb;
    bb = (Buffer*)calloc(1,sizeof(Buffer));
    if(bb == NULL) fail(ttm,EMEMORY);
    bb->content = (char*)malloc(buffersize);
    if(bb->content == NULL) fail(ttm,EMEMORY);
    bb->alloc = buffersize;
    bb->length = 0;
    bb->active = bb->content;
    bb->end = bb->active;
    return bb;
}

static void
freeBuffer(TTM* ttm, Buffer* bb)
{
    if(bb->content != NULL)
	free(bb->content);
    free(bb);
}

/* Make room for a string of length n at current position. */
static void
expandBuffer(TTM* ttm, Buffer* bb, unsigned long len)
{
    assert(bb != NULL);
    if((bb->alloc - bb->length) < len) fail(ttm,EBUFFERSIZE);
    if(bb->active < bb->end) {
        /* make room for len characters by moving bb->active and up*/
	makespace(bb->active+len,bb->active,len);
    }
    bb->active += len;
    bb->length += len;
    bb->end = bb->content+bb->length;
    *(bb->end) = NUL;
}

#if 0
/* Remove len characters at current position */
static void
compressBuffer(TTM* ttm, Buffer* bb, unsigned long len)
{
    assert(bb != NULL);
    if(len > 0 && bb->active < bb->end) {
	memcpy(bb->active,bb->active+len,len);	
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
}

/* Change the buffer length without disturbing
   any existing content (unless shortening)
   If space is added, its content is undefined.
*/
static void
setBufferLength(TTM* ttm, Buffer* bb, unsigned long len)
{
    if(len >= bb->alloc) fail(ttm,EBUFFERSIZE);
    bb->length = len;
    bb->end = bb->content+bb->length;
    *(bb->end) = NUL; /* make sure */    
}

/**************************************************/

/* Manage the frame stack */
static Frame*
pushFrame(TTM* ttm)
{
    Frame* frame;
    if(ttm->stacknext >= ttm->limits.stacksize)
	fail(ttm,ESTACKOVERFLOW);
    frame = &ttm->stack[ttm->stacknext];
    frame->argc = 0;
    frame->active = 0;
    ttm->stacknext++;
    return frame;
}

static Frame*
popFrame(TTM* ttm)
{
    Frame* frame;
    if(ttm->stacknext == 0)
	fail(ttm,ESTACKUNDERFLOW);
    ttm->stacknext--;
    if(ttm->stacknext == 0)
	frame = NULL;
    else
	frame = &ttm->stack[ttm->stacknext-1];    
    return frame;
}

/**************************************************/
static String*
newString(TTM* ttm)
{
    String* str = (String*)calloc(1,sizeof(String));
    if(str == NULL) fail(ttm,EMEMORY);
    return str;    
}

static void
freeString(TTM* ttm, String* f)
{
    assert(f != NULL);
    if(f->name != NULL) free(f->name);
    if(!f->builtin && f->body != NULL) free(f->body);
    free(f);
}

/**
Dictionary Management.  The string dictionary is
simplified by making it a array of chains indexed by the
first char of the name of the string.
*/

static void
dictionaryInsert(TTM* ttm, String* str)
{
    String** table = ttm->dictionary;
    String* next;
    int c0 = str->name[0];

    next = table[c0];
    if(next == NULL) {
	table[c0] = str;
    } else {
	String* prev = NULL;
	/* search chain for str */
        while(next != NULL) {
	    if(strcmp(str->name,next->name)==0)
		break;
	    prev = next;
	    next = next->next;
	}
	if(next == NULL) {/* str not previously defined */
	    prev->next = str;
	} else {/* replace existing definition */
	    String* chain = next->next; /* save */
	    freeString(ttm,next);
	    prev->next = str;
	    str->next = chain;
	}
    }
}

static String*
dictionaryLookup(TTM* ttm, char* name)
{
    String** table = ttm->dictionary;
    String* f;
    int c0;
    assert(name != NULL);
    c0 = name[0];
    f = table[c0];
    while(f != NULL) {/* search chain for str */
	if(strcmp(name,f->name)==0)
	    break;
	f = f->next;
    }
    return f;
}

static String*
dictionaryRemove(TTM* ttm, char* name)
{
    String** table = ttm->dictionary;
    String* next;
    String* prev;
    int c0;
    assert(name != NULL);
    c0 = name[0];
    next = table[c0];
    prev = NULL;
    while(next != NULL) {/* search chain for str */
	if(strcmp(name,next->name)==0) {
	    prev->next = next->next;
	    break;
	}
	prev = next;
	next = next->next;
    }
    return next;
}

/**************************************************/
static Charclass*
newCharclass(TTM* ttm)
{
    Charclass* cl = (Charclass*)calloc(1,sizeof(Charclass));
    if(cl == NULL) fail(ttm,EMEMORY);
    return cl;
}

static void
freeCharclass(TTM* ttm, Charclass* cl)
{
    assert(cl != NULL);
    if(cl->name != NULL) free(cl->name);
    if(cl->characters) free(cl->characters);
    free(cl);
}

/**
Charclass Table Management.  The char class table is
simplified by making it a array of chains indexed by the
first char of the name of the char class.
*/

static void
charclassInsert(TTM* ttm, Charclass* cl)
{
    Charclass** table = ttm->charclasses;
    Charclass* next;
    int c0 = cl->name[0];
    next = table[c0];
    if(next == NULL) {
	table[c0] = cl;
    } else {
	Charclass* prev = NULL;
	/* search chain for cl */
        while(next != NULL) {
	    if(strcmp(cl->name,next->name)==0)
		break;
	    prev = next;
	    next = next->next;
	}
	if(next == NULL) {/* cl not previously defined */
	    prev->next = cl;
	} else {/* replace existing definition */
	    Charclass* chain = next->next; /* save */
	    freeCharclass(ttm,next);
	    prev->next = cl;
	    cl->next = chain;
	}
    }
}

static Charclass*
charclassLookup(TTM* ttm, char* name)
{
    Charclass** table = ttm->charclasses;
    Charclass* cl;
    int c0;
    c0 = name[0];
    cl = table[c0];
    while(cl != NULL) {/* search chain for cl */
	if(strcmp(name,cl->name)==0)
	    break;
	cl = cl->next;
    }
    return cl;
}

static Charclass*
charclassRemove(TTM* ttm, char* name)
{
    Charclass** table = ttm->charclasses;
    Charclass* next;
    Charclass* prev;
    int c0;
    c0 = name[0];
    next = table[c0];
    prev = NULL;
    while(next != NULL) {/* search chain for cl */
	if(strcmp(name,next->name)==0) {
	    prev->next = next->next;
	    break;
	}
	prev = next;
	next = next->next;
    }
    return next;
}

static int
charclassMatch(char* s, char* charclass)
{
    int c;
    char* p=charclass;
    while((c=*p)) {
	if(ismultibyte(c)) {
	    int i;
	    int count=(utf8count(c)-1);
	    for(i=0;i<count;i++) {if(s[i] != p[i]) return 0;}
	    p += count;
	} else if(*s != *p) {
	    break;
	} else /* => *s == *p */
	    return 1;
    }
    return 0;
}

/**************************************************/

/**
This is basic top level scanner.
*/
static void
scan(TTM* ttm)
{
    int c;
    Buffer* bb = ttm->buffer;

    for(;;) {
	if(ttm->flags & FLAG_EXIT) break;
	c = *bb->active; /* NOTE that we do not bump here */
	if(c == NUL) { /* End of buffer */
	    break;
	} else if(isescape(c)) {
	    bb->active++;
	    c = *bb->active; /* do not bump */
	    if(ismultibyte(c)) {
		int i; for(i=0;i<utf8count(c);i++)
		    {*bb->passive++ = *bb->active++;}
	    } else if((c = convertEscapeChar(c)) != NUL)
		{*bb->passive++=(char)c;}
	} else if(c == ttm->sharpc) {/* Start of call? */
	    if(bb->active[1] == ttm->openc
	       || (bb->active[1] == ttm->sharpc
                    && bb->active[2] == ttm->openc)) {
		/* It is a real call */
		exec(ttm,bb);
	    } else /* not an call; just pass the # along passively */
	        *bb->passive++ = (char)c;
	} else if(c == ttm->openc) { /* Start of <...> escaping */
	    int depth = 0;
	    /* Skip the leading lbracket */
	    for(;;) {
	        c = *(bb->active);
		if(c == NUL) fail(ttm,EEOS); /* Unexpected EOF */
	        if(isescape(c)) {
		    int i, count;
		    *bb->passive++ = (char)c;
		    bb->active++;
		    c = *bb->active;
		    count = (ismultibyte(c)?utf8count(c):1);
		    for(i=0;i<utf8count(c);i++) {*bb->passive++=*bb->active++;}
	        } else if(c == ttm->openc) {
		    *bb->passive++ = (char)c;
		    bb->active++;
	            depth++;
		} else if(c == ttm->closec) {
		    bb->active++;
		    if(--depth == 0) break; /* we are done */
	        } else
		    bb->active++; /* keep moving */
	    }/*<...> for*/
	} else /* non-signficant character */
	    bb->active++; /* keep moving */
    } /*scan for*/

    /* When we get here, we are finished, so clean up */
    { 
	unsigned long newlen;
	/* reset the buffer length using bb->passive.*/
	newlen = bb->passive - bb->content;
	setBufferLength(ttm,bb,newlen);
	/* reset bb->active */
	bb->active = bb->passive;
    }
}

static void
exec(TTM* ttm, Buffer* bb)
{
    Frame* frame;
    String* fcn;
    char* savepassive;

    frame = pushFrame(ttm);
    /* Skip to the start of the function name */
    if(bb->active[1] == ttm->openc) {
	bb->active += 2;
	frame->active = 1;
    } else {
	bb->active += 3;
	frame->active = 0;
    }
    /* Parse and store relevant pointers into frame. */
    savepassive = bb->passive;
    parsecall(ttm,frame);
    bb->passive = savepassive;

    /* Now execute this function, which will leave result in bb->result */
    if(frame->argc == 0) fail(ttm,ENONAME);
    if(strlen(frame->argv[0])==0) fail(ttm,ENONAME);
    /* Locate the function to execute */
    fcn = dictionaryLookup(ttm,frame->argv[0]);
    if(fcn == NULL) fail(ttm,ENONAME);
    if(fcn->minargs < (frame->argc - 1)) /* -1 to account for function name*/
	fail(ttm,EFEWPARMS);
    /* Reset the result buffer */
    resetBuffer(ttm,ttm->result);
    if(ttm->flags & FLAG_TRACE)
	trace(ttm,1,0);
    if(fcn->builtin)
	fcn->fcn(ttm,frame);
    else /* invoke the pseudo function "call" */
	call(ttm,frame,fcn->body);
    if(ttm->flags & FLAG_TRACE)
	trace(ttm,0,0);

    /* Now, put the result into the buffer */
    if(ttm->result->length > 0) {
	char* insertpos;
	unsigned long resultlen = ttm->result->length;
	/*Compute the space avail between bb->passive and bb->active */
	unsigned long avail = (bb->active - bb->passive); 
	/* Compute amount we need to expand, if any */
	if(avail < resultlen)
	    expandBuffer(ttm,bb,(resultlen - avail));/*will change bb->active*/
	/* We insert the result as follows:
	   frame->passive => insert at bb->passive (and move bb->passive)
	   frame->active => insert at bb->active - (ttm->result->length)
	*/
	if(frame->active)
	    insertpos = (bb->active - resultlen);
	else { /*frame->passive*/
	    insertpos = bb->passive;
	    bb->passive += resultlen;
	}
	memcpy((void*)insertpos,ttm->result->content,ttm->result->length);
    }
    popFrame(ttm);
}

/**
Construct a frame; leave bb->active pointing just
past the call.
*/
static void
parsecall(TTM* ttm, Frame* frame)
{
    int c,done,depth,i,count;
    Buffer* bb = ttm->buffer;

    for(done=0;done;) {
	frame->argv[frame->argc] = bb->passive; /* start of ith argument */
	c = *bb->active; /* Note that we do not bump here */
	if(c == NUL) fail(ttm,EEOS); /* Unexpected end of buffer */
        if(isescape(c)) {
	    bb->active++;
	    c = *bb->active;
	    if(ismultibyte(c)) {
	        for(i=0;i<utf8count(c);i++)
		    {*bb->passive++ = *bb->active++;}
	    } else if((c = convertEscapeChar(c)) != NUL)
		{*bb->passive++=(char)c;}
	} else if(c == ttm->semic || c == ttm->closec) {
	    /* End of an argument */
	    *bb->passive++ = NUL; /* null terminate the argument */
	    /* move to next arg */
	    frame->argc++;
	    if(c == ttm->closec) done=1;
	    else if(frame->argc >= MAXARGS) fail(ttm,EMANYPARMS);
	    else frame->argv[frame->argc] = bb->passive;
	} else if(c == ttm->sharpc) {
	    /* check for call within call */
	    if(bb->active[1] == ttm->openc
	       || (bb->active[1] == ttm->sharpc
                    && bb->active[2] == ttm->openc)) {
		/* Recurse to compute inner call */
		exec(ttm,bb);
	    }
	    *bb->passive++ = *bb->active++; /* just pass it */
	} else if(c == ttm->openc) {/* <...> nested brackets */
	    bb->active++; /* skip leading lbracket */
	    for(;;) {
	        c = *(bb->active);
		if(c == NUL) fail(ttm,EEOS); /* Unexpected EOF */
	        if(isescape(c)) {
		    *bb->passive++ = (char)c;
		    bb->active++;
		    c = *bb->active;
		    count = (ismultibyte(c)?utf8count(c):1);
		    for(i=0;i<utf8count(c);i++) {*bb->passive++=*bb->active++;}
	        } else if(c == ttm->openc) {
		    *bb->passive++ = (char)c;
		    bb->active++;
	            depth++;
		} else if(c == ttm->closec) {
		    bb->active++;
		    if(--depth == 0) break; /* we are done */
	        } else
		    bb->active++; /* keep moving */
	    }/*<...> for*/
	    break;	    
	} else {
	    /* keep moving */
	    bb->active++;
	}
    }/*outer for */
}

/**************************************************/
/**
Execute a non-builtin function
*/

static char*
call(TTM* ttm, Frame* frame, char* body)
{
    char* p;
    int c;
    unsigned long len;
    char* result;
    char* dst;

    /* Compute the size of the output */
    for(len=0,p=body;(c=*p++);) {
	if(c == SEGMENT) {
	    int segindex = (int)(*p++);
	    if(segindex < frame->argc)
	        len += strlen(frame->argv[segindex]);
	    /* else treat as empty string */
	} else if(c == CREATEMARK) {
	    len += CREATELEN;
	} else
	    len++;
    }
    /* Compute the result */
    resetBuffer(ttm,ttm->result);
    setBufferLength(ttm,ttm->result,len);
    result = ttm->result->content;
    dst = result;
    dst[0] = NUL; /* so we can use strcat */
    for(p=body;(c=*p++);) {
	if(c == SEGMENT) {
	    int segindex = (int)(*p++);
	    if(segindex < frame->argc) {
		char* arg = frame->argv[segindex];
	        strcat(dst,arg);
		dst += strlen(arg);
	    } /* else treat as null string */
	} else if (c == CREATEMARK) {
	    char crformat[64];
	    char crval[CREATELEN+1];
	    /* Construct the format string */
	    snprintf(crformat,sizeof(crformat),"%%0%dd",CREATELEN);
	    /* Construct the cr value */
	    ttm->crcounter++;
	    snprintf(crval,sizeof(crval),crformat,ttm->crcounter);
	    strcat(dst,crval);
	    dst += strlen(crval);
	} else
	    *dst++ = (char)c;
    }
    return result;
}

/**************************************************/
/* Built-in Support Procedures */

static void
printstring(TTM* ttm, FILE* output, char* s)
{
    int c;
    char* p;
    int slen = strlen(s);

    if(slen == 0) return;
    for(p=s;(c=*p++);) {
	if(c == UNIVERSAL_ESCAPE) {
	    c=*p++;
	    if(c == NUL)
		break; /* escape at end of buffer for some reason */
	    /* if c is a control character, then elide it */
	    if(!iscontrol(c))
	        fputc(c,output);
	} else
	    fputc(c,output);
    }
}

/**************************************************/
/* Builtin functions */

/* Original TTM functions */

/* Dictionary Operations */
static void
ttm_ap(TTM* ttm, Frame* frame) /* Append to a string */
{
    char* body;
    char* apstring;
    String* str = dictionaryLookup(ttm,frame->argv[1]);
    if(str == NULL) {/* Define the string */
	ttm_ds(ttm,frame);
	return;
    }
    if(str->builtin) fail(ttm,ENOPRIM);
    body = str->body;
    apstring = frame->argv[2];
    str->residual = strlen(body)+strlen(apstring);
    realloc(body,str->residual+1);
    if(body == NULL) fail(ttm,EMEMORY);
    strcat(body,apstring);
}

static void
ttm_cf(TTM* ttm, Frame* frame) /* Copy a function */
{
    char* newname = frame->argv[1];
    char* oldname = frame->argv[2];
    String* newstr = dictionaryLookup(ttm,newname);
    String* oldstr = dictionaryLookup(ttm,oldname);

    if(oldstr == NULL)
	fail(ttm,ENONAME);
    if(newstr == NULL) {
	/* create a new string object */
	newstr = newString(ttm);
	newstr->name = newname;
	frame->argv[1] = NULL;
	dictionaryInsert(ttm,newstr);
    }
    /* Propagate old to new */
    if(oldstr->builtin) {
	newstr->builtin = 1;
	newstr->fcn = oldstr->fcn;
	newstr->minargs = oldstr->minargs;
	newstr->residual = 0;
	if(newstr->body != NULL) free(newstr->body);
	newstr->body = NULL;
    } else {
	char* p;
	char newmarks[MAXMARKS+1]; /* so we can start at one */
	int newindex,i;

	newstr->builtin = 0;
	newstr->fcn = NULL;
	newstr->minargs = oldstr->minargs;
	newstr->residual = 0;
	if(newstr->body != NULL) free(newstr->body);
	newstr->body = strdup(oldstr->body+oldstr->residual);
	/* Note: figuring out the numbering for segment marks
           in the new body is hopelessly undefined because
           there may be gaps in the segment numbers.
	   So, the best I can think of is to collect the different
	   segment marks and renumber them.
	*/
	memset(newmarks,0,sizeof(newmarks)); /* clear the mark set */
	/* Collect the segment number in the new string */
	for(p=newstr->body;*p;p++) {
	    if(*p == SEGMENT) {p++;newmarks[(int)*p]++;}
	}
	/* Walk the newmarks and assign a new mark number */
	for(newindex=1,i=1;i<=MAXMARKS;i++) {
	    if(newmarks[i] > 0) newmarks[i] = newindex++;
	}
        /* Renumber the marks and count */
        for(p=newstr->body;*p;p++) {
	    if(*p == SEGMENT) {
		p++;
		*p = (char)newmarks[(int)*p];
	    }
	}
    }
}

static void
ttm_cr(TTM* ttm, Frame* frame) /* Mark for creation */
{
    String* str;
    int bodylen,crlen;
    char* body;
    char* crstring;

    str = dictionaryLookup(ttm,frame->argv[1]);
    if(str == NULL)
	fail(ttm,ENONAME);
    if(str->builtin)
	fail(ttm,ENOPRIM);

    body = str->body;
    bodylen = strlen(body);
    crstring = frame->argv[2];
    crlen = strlen(crstring);
    if(crlen > 0 && crlen < bodylen) { /* only search if it has a chance of success */
	/* Search for occurrences of arg */
	int endpos = (bodylen - crlen); /* max point for which search makes sense */
	int pos;
	for(pos=0;pos<endpos;) {
	    if(memcmp((void*)body+pos,(void*)crstring,crlen) != 0)
		continue;
	    /* we have a match, replace match by a create marker */
	    makespace(body+pos+1,body+pos,1);
	    body[pos++] = CREATEMARK;
	    /* compress out the crstring */
	    memcpy(body+pos,body+pos+crlen,crlen);
	    /* fix up */
	    bodylen -= (crlen - 1);
            endpos = (bodylen - crlen);
	}
    }
}

static void
ttm_ds(TTM* ttm, Frame* frame)
{
    String* str = dictionaryLookup(ttm,frame->argv[1]);
    if(str == NULL) {
	/* create a new string object */
	str = newString(ttm);
	str->name = frame->argv[1];
	frame->argv[1] = NULL;
	dictionaryInsert(ttm,str);
    }
    str->builtin = 0;
    if(str->body != NULL)
	free(str->body);
    str->body = frame->argv[2];
    frame->argv[2] = NULL;
}

static void
ttm_es(TTM* ttm, Frame* frame) /* Erase string */
{
    int i;
    for(i=1;i<frame->argc;i++) {
	char* strname = frame->argv[i];
	String* prev = dictionaryRemove(ttm,strname);
	if(prev != NULL) {
	    freeString(ttm,prev); /* reclaim the string */
	}
    }
}

/* Helper function for #<sc> and #<ss> */
static int
ttm_ss0(TTM* ttm, Frame* frame)
{
    String* str;
    int bodylen;
    char* body;
    int i;
    int segcount;

    str = dictionaryLookup(ttm,frame->argv[2]);
    if(str == NULL)
	fail(ttm,ENONAME);
    if(str->builtin)
	fail(ttm,ENOPRIM);

    segcount = 0;
    str->minargs = 0;
    body = str->body;
    bodylen = strlen(body);
    for(i=3;i<frame->argc;i++) {
	char* arg = frame->argv[i];
	int arglen = strlen(arg);
        int endpos = (bodylen - arglen); /* max point for which search makes sense */
	int pos;

	if(arglen == 0 || arglen > bodylen) continue; /* No substitution possible */
	/* Search for occurrences of arg */
	for(pos=0;pos<endpos;) {
	    if(memcmp((void*)body+pos,(void*)arg,arglen) != 0)
		continue;
	    /* we have a match, replace match by a segment marker (beginning at 1)*/
	    /* make sure we have 2 characters of space */
	    makespace(body+pos+2,body+pos,2);
	    body[pos++] = SEGMENT;
	    body[pos++] = (char)((i-1)&0xFF);
	    /* compress out the argument */
	    memcpy(body+pos,body+pos+arglen,arglen);
	    /* fix up */
	    bodylen -= (arglen - 2);
            endpos = (bodylen - arglen);
	    segcount++;
	}
    }
    return segcount;
}

static void
ttm_sc(TTM* ttm, Frame* frame) /* Segment and count */
{
    char count[64];
    int nsegs = ttm_ss0(ttm,frame);
    snprintf(count,sizeof(count),"%d",nsegs);
    /* Insert into ttm->result */
    setBufferLength(ttm,ttm->result,strlen(count));
    strcpy(ttm->result->content,count);
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
    String* str;

    str = dictionaryLookup(ttm,frame->argv[2]);
    if(str == NULL)
	fail(ttm,ENONAME);
    if(str->builtin)
	fail(ttm,ENOPRIM);
    /* Check for pointing at trailing NUL */
    if(str->residual < strlen(str->body)) {
	setBufferLength(ttm,ttm->result,1);
	ttm->result->content[0] = str->body[str->residual];
    }
}

static void
ttm_cn(TTM* ttm, Frame* frame) /* Call n characters */
{
    String* str;
    long long n;
    int avail;
    ERR err;

    str = dictionaryLookup(ttm,frame->argv[2]);
    if(str == NULL)
	fail(ttm,ENONAME);
    if(str->builtin)
	fail(ttm,ENOPRIM);
    /* Get number of characters to extract */
    err = toInt64(frame->argv[1],&n);
    if(err != ENOERR) fail(ttm,err);
    /* modify to match the number available */
    avail = strlen(str->body) - str->residual;
    if(avail < 0) avail = 0;
    if(avail < n) n = avail;
    if(n > 0) {
	setBufferLength(ttm,ttm->result,n);
	memcpy(ttm->result->content,str->body+str->residual,n);
	ttm->result->content[n] = NUL;
    }
}

static void
ttm_cp(TTM* ttm, Frame* frame) /* Call parameter */
{
    String* str;
    char* rp;
    char* p;
    int c;

    str = dictionaryLookup(ttm,frame->argv[1]);
    if(str == NULL)
	fail(ttm,ENONAME);
    if(str->builtin)
	fail(ttm,ENOPRIM);

    rp = str->body + str->residual;
    for(p=rp;(c=*p);p++) {
	if(c == ttm->escapec || c == UNIVERSAL_ESCAPE) {
	    *p = UNIVERSAL_ESCAPE;
	    p++;
	} else if(c == ttm->semic) {
	    break;
	} else if(c == ttm->openc) {
	    int depth = 1;
	    for(;(c=*p);p++) {
		if(c == ttm->escapec || c == UNIVERSAL_ESCAPE) {
		    *p = UNIVERSAL_ESCAPE;
		    p++;
		} else if(c == ttm->openc) {
		    depth++;
		} else if(c == ttm->closec) {
		    depth--;
		    if(depth == 0) {p++; break;}
		}
	    }
	}
    }

    if(p >= rp) {/* non-zero length parameter */
	int len = (p - rp);
	setBufferLength(ttm,ttm->result,len);
	memcpy((void*)ttm->result->content,(void*)rp,len);
	ttm->result->content[len] = NUL;
    }
    str->residual = (p - str->body);
}

static void
ttm_cs(TTM* ttm, Frame* frame) /* Call segment */
{
    String* str;
    char* p;
    unsigned long offset;

    str = dictionaryLookup(ttm,frame->argv[1]);
    if(str == NULL)
	fail(ttm,ENONAME);
    if(str->builtin)
	fail(ttm,ENOPRIM);

    /* Locate the next segment mark */
    /* Unclear if create marks also qualify; assume not */
    p = str->body + str->residual;
    for(;*p;p++) {if(*p == SEGMENT) break;}
    offset = (p - str->body) - str->residual;
    if(offset > 0) {
	setBufferLength(ttm,ttm->result,offset);
	memcpy((void*)ttm->result->content,str->body+str->residual,offset);
	ttm->result->content[offset] = NUL;
    }
}

static void
ttm_isc(TTM* ttm, Frame* frame) /* Initial character scan */
{
    String* str;
    char* retval;

    str = dictionaryLookup(ttm,frame->argv[2]);
    if(str == NULL)
	fail(ttm,ENONAME);
    if(str->builtin)
	fail(ttm,ENOPRIM);
    /* check for initial string match */
    if(strcmp(frame->argv[1],str->body+str->residual)==0) {
	retval = frame->argv[3];
    } else
	retval = frame->argv[4];
    setBufferLength(ttm,ttm->result,strlen(retval));
    strcpy(ttm->result->content,retval);    
}

static void
ttm_rrp(TTM* ttm, Frame* frame) /* Reset residual pointer */
{
    String* str = dictionaryLookup(ttm,frame->argv[1]);
    if(str == NULL)
	fail(ttm,ENONAME);
    if(str->builtin)
        fail(ttm,ENOPRIM);
    str->residual = 0;
}

static void
ttm_scn(TTM* ttm, Frame* frame) /* Character scan */
{
    String* str;
    char* s1;
    char* s2;
    int s1len,match,bodylen;
    char* start;
    unsigned long offset,endpoint;

    str = dictionaryLookup(ttm,frame->argv[2]);
    if(str == NULL)
	fail(ttm,ENONAME);
    if(str->builtin)
	fail(ttm,ENOPRIM);

    s1 = frame->argv[1];
    s1len = strlen(s1);
    s2 = frame->argv[3];
    if(s1len == 0)
	goto fail;
    bodylen = strlen(str->body);
    if((bodylen - (str->residual + s1len)) < 0)
	goto fail;
    start = str->body + str->residual;
    endpoint = bodylen - s1len;
    /* check for substring match after the residual pointer */
    for(match=0,offset=0;offset<endpoint;offset++) {
	if(memcmp(start+offset,s1,s1len) == 0) {match=1;break;}
    }
    if(match) {
	if(offset == 0)
	    str->residual += s1len;
	else {
	    int retlen = offset - str->residual;
	    setBufferLength(ttm,ttm->result,retlen);
	    memcpy((void*)ttm->result->content,(void*)(str->body+retlen),retlen);
	    ttm->result->content[retlen] = NUL;
	}
    } else
	goto fail;
    return;

fail:
    if(s2 != NULL) {
	int s2len = strlen(s2);
        setBufferLength(ttm,ttm->result,s2len);
        strcpy(ttm->result->content,s2);
    }
}

/* String Scanning Operations */

static void
ttm_gn(TTM* ttm, Frame* frame) /* Give n characters */
{
    char* snum = frame->argv[1];
    char* s = frame->argv[2];
    int slen = strlen(s);
    ERR err;
    long long num;

    err = toInt64(snum,&num);
    if(err != ENOERR) fail(ttm,err);
    if(num > 0) {
	if(slen < num) num = slen;
	setBufferLength(ttm,ttm->result,num);
	memcpy(ttm->result->content,s,num);
	ttm->result->content[num] = NUL;
    } else if(num < 0) {
	int rem;
	num = -num;
	if(slen < num) num = slen;
	rem = (slen - num);
	setBufferLength(ttm,ttm->result,rem);
	memcpy((void*)ttm->result->content,s+rem,num);
	ttm->result->content[num] = NUL;
    }
}

static void
ttm_zlc(TTM* ttm, Frame* frame) /* Zero-level commas */
{
    char* s;
    char* p;
    int slen,c;

    s = frame->argv[1];
    slen = strlen(s);
    setBufferLength(ttm,ttm->result,slen); /* result will be same length */
    strcpy(ttm->result->content,s);
    for(p=ttm->result->content;(c=*p);p++) {
	if(c == ttm->escapec || c == UNIVERSAL_ESCAPE) {
	    *p++ = UNIVERSAL_ESCAPE;
	} else if(c == COMMA) {
	    *p++ = ttm->semic;	
	} else if(c == LPAREN) {
	    int depth = 1;
	    for(;(c=*p);p++) {
		if(c == ttm->escapec || c == UNIVERSAL_ESCAPE) {
		    *p++ = UNIVERSAL_ESCAPE;
		} else if(c == LPAREN) {
		    depth++;
		} else if(c == RPAREN) {
		    depth--;
		    if(depth == 0) {p++; break;}
		}
	    }
	}
    }
    *p = NUL; /* make sure it is terminated */
    setBufferLength(ttm,ttm->result,(p - ttm->result->content));
}

static void
ttm_zlcp(TTM* ttm, Frame* frame) /* Zero-level commas and parentheses */
{
    /* A(B) will give A;B and (A),(B),C will give A;B;C */
    char* s;
    char* p;
    char* q;
    int slen,c;

    s = frame->argv[1];
    slen = strlen(s);
    setBufferLength(ttm,ttm->result,slen); /* result may be shorter; handle below */
    q = ttm->result->content;
    p = s;
    for(;(c=*p);p++) {
	if(c == ttm->escapec || c == UNIVERSAL_ESCAPE) {
	    *q++ = UNIVERSAL_ESCAPE;
	    *q++ = *(++p);
	} else if(c == COMMA) {
	    if(p != s && *(q-1)==ttm->semic) {
	        /* dont copy this comma */
	    } else
	        *q++ = ttm->semic;	
	} else if(c == LPAREN) {
	    int depth;
	    if(p != s && *(q-1)==ttm->semic) {
		/* do not copy this comma */
	    } else
	        *q++ = ttm->semic;	
	    /* find matching lparen */
	    for(depth=1,p++;(c=*p);p++) {
		if(c == ttm->escapec || c == UNIVERSAL_ESCAPE) {
		    *q++ = UNIVERSAL_ESCAPE;
		    *q++ = *(++p);
		} else if(c == LPAREN) {
		    depth++;
		} else if(c == RPAREN) {
		    depth--;
		    if(depth == 0) break;
		}
	    }
	    if(c == RPAREN) {
		if(p != s && *(q-1) == ttm->semic) {
		    /* do not copy this RPAREN */
		} else
		    *q++ = ttm->semic;
	    }
	}
    }
    *q = NUL; /* make sure it is terminated */
    setBufferLength(ttm,ttm->result,(q-ttm->result->content));
}

static void
ttm_ccl(TTM* ttm, Frame* frame) /* Call class */
{
    Charclass* cl = charclassLookup(ttm,frame->argv[1]);
    String* str = dictionaryLookup(ttm,frame->argv[2]);
    char* p;
    char* start;
    unsigned long len;
    int c;

    if(cl == NULL || str == NULL)
	fail(ttm,ENONAME);
    if(str->builtin)
	fail(ttm,ENOPRIM);
    /* Starting at str->residual, locate first char not in class */
    start = str->body+str->residual;
    p = start;
    while((c=*p++)) {
	if(cl->negative && charclassMatch(p,cl->characters)) break;
	if(!cl->negative && !charclassMatch(p,cl->characters)) break;
    }
    len = (p - start);
    if(len > 0) {
	setBufferLength(ttm,ttm->result,len);
	memcpy((void*)ttm->result->content,start,len);
	ttm->result->content[len] = NUL;
	str->residual += len;
    }
}

/* Shared helper for dcl and dncl */
static void
ttm_dcl0(TTM* ttm, Frame* frame, int negative)
{
    Charclass* cl = charclassLookup(ttm,frame->argv[1]);
    if(cl == NULL) {
	/* create a new charclass object */
	cl = newCharclass(ttm);
	cl->name = frame->argv[1];
	frame->argv[1] = NULL;
	charclassInsert(ttm,cl);
    }
    if(cl->characters != NULL)
	free(cl->characters);
    cl->characters = frame->argv[2];
    frame->argv[2] = NULL;
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
    int i;
    for(i=1;i<frame->argc;i++) {
	char* clname = frame->argv[i];
	Charclass* prev = charclassRemove(ttm,clname);
	if(prev != NULL) {
	    freeCharclass(ttm,prev); /* reclaim the character class */
	}
    }
}

static void
ttm_scl(TTM* ttm, Frame* frame) /* Skip class */
{
    Charclass* cl = charclassLookup(ttm,frame->argv[1]);
    String* str = dictionaryLookup(ttm,frame->argv[2]);
    char* p;
    char* start;
    unsigned long len;
    int c;

    if(cl == NULL || str == NULL)
	fail(ttm,ENONAME);
    if(str->builtin)
	fail(ttm,ENOPRIM);
    /* Starting at str->residual, locate first char not in class */
    start = str->body+str->residual;
    p = start;
    while((c=*p++)) {
	if(cl->negative && charclassMatch(p,cl->characters)) break;
	if(!cl->negative && !charclassMatch(p,cl->characters)) break;
    }
    len = (p - start);
    str->residual += len;
}

static void
ttm_tcl(TTM* ttm, Frame* frame) /* Test class */
{
    Charclass* cl = charclassLookup(ttm,frame->argv[1]);
    String* str = dictionaryLookup(ttm,frame->argv[2]);
    char* rp;
    char* retval;
    unsigned long retlen;

    if(cl == NULL)
	fail(ttm,ENONAME);
    if(str->builtin)
	fail(ttm,ENOPRIM);
    if(str == NULL)
	retval = frame->argv[4];
    else {
        /* see if char at str->residual is in class */
        rp = str->body + str->residual;
        if(cl->negative && charclassMatch(rp,cl->characters)) 
	    retval = frame->argv[3];
        else if(!cl->negative && !charclassMatch(rp,cl->characters))
	    retval = frame->argv[3];
	else
	    retval = frame->argv[4];
    }
    retlen = strlen(retval);
    if(retlen > 0) {
	setBufferLength(ttm,ttm->result,retlen);
	strcpy(ttm->result->content,retval);
    }    
}

/* Arithmetic Operators */

static void
ttm_abs(TTM* ttm, Frame* frame) /* Obtain absolute value */
{
    char* slhs;
    long long lhs;
    ERR err;
    char result[32];

    slhs = frame->argv[1];    
    err = toInt64(slhs,&lhs);
    if(err != ENOERR) fail(ttm,err);
    if(lhs < 0) lhs = -lhs;
    snprintf(result,sizeof(result),"%lld",lhs);
    setBufferLength(ttm,ttm->result,strlen(result));
    strcpy(ttm->result->content,result);
}

static void
ttm_ad(TTM* ttm, Frame* frame) /* Add */
{
    char* slhs;
    char* srhs;
    long long lhs,rhs;
    ERR err;
    char result[32];

    slhs = frame->argv[1];    
    srhs = frame->argv[2];

    err = toInt64(slhs,&lhs);
    if(err != ENOERR) fail(ttm,err);
    err = toInt64(srhs,&rhs);
    if(err != ENOERR) fail(ttm,err);
    lhs = (lhs + rhs);
    snprintf(result,sizeof(result),"%lld",lhs);
    setBufferLength(ttm,ttm->result,strlen(result));
    strcpy(ttm->result->content,result);
}

static void
ttm_dv(TTM* ttm, Frame* frame) /* Divide and give quotient */
{
    char* slhs;
    char* srhs;
    long long lhs,rhs;
    ERR err;
    char result[32];

    slhs = frame->argv[1];    
    srhs = frame->argv[2];

    err = toInt64(slhs,&lhs);
    if(err != ENOERR) fail(ttm,err);
    err = toInt64(srhs,&rhs);
    if(err != ENOERR) fail(ttm,err);
    lhs = (lhs / rhs);
    snprintf(result,sizeof(result),"%lld",lhs);
    setBufferLength(ttm,ttm->result,strlen(result));
    strcpy(ttm->result->content,result);
}

static void
ttm_dvr(TTM* ttm, Frame* frame) /* Divide and give remainder */
{
    char* slhs;
    char* srhs;
    long long lhs,rhs;
    ERR err;
    char result[32];

    slhs = frame->argv[1];    
    srhs = frame->argv[2];

    err = toInt64(slhs,&lhs);
    if(err != ENOERR) fail(ttm,err);
    err = toInt64(srhs,&rhs);
    if(err != ENOERR) fail(ttm,err);
    lhs = (lhs % rhs);
    snprintf(result,sizeof(result),"%lld",lhs);
    setBufferLength(ttm,ttm->result,strlen(result));
    strcpy(ttm->result->content,result);
}

static void
ttm_mu(TTM* ttm, Frame* frame) /* Multiply */
{
    char* slhs;
    char* srhs;
    long long lhs,rhs;
    ERR err;
    char result[32];

    slhs = frame->argv[1];    
    srhs = frame->argv[2];

    err = toInt64(slhs,&lhs);
    if(err != ENOERR) fail(ttm,err);
    err = toInt64(srhs,&rhs);
    if(err != ENOERR) fail(ttm,err);
    lhs = (lhs * rhs);
    snprintf(result,sizeof(result),"%lld",lhs);
    setBufferLength(ttm,ttm->result,strlen(result));
    strcpy(ttm->result->content,result);
}

static void
ttm_su(TTM* ttm, Frame* frame) /* Substract */
{
    char* slhs;
    char* srhs;
    long long lhs,rhs;
    ERR err;
    char result[32];

    slhs = frame->argv[1];    
    srhs = frame->argv[2];
    err = toInt64(slhs,&lhs);
    if(err != ENOERR) fail(ttm,err);
    err = toInt64(srhs,&rhs);
    if(err != ENOERR) fail(ttm,err);
    lhs = (lhs - rhs);
    snprintf(result,sizeof(result),"%lld",lhs);
    setBufferLength(ttm,ttm->result,strlen(result));
    strcpy(ttm->result->content,result);
}

static void
ttm_eq(TTM* ttm, Frame* frame) /* Compare numeric equal */
{
    char* slhs;
    char* srhs;
    char* t;
    char* f;
    long long lhs,rhs;
    ERR err;
    char* result;

    slhs = frame->argv[1];    
    srhs = frame->argv[2];
    t = frame->argv[3];    
    f = frame->argv[4];

    err = toInt64(slhs,&lhs);
    if(err != ENOERR) fail(ttm,err);
    err = toInt64(srhs,&rhs);
    if(err != ENOERR) fail(ttm,err);

    result = (lhs == rhs ? t : f);
    setBufferLength(ttm,ttm->result,strlen(result));
    strcpy(ttm->result->content,result);
}

static void
ttm_gt(TTM* ttm, Frame* frame) /* Compare numeric greater-than */
{
    char* slhs;
    char* srhs;
    char* t;
    char* f;
    long long lhs,rhs;
    ERR err;
    char* result;

    slhs = frame->argv[1];    
    srhs = frame->argv[2];
    t = frame->argv[3];    
    f = frame->argv[4];

    err = toInt64(slhs,&lhs);
    if(err != ENOERR) fail(ttm,err);
    err = toInt64(srhs,&rhs);
    if(err != ENOERR) fail(ttm,err);

    result = (lhs > rhs ? t : f);
    setBufferLength(ttm,ttm->result,strlen(result));
    strcpy(ttm->result->content,result);
}

static void
ttm_lt(TTM* ttm, Frame* frame) /* Compare numeric less-than */
{
    char* slhs;
    char* srhs;
    char* t;
    char* f;
    long long lhs,rhs;
    ERR err;
    char* result;

    slhs = frame->argv[1];    
    srhs = frame->argv[2];
    t = frame->argv[3];    
    f = frame->argv[4];

    err = toInt64(slhs,&lhs);
    if(err != ENOERR) fail(ttm,err);
    err = toInt64(srhs,&rhs);
    if(err != ENOERR) fail(ttm,err);

    result = (lhs < rhs ? t : f);
    setBufferLength(ttm,ttm->result,strlen(result));
    strcpy(ttm->result->content,result);
}

static void
ttm_eql(TTM* ttm, Frame* frame) /* ? Compare logical equal */
{
    char* slhs;
    char* srhs;
    char* t;
    char* f;
    char* result;

    slhs = frame->argv[1];    
    srhs = frame->argv[2];
    t = frame->argv[3];    
    f = frame->argv[4];

    result = (strcmp(slhs,srhs) == 0 ? t : f);
    setBufferLength(ttm,ttm->result,strlen(result));
    strcpy(ttm->result->content,result);
}

static void
ttm_gtl(TTM* ttm, Frame* frame) /* ? Compare logical greater-than */
{
    char* slhs;
    char* srhs;
    char* t;
    char* f;
    char* result;

    slhs = frame->argv[1];    
    srhs = frame->argv[2];
    t = frame->argv[3];    
    f = frame->argv[4];

    result = (strcmp(slhs,srhs) > 0 ? t : f);
    setBufferLength(ttm,ttm->result,strlen(result));
    strcpy(ttm->result->content,result);
}

static void
ttm_ltl(TTM* ttm, Frame* frame) /* ? Compare logical less-than */
{
    char* slhs;
    char* srhs;
    char* t;
    char* f;
    char* result;

    slhs = frame->argv[1];    
    srhs = frame->argv[2];
    t = frame->argv[3];    
    f = frame->argv[4];

    result = (strcmp(slhs,srhs) < 0 ? t : f);
    setBufferLength(ttm,ttm->result,strlen(result));
    strcpy(ttm->result->content,result);
}

/* Peripheral Input/Output Operations */

static void
ttm_ps(TTM* ttm, Frame* frame) /* Print a String */
{
    char* s = frame->argv[1];
    printstring(ttm,stdout,s);
}

static void
ttm_psr(TTM* ttm, Frame* frame) /* Print String and Read */
{
    ttm_ps(ttm,frame);
    ttm_rs(ttm,frame);
}

static void
ttm_rs(TTM* ttm, Frame* frame) /* Read a String */
{
    int len,c;
    for(len=0;;len++) {
	c=fgetc(stdin);
	if(c == ttm->metac) break;
        setBufferLength(ttm,ttm->result,len+1);
	ttm->result->content[len] = (char)c;
    }
}

static void
ttm_pserr(TTM* ttm, Frame* frame) /* Print a String to stderr */
{
    char* s = frame->argv[2];
    printstring(ttm,stderr,s);
}

static void
ttm_cm(TTM* ttm, Frame* frame) /* Change meta character */
{
    char* smeta = frame->argv[1];
    if(strlen(smeta) > 0) {
	ttm->metac = smeta[0];
    }
}

/* Library Operations */

static void
ttm_names(TTM* ttm, Frame* frame) /* Obtain String Names */
{
    int i,count,first;
    unsigned long len;
    char* p;

    /* First, figure out the total name size and count of number of names */
    count = 0;
    len = 0;
    for(i=0;i<256;i++) {
	if(ttm->dictionary[i] != NULL) {
	    String* name = ttm->dictionary[i];
	    while(name != NULL) {
		count++;
	        len += strlen(name->name);
		name = name->next;
	    }
	}
    }
    setBufferLength(ttm,ttm->result,len+(count-1));
    p = ttm->result->content;
    first = 1;
    for(i=0;i<256;i++) {
	if(ttm->dictionary[i] != NULL) {
	    String* name = ttm->dictionary[i];
	    while(name != NULL) {
		if(!first) strcpy(p,",");
		p++;
		strcpy(p,name->name);		
	        p += strlen(name->name);
		name = name->next;
		first = 0;
	    }
	}
    }
}

static void
ttm_exit(TTM* ttm, Frame* frame) /* Return from TTM */
{
    ttm->flags |= FLAG_EXIT;
    if(frame->argc > 1)
	ttm->exitcode = 1;
}

/* Utility Operations */

static void
ttm_ndf(TTM* ttm, Frame* frame) /* Determine if a Name is Defined */
{
    char* s;
    char* t;
    char* f;
    char* result;
    String* name;

    s = frame->argv[1];    
    t = frame->argv[2];
    f = frame->argv[3];

    name = dictionaryLookup(ttm,s);    
    result = (name != NULL ? t : f);
    setBufferLength(ttm,ttm->result,strlen(result));
    strcpy(ttm->result->content,result);
}

static void
ttm_norm(TTM* ttm, Frame* frame) /* Obtain the Norm of a String */
{
    char* s;
    char result[32];

    s = frame->argv[1];
    snprintf(result,sizeof(result),"%d",strlen(s));
    setBufferLength(ttm,ttm->result,strlen(result));
    strcpy(ttm->result->content,result);
}

static void
ttm_time(TTM* ttm, Frame* frame) /* Obtain Execution Time */
{
    unsigned long long utime;
    char result[64];
    utime = getRunTime();
    snprintf(result,sizeof(result),"%llu",utime);
    setBufferLength(ttm,ttm->result,strlen(result));
    strcpy(ttm->result->content,result);
}

static void
ttm_tf(TTM* ttm, Frame* frame) /* Turn Trace Off */
{
    ttm->flags &= ~(FLAG_TRACE);
}

static void
ttm_tn(TTM* ttm, Frame* frame) /* Turn Trace On */
{
    ttm->flags |= (FLAG_TRACE);
}

/* Functions new to this implementation */

static void
ttm_argv(TTM* ttm, Frame* frame) /* Get ith command line argument */
{
    int index = atol(frame->argv[1]);
    if(index < 0 || index >= getOptionStringLength(argoptions))
	fail(ttm,ERANGE);
    setBufferLength(ttm,ttm->result,strlen(argoptions[index]));
    strcpy(ttm->result->content,argoptions[index]);
}

/**
For security reasons, we impose the constraint
that the file name must only be accessible
through one of the include paths.
This has the possibly undesirable consequence
that if the user used #<include>, then the user
must also specify a -I on the command line.
*/
static void
ttm_include(TTM* ttm, Frame* frame)  /* Include text of a file */
{
    char** path;
    FILE* finclude;
    char* suffix;
    char filename[8192];
    char* startpos;
    Buffer* bb = ttm->buffer;

    suffix = frame->argv[1];
    if(strlen(suffix) == 0)
	fail(ttm,EINCLUDE);
    /* try to open file as is */
    finclude = fopen(filename,"r");
    if(finclude == NULL && suffix[0] == '/')
	fail(ttm,EIO);
    /* Since it is a relative path, try to use -I list */
    for(path=includes;*path;) {
	strcpy(filename,*path);
	strcat(filename,"/");
	strcat(filename,suffix);
	finclude = fopen(filename,"r");
        if(finclude != NULL) break;
    }
    if(finclude == NULL)
	fail(ttm,EINCLUDE);
    startpos = bb->active;
    if(!readfile(ttm,finclude,bb))
	fail(ttm,EIO);
    if(frame->active)
	bb->active = startpos;
}

/**************************************************/

/**
 Builtin function table
*/

struct Builtin {
    char* name;
    int minargs;
    TTMFCN fcn;
};

/* TODO: fix the minargs values */

/* Define a subset of the original TTM functions */
static struct Builtin builtin_orig[] = {
    /* Dictionary Operations */
    {"ap",2,ttm_ap}, /* Append to a string */
    {"cf",2,ttm_cf}, /* Copy a function */
    {"cr",2,ttm_cr}, /* Mark for creation */
    {"ds",2,ttm_ds}, /* Define string */
    {"es",2,ttm_es}, /* Erase string */
    {"sc",2,ttm_sc}, /* Segment and count */
    {"ss",2,ttm_ss}, /* Segment a string */
    /* String Selection */
    {"cc",2,ttm_cc}, /* Call one character */
    {"cn",2,ttm_cn}, /* Call n characters */
    {"cp",2,ttm_cp}, /* Call parameter */
    {"cs",2,ttm_cs}, /* Call segment */
    {"isc",2,ttm_isc}, /* Initial character scan */
    {"rrp",2,ttm_rrp}, /* Reset residual pointer */
    {"scn",2,ttm_scn}, /* Character scan */
    /* String Scanning Operations */
    {"gn",2,ttm_gn}, /* Give n characters */
    {"zlc",2,ttm_zlc}, /* Zero-level commas */
    {"zlcp",2,ttm_zlcp}, /* Zero-level commas and parentheses */
    /* Character Class Operations */
    {"ccl",2,ttm_ccl}, /* Call class */
    {"dcl",2,ttm_dcl}, /* Define a class */
    {"dncl",2,ttm_dncl}, /* Define a negative class */
    {"ecl",2,ttm_ecl}, /* Erase a class */
    {"scl",2,ttm_scl}, /* Skip class */
    {"tcl",2,ttm_tcl}, /* Test class */
    /* Arithmetic Operations */
    {"abs",2,ttm_abs}, /* Obtain absolute value */
    {"ad",2,ttm_ad}, /* Add */
    {"dv",2,ttm_dv}, /* Divide and give quotient */
    {"dvr",2,ttm_dvr}, /* Divide and give remainder */
    {"mu",2,ttm_mu}, /* Multiply */
    {"su",2,ttm_su}, /* Substract */
    /* Numeric Comparisons */
    {"eq",2,ttm_eq}, /* Compare numeric equal */
    {"gt",2,ttm_gt}, /* Compare numeric greater-than */
    {"lt",2,ttm_lt}, /* Compare numeric less-than */
    /* Logical Comparisons */
    {"eq?",2,ttm_eql}, /* ? Compare logical equal */
    {"gt?",2,ttm_gtl}, /* ? Compare logical greater-than */
    {"lt?",2,ttm_ltl}, /* ? Compare logical less-than */
    /* Peripheral Input/Output Operations */
    {"cm",2,ttm_cm}, /*Change Meta Character*/
    {"ps",2,ttm_ps}, /* Print a String */
    {"pserr",2,ttm_pserr}, /* Print a String to stderr*/
    {"psr",2,ttm_psr}, /* Print String and Read */
#ifdef IMPLEMENTED
    {"rcd",2,ttm_rcd}, /* Set to Read Prom Cards */
#endif
    {"rs",2,ttm_rs}, /* Read a String */
    /*Formated Output Operations*/
#ifdef IMPLEMENTED
    {"fm",2,ttm_fm}, /* Format a Line or Card */
    {"tabs",2,ttm_tabs}, /* Declare Tab Positions */
    {"scc",2,ttm_scc}, /* Set Continuation Convention */
    {"icc",2,ttm_icc}, /* Insert a Control Character */
    {"outb",2,ttm_outb}, /* Output the Buffer */
#endif
    /* Library Operations */
#ifdef IMPLEMENTED
    {"store",2,ttm_store}, /* Store a Program */
    {"delete",2,ttm_delete}, /* Delete a Program */
    {"copf",2,ttm_copf}, /* Copy a Program */
    {"show",2,ttm_show}, /* Show Program Names */
#endif
    {"names",2,ttm_names}, /* Obtain String Names */
    /* Utility Operations */
#ifdef IMPLEMENTED
    {"break",2,ttm_break}, /* Program Break */
#endif
    {"exit",2,ttm_exit}, /* Return from TTM */
    {"ndf",2,ttm_ndf}, /* Determine if a Name is Defined */
    {"norm",2,ttm_norm}, /* Obtain the Norm of a String */
    {"time",2,ttm_time}, /* Obtain Execution Time */
    {"tf",2,ttm_tf}, /* Turn Trace Off */
    {"tn",2,ttm_tn}, /* Turn Trace On */
    {NULL,0,NULL} /* terminator */
    };
    
    /* Functions new to this implementation */
    static struct Builtin builtin_new[] = {
    {"argv",2,ttm_argv}, /* Get ith command line argument */
    {"include",2,ttm_include}, /* Include text of a file */
    {NULL,0,NULL} /* terminator */
};

static void
defineBuiltinFunction1(TTM* ttm, struct Builtin* bin)
{
    /* Make sure we did not define builtin twice */
    String* function = dictionaryLookup(ttm,bin->name);
    assert(function == NULL);
    /* create a new function object */
    function = newString(ttm);
    function->builtin = 1;
    function->name = strdup(bin->name);
    function->minargs = bin->minargs;
    function->fcn = bin->fcn;
    dictionaryInsert(ttm,function);
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
/* Error reporting */

static void
fail(TTM* ttm, ERR eno)
{
    char msg[4096];
    if(eno >= 0 && eno < sizeof(errmsgs)/sizeof(char*))
	eno = EOTHER;
    snprintf(msg,sizeof(msg),"(%d) %s",eno,errmsgs[eno]);
    fatal(ttm,msg);
}

static void
fatal(TTM* ttm, const char* msg)
{
    fprintf(stderr,"Fatal error: %s\n",msg);
    if(ttm != NULL) {
	/* Dump the frame stack */
    }
    exit(1);
}

/**************************************************/
/* Utility functions */

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

Return ENOERR if conversion succeeded, err if failed.

*/

static ERR
toInt64(char* s, long long* lp)
{
    char* p = s;
    int c;
    int negative = 1;
    /* skip white space */
    while((c=*p++)) {if(c != ' ' && c != '\t') break;}
    if(c == NUL) return EDECIMAL; /* nothing but white space or just a sign*/
    if(c == '-') {negative = -1; c=*p++;} else if(c == '+') {c=*p++;}
    if(c == NUL) return EDECIMAL; /* just a +|- */
    if(c == '0' && (*p == 'x' || *p == 'X')) { /* hex case */
	unsigned long long ul = 0;
	int i;
	for(i=0;i<16;i++) {/* allow no more than 16 hex digits */
	    c=*p++;
	    if(!ishex(c)) break;
	    ul = (ul<<8) | fromhex(c);
	}
	if(i==0)
	    return EDECIMAL; /* it was just "0x" */
	if(i == 16) /* too big */
	    return EMANYDIGITS;
	if(lp) {*lp = *(long long*)&ul;} /* return as signed value */
	return ENOERR;
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
		return EMANYDIGITS;
	    }
	}      
	/* See if we have a trailing [mMkK];
           ignore overflow in this case */
        if(c == 'm' || c == 'M')
	    ul = (ul<<20);
        else if(c == 'k' || c == 'K')
	    ul = (ul<<10);
	if(lp) {
	    /* convert back to signed */
	    long long l;
	    l = *(long long*)&ul;
	    l *= negative;
	    *lp = l;
	}
        return ENOERR;
    } /* else illegal */
    return EDECIMAL;
}

/* Assumes ismultibyte(c) is true */
static int
utf8count(int c)
{
    if(((c) & 0xE0) == 0xC02) return 2;
    if(((c) & 0xF0) == 0xE0) return 3;
    if(((c) & 0xF8) == 0xF0) return 4;
    if(((c) & 0xFC) == 0xF8) return 5;
    if(((c) & 0xFE) == 0xFC) return 6;
    return 1;
}

/* Given a char, return its escaped value.
Zero indicates it should be elided
*/
static int
convertEscapeChar(int c)
{
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
    return c;
}

/**
Trace a top frame in the frame stack.
*/
static void
trace(TTM* ttm, int entering, int dumping)
{
    trace1(ttm, ttm->stacknext-1, entering, dumping);
}


static void
trace1(TTM* ttm, int depth, int entering, int dumping)
{
    char tag[4];
    int i;
    Frame* frame;

    if(ttm->stacknext == 0) {
	fprintf(stderr,"trace: no frame to trace\n");
	return;
    }	

    frame = &ttm->stack[depth];

    i = 0;
    tag[i++] = (char)ttm->sharpc;
    if(frame->active)
	tag[i++] = (char)ttm->sharpc;
    tag[i++] = (char)ttm->openc;
    tag[i] = NUL;
    fprintf(stderr,"[%02d] ",ttm->stacknext-1);
    if(dumping)
        fprintf(stderr,"%s: ",(entering?"begin":"end"));
    fprintf(stderr,"%s",tag);
    for(i=0;i<frame->argc;i++) {
	if(i > 0) fprintf(stderr,"%c",ttm->semic);
        fprintf(stderr,"%s",frame->argv[i]);
    }
    fprintf(stderr,"%c\n",ttm->closec);
}

/**
Dump the stack
*/
static void
dumpstack(TTM* ttm)
{
    int i;
    for(i=ttm->stacknext-1;i>=0;i--) {
	Frame* frame;
	frame = &ttm->stack[i];
        trace(ttm,0,1);
    }
}

/**************************************************/
/* Main() Support functions */

static int
getOptionStringLength(char** list)
{
    int i;
    char** p;
    for(i=0,p=list;*p;i++,p++);
    return i;
}

static int
pushOptionString(char* option, int max, char** list)
{
    int i;
    for(i=0;i<max;i++) {
	if(list[i] == NULL) {
	    list[i] = (char*)malloc(strlen(option)+1);
	    strcpy(list[i],option);
	    return 1;
	}
    }
    return 0;
}

static void
initglobals()
{
    memset((void*)includes,0,sizeof(includes));
    memset((void*)eoptions,0,sizeof(eoptions));
    memset((void*)argoptions,0,sizeof(argoptions));
}

static void
usage(void)
{
    fprintf(stderr,"ttm [-B integer] [-d string] [-D name=string] [-e string] [-i] [-I directory] [-o file] [-V] [--] [file] [arg...]\n");
    fprintf(stderr,"\tOptions may be repeated\n");
    exit(0);
}

static void
convertDtoE(const char* def)
{
    char* macro;
    char* sep;
    macro = (char*)malloc(strlen(def)+strlen("##<ds;;>")+1);
    if(macro == NULL) fail(NOTTM,EMEMORY);
    strcpy(macro,"##<ds;");
    strcat(macro,def);
    sep = strchr(def,'=');
    if(sep != NULL)
	*sep++ = ';';
    else
        strcat(macro,";");
    strcat(macro,">");
    pushOptionString(macro,MAXEOPTIONS,eoptions);    
    free(macro);
}

static void
readinput(TTM* ttm, const char* filename,Buffer* bb)
{
    char* content = NULL;
    FILE* f = NULL;
    int isstdin = 0;
    int i;
    unsigned long buffersize = bb->alloc;

    if(strcmp(filename,"-") == 0) {
	/* Read from stdinput */
	isstdin = 1;
	f = stdin;	
    } else {
	f = fopen(filename,"r");
	if(f == NULL) {
	    fprintf(stderr,"Cannot read file: %s\n",filename);
	    exit(1);
	}
    }
    
    resetBuffer(ttm,bb);
    content = bb->content;

    /* Read character by character until EOF */
    for(i=0;i<buffersize-1;i++) {
	int c = fgetc(f);
	if(c == EOF) break;
	content[i] = (char)c;
    }
    content[i] = NUL;
    setBufferLength(ttm,bb,i);
    if(!isstdin) fclose(f);
}

static int
readbalanced(TTM* ttm)
{
    Buffer* bb;
    char* content;
    int i, depth, iseof;

    unsigned long buffersize;

    bb = ttm->buffer;
    resetBuffer(ttm,bb);
    buffersize = bb->alloc;
    content = bb->content;

    /* Read character by character until EOF; take escapes and open/close
       into account */
    for(iseof=0,depth=0,i=0;i<buffersize-1;) {
	int c = fgetc(stdin);
	content[i++] = (char)c;
	if(c == EOF) {iseof=1; break;}
        if(c == ttm->escapec) {
	    c = fgetc(stdin);
	    if(c == EOF) break;
	    content[i] = c;
	} else if(c == ttm->openc) depth++;
	else if(c == ttm->closec) {
	    depth--;
	    if(depth == 0) {
		while((c=fgetc(stdin))) {if(c == '\n') break;}
		break;
	    }
	}
    }
    setBufferLength(ttm,bb,i);
    return (i == 0 && iseof ? 0 : 1);
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

    fseek(file,0,SEEK_SET); /* end of the file */
    filesize = ftell(file); /* get file length */
    rewind(file);
 
    expandBuffer(ttm,bb,(unsigned long)filesize); /* make room for the file input */
    fread(bb->content,filesize,1,file);
    if(ferror(file)) return 0;
    bb->active += (unsigned long)filesize;
    return 1;
}

/**************************************************/
/* Main() */

static char* options = "B:d:D:e:iI:o:S:V-";

int
main(int argc, char** argv)
{
    int i,exitcode;
    long long buffersize = 0;
    long long stacksize = 0;
    char* debugargs = strdup("");
    int interactive = 0;
    char* outputfilename = NULL;
    char* inputfilename = NULL;
    int isstdout = 1;
    FILE* outputfile = NULL;
    TTM* ttm = NULL;
    int c;

    if(argc == 1)
	usage();

    initglobals();

    while ((c = getopt(argc, argv, options)) != EOF) {
	switch(c) {
	case 'B':
	    if(buffersize == 0) {
		ERR err = toInt64(optarg,&buffersize);
		if(err != ENOERR || buffersize < 0)
		    fatal(NOTTM,"Illegal buffersize");
	    } break;
	case 'S':
	    if(stacksize == 0) {
		ERR err = toInt64(optarg,&stacksize);
		if(err != ENOERR || stacksize < 0)
		    fatal(NOTTM,"Illegal stacksize");
	    } break;
	case 'd':
	    if(debugargs == NULL)
	        debugargs = strdup(optarg);
	    break;
	case 'D':
	    /* Convert the -D to a -e */
	    convertDtoE(optarg);
	    break;
	case 'e':
	    pushOptionString(optarg,MAXEOPTIONS,eoptions);
	    break;
	case 'i':
	    interactive = 1;
	    break;
	case 'I':
	    if(optarg[strlen(optarg)-1] == '/')
	        optarg[strlen(optarg)-1] = NUL;
	    pushOptionString(optarg,MAXINCLUDES,includes);
	    break;
	case 'o':
	    if(outputfilename == NULL)
		outputfilename = strdup(optarg);
	    break;
	case 'V':
	    printf("TTM Version 1.0\n");
	    exit(0);
	case '-':
	    break;
	case '?':
	default:
	    usage();
	}
    }

    /* Extract the file to execute and its args, if any */
    if(optind < argc) {
	inputfilename = strdup(argv[optind++]);
	if(strcmp("-",inputfilename) == 0) {/* reading from stdin */	
	    /* Complain if interactive and input file name is - */
	    if(interactive) {
		fprintf(stderr,"Interactive illegal if input is from stdin\n");
		exit(1);
	    }
	}
        for(;optind < argc;optind++)
	    pushOptionString(argv[optind++],MAXARGS,argoptions);
    }

    /* Complain if interactive and output file name specified */
    if(outputfilename != NULL && interactive) {
	fprintf(stderr,"Interactive is illegal if output file specified\n");
	exit(1);
    }

    if(buffersize < MINBUFFERSIZE)
	buffersize = MINBUFFERSIZE;	    
    if(stacksize < MINSTACKSIZE)
	stacksize = MINSTACKSIZE;	    

    if(outputfilename == NULL) {
	outputfile = stdout;
	isstdout = 1;
    } else {
	outputfile = fopen(outputfilename,"w");
	if(outputfile == NULL) {
	    fprintf(stderr,"Output file is not writable: %s\n",outputfilename);
	    exit(1);
	}	    
	isstdout = 0;
    }

    /* Create the ttm state */
    ttm = newTTM(buffersize,stacksize);
    ttm->output = outputfile;

    defineBuiltinFunctions(ttm);

    /* Execute the -e strings in turn */
    for(i=0;eoptions[i] != NULL;i++) {
	char* content;
	resetBuffer(ttm,ttm->buffer);
	setBufferLength(ttm,ttm->buffer,strlen(eoptions[i]));
	content = ttm->buffer->content;
	strcpy(content,eoptions[i]);
	scan(ttm);
	if(ttm->flags & FLAG_EXIT)
	    goto done;
    }

    /* Now execute the inputfile, if any */
    if(inputfilename != NULL) {
        readinput(ttm,inputfilename,ttm->buffer);
        scan(ttm);
	if(ttm->flags & FLAG_EXIT)
	    goto done;
    }    

    /* If interactive, start read-eval loop */
    if(interactive) {
	for(;;) {
	    if(!readbalanced(ttm)) break;
            scan(ttm);
	    if(ttm->flags & FLAG_EXIT)
	        goto done;
	}
    }

done:
    /* Dump any output left in the buffer */
    printbuffer(ttm);

    exitcode = ttm->exitcode;

    /* cleanup */
    if(!isstdout) fclose(outputfile);    
    freeTTM(ttm);

    exit(exitcode);
}

/**************************************************/
/**************************************************/
/**
Implement functions that deal with Linux versus Windows
differences.
*/

/**************************************************/

#ifdef _MSC_VER

static unsigned long long
getRunTime(void)
{
    unsigned long long runtime;
    FILETIME ftCreation,ftExit,ftKernel,ftUser;

    GetProcessTimes(GetCurrentProcess(),
		    &ftCreation, &ftExit, &ftKernel, &ftUser);

    runtime = (ftUser.dwHighDateTime << 32 | ftUser.dwLowDateTime);
    /* Divide by 10000 to get milliseconds from 100 nanosecond ticks */
    runtime /= 10000;
    return runtime;
}


#else /*!_MSC_VER */

static long frequency = 0;

static unsigned long long
getRunTime(void)
{
    unsigned long long runtime;
    struct tms timers;

    if(frequency == 0) 
        frequency = sysconf(_SC_CLK_TCK);

    (void)times(&timers);
    runtime = timers.tms_utime; /* in clock ticks */
    runtime = ((runtime * 1000) / frequency) ; /* runtime in milliseconds */    
    return runtime;
}

#endif /*!_MSC_VER */


/**************************************************/
/* Getopt */

#ifdef _MSC_VER
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
getopt(int argc, char *argv[], char *optstring)
{
    static char *next = NULL;
    char c;
    char *cp = malloc(sizeof(char)*1024);

    if (optind == 0)
        next = NULL;
    optarg = NULL;
    if (next == NULL || *next == ('\0')) {
        if (optind == 0)
            optind++;
        if (optind >= argc
	    || argv[optind][0] != ('-')
            || argv[optind][1] == ('\0')) {
            optarg = NULL;
            if (optind < argc)
                optarg = argv[optind];
            return EOF;
        }
        if (strcmp(argv[optind], "--") == 0) {
            optind++;
            optarg = NULL;
            if (optind < argc)
                optarg = argv[optind];
            return EOF;
        }
        next = argv[optind];
        next++;     // skip past -
        optind++;
    }
    c = *next++;
    cp = strchr(optstring, c);
    if (cp == NULL || c == (':'))
        return ('?');
    cp++;
    if (*cp == (':')) {
        if (*next != ('\0')) {
            optarg = next;
            next = NULL;
        } else if (optind < argc) {
            optarg = argv[optind];
            optind++;
        } else {
            return ('?');
        }
    }
    return c;
}
#endif /*_MSC_VER*/
