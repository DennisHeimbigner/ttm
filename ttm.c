/**
This software is released under the terms of the Apache License version 2.
For details of the license, see http://www.apache.org/licenses/LICENSE-2.0.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

/**************************************************/
/**
(Un)Define these if you have/have-not the specified function
This is in lieu of the typical config.h.
*/

#define HAVE_MEMMOVE 

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

#define SEGMENT 0xff /* Not a legal value in utf-8 and not NUL */
#define UNIVERSAL_ESCAPE SEGMENT

#define MAXARGS 255
#define MAXINCLUDES 1024
#define MAXEOPTIONS 1024

#define NUL '\0'


#define MINBUFFERSIZE 1024

#define MINSTACKSIZE 1024

/*Mnemonics*/
#define NESTED 1

/* Debug Flags */

#define FLAG_TRACE 1

/**************************************************/
/* Error functions */
#define ebuffersize() fail0("Buffer overflow")
#define ememory(where) fail1("Out of memory: %s",where)
#define enullfcnname() fail0("Null function name")
#define enofunction(f) fail1("Undefined function: %s",f)
#define efewargs(f) fail1("Too few arguments to function: %s",f)
#define estackunderflow() fail0("Stack underflow")
#define ebadoption(o) fail1("Illegal option: %s",o)
#define etoomanyincludes(f) fail1("Too many includes: %s",f)
#define ebadbuffersize() fail0("Illegal buffer size")

#define fail0(msg) fail(msg,"","")
#define fail1(msg,arg1) fail(msg,arg1,"")
#define fail2(msg,arg1,arg2) fail(msg,arg1,arg2)

static void
fail(char* msg0, char* arg1, char* arg2)
{
    char* msg1 = (char*)malloc(strlen(msg0)+strlen("fatal error: ")+1+1);
    strcpy(msg1,"Fatal error: ");
    strcat(msg1,msg0);
    strcat(msg1,"\n");
    fprintf(stderr,msg1,arg1,arg2);
    exit(1);
}

/**************************************************/
/* Misc. #defines */

#define ismultibyte(c) (((c) & 0x80) == 0x80 ? 1 : 0)

#define iswhitespace(c) ((c) <= ' ' ? 1 : 0)

#define iscontrol(c) ((c) < ' ' || (c) == 127 ? 1 : 0)

#define isdec(c) (((c) >= '0' && (c) <= '9') ? 1 : 0)

#define ishex(c) ((c >= '0' && c <= '9') \
		  || (c >= 'a' && c <= 'f') \
		  || (c >= 'A' && c <= 'F') ? 1 : 0)

/**************************************************/
/**
Forward definitions
*/

typedef struct TTM TTM;
typedef struct Function Function;
typedef struct Frame Frame;
typedef struct Buffer Buffer;

static void scan(TTM* ttm);
static void scanR(TTM* ttm, Buffer* bb, int nested);
static unsigned long scanopenclose(TTM* ttm, Buffer* bb);
static void exec(TTM* ttm, Buffer* bb);
static void parseinvocation(TTM* ttm, Buffer* bb, Frame* frame);
static char* call(TTM* ttm, Frame* frame, char* body);
static void passEscape(Buffer* bb);
static int getOptionStringLength(char** list);
static int pushOptionString(char* option, int max, char** list);
static int readfile(FILE* file, Buffer* bb);

/**************************************************/
/* Common typedefs */

typedef char* (*TTMFCN)(TTM*, Frame*);

/**************************************************/
/* Global variables */

static char* includes[MAXINCLUDES+1]; /* null terminated */
static char* eoptions[MAXEOPTIONS+1]; /* null terminated */
static char* argoptions[MAXARGS+1]; /* null terminated */

/**************************************************/
/**
Structures with associated operations
*/

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
    char* pos; /* into content */
    char* end; /* into content */
    char* content;
};

/* Macro defined procedures */
#define Bavail(bb) ((bb)->alloc - (bb)->length)
#define Bensure(bb,len) {if(Bavail(bb) < len) ebuffersize();}

static Buffer*
newBuffer(unsigned long buffersize)
{
    Buffer* bb;
    bb = (Buffer*)calloc(1,sizeof(Buffer));
    if(bb == NULL) ememory("buffer");
    bb->content = (char*)malloc(buffersize);
    if(bb->content == NULL) ememory("buffer");
    bb->alloc = buffersize;
    bb->length = 0;
    bb->pos = bb->content;
    bb->end = bb->pos;
    return bb;
}

static void
freeBuffer(Buffer* bb)
{
    assert(bb != NULL);
    if(bb->content != NULL)
	free(bb->content);
    free(bb);
}

/* Make room for a string of length n at current position. */
static void
expandBuffer(Buffer* bb, unsigned long len)
{
    assert(bb != NULL);
    Bensure(bb,len); /* ensure enough space */
    if(bb->pos < bb->end) {
        /* make room for len characters */
	makespace(bb->content+bb->length,bb->content+bb->length,len);
    }
    bb->length += len;
    bb->end = bb->content+bb->length;
    *(bb->end) = NUL;
}

/* Remove len characters at current position */
static void
compressBuffer(Buffer* bb, unsigned long len)
{
    assert(bb != NULL);
    if(len > 0 && bb->pos < bb->end) {
	memcpy(bb->pos,bb->pos+len,len);	
    }    
    bb->length -= len;
    bb->end = bb->content+bb->length;
    *(bb->end) = NUL;
}

/* Reset buffer and set content */
static void
resetBuffer(Buffer* bb)
{
    assert(bb != NULL);
    bb->length = 0;
    bb->pos = bb->content;
    bb->end = bb->content;
}

/* get content */
static char*
getBufferContent(Buffer* bb)
{
    assert(bb != NULL);
    return bb->content;
}

static void
setBufferLength(Buffer* bb, unsigned long len)
{
    assert(bb != NULL);
    if(len >= bb->alloc) ebuffersize();
    bb->length = len;
    bb->end = bb->content+bb->length;
    *(bb->end) = NUL; /* make sure */    
}

/**************************************************/
/**
  Define a ttm frame
*/

struct Frame {
  char* argv[MAXARGS+1];
  int argc;
  int active; /* 1 => # 0 => ## */
};

static void
initFrame(Frame* frame)
{
    frame->argc = 0;
    frame->active = 0;
}

static void
clearFrame(Frame* frame)
{
    int i;
    for(i=0;i<frame->argc;i++) {
	if(frame->argv[i] != NULL) {
	    free(frame->argv[i]);
	    frame->argv[i] = NULL;
	}
    }
    frame->argc = 0;
}

/**************************************************/
/**
Function definition
*/

struct Function {
    char* name;
    int builtin;
    int minargs;
    TTMFCN fcn; /* builtin == 1 */
    char* body; /* builtin == 0 */
    Function* next; /* "hash" chain */
};

static Function*
newFunction(void)
{
    Function* fcn = (Function*)calloc(1,sizeof(Function));
    if(fcn == NULL) ememory("function");
    return fcn;    
}

static void
freeFunction(Function* f)
{
    assert(f != NULL);
    if(f->name != NULL) free(f->name);
    if(!f->builtin && f->body != NULL) free(f->body);
    free(f);
}

/**
Hash Table Management.
The function hash table is simplified by
making it a array of chains
indexed by the first char of the name
of the function.
*/

static void
hashInsert(Function** table, Function* fcn)
{
    Function* next;
    int c0 = fcn->name[0];
    next = table[c0];
    if(next == NULL) {
	table[c0] = fcn;
    } else {
	Function* prev = NULL;
	/* search chain for fcn */
        while(next != NULL) {
	    if(strcmp(fcn->name,next->name)==0)
		break;
	    prev = next;
	    next = next->next;
	}
	if(next == NULL) {/* fcn not previously defined */
	    prev->next = fcn;
	} else {/* replace existing definition */
	    Function* chain = next->next; /* save */
	    freeFunction(next);
	    prev->next = fcn;
	    fcn->next = chain;
	}
    }
}

static Function*
hashLookup(Function** table, char* name)
{
    Function* f;
    int c0;
    assert(name != NULL);
    c0 = name[0];
    f = table[c0];
    while(f != NULL) {/* search chain for fcn */
	if(strcmp(name,f->name)==0)
	    break;
	f = f->next;
    }
    return f;
}

static Function*
hashRemove(Function** table, char* name)
{
    Function* next;
    Function* prev;
    int c0;
    assert(name != NULL);
    c0 = name[0];
    next = table[c0];
    prev = NULL;
    while(next != NULL) {/* search chain for fcn */
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
/**
TTM state object
*/

struct TTM {
    unsigned long buffersize;
    unsigned long flags;
    int sharpc; /* sharp-like char */
    int openc; /* <-like char */
    int closec; /* >-like char */
    int semic; /* ;-like char */
    int escapec; /* escape-like char */
    Buffer* buffer; /* contains the string being processed */
    Function* functions[256]; /* indexed by fcn->name[0] */
    Frame* stack;    
    unsigned long stackalloc;
    int stacknext; /* |stack| == (stacknext) */
    FILE* output;    
};

static TTM*
newTTM(unsigned long buffersize)
{
    TTM* ttm = (TTM*)calloc(1,sizeof(TTM));
    if(ttm == NULL) ememory("TTM");
    ttm->buffersize = buffersize;
    ttm->sharpc = '#';
    ttm->openc = '<';
    ttm->closec = '>';
    ttm->semic = ';';
    ttm->escapec = '\\';
    ttm->buffer = newBuffer(buffersize);
    ttm->stacknext = 0;
    ttm->stackalloc = MINSTACKSIZE;
    ttm->stack = (Frame*)malloc(sizeof(Frame)*ttm->stackalloc);
    if(ttm->stack == NULL) ememory("stacksize");
    return ttm;
}

static void
freeTTM(TTM* ttm)
{
    freeBuffer(ttm->buffer);
    free(ttm);
}

/* Manage the frame stack */
static Frame*
pushFrame(TTM* ttm)
{
    Frame* frame;
    if(ttm->stacknext >= ttm->stackalloc) {
	assert(ttm->stack != NULL);
	/* Double in size every time */
	realloc(ttm->stack,sizeof(Frame)*ttm->stackalloc*2);
	if(ttm->stack == NULL) ememory("Stack");
    }
    frame = &ttm->stack[ttm->stacknext];
    initFrame(frame);
    ttm->stacknext++;
    return frame;
}

static Frame*
popFrame(TTM* ttm)
{
    Frame* frame;
    if(ttm->stacknext == 0) estackunderflow();
    ttm->stacknext--;
    if(ttm->stacknext == 0)
	frame = NULL;
    else
	frame = &ttm->stack[ttm->stacknext-1];    
    return frame;
}

/**************************************************/

/**
This is the basic scanner main entry point.
*/
static void
scan(TTM* ttm)
{
    scanR(ttm,ttm->buffer,!NESTED);
}

/**
This is recursive helper for the basic scanner. It is recursive
with respect to exec/parseinvocation.
*/
static void
scanR(TTM* ttm, Buffer* bb, int nested)
{
    for(;;) {
	int c = *bb->pos; /* NOTE that we do not bump here */
	if(c == NUL) { /* End of buffer */
	    break;
	} else if(c == ttm->escapec) {
	    passEscape(bb);
	} else if(nested
		  && (c == ttm->semic || c == ttm->closec)) {
	    /* let next level up deal with the closing char */
	    break;
	} else if(c == ttm->sharpc) {
	    char* begincall;
	    begincall = bb->pos;
	    /* check for invocation */
	    if(bb->pos[1] == ttm->openc
	       || (bb->pos[1] == ttm->sharpc
                    && bb->pos[2] == ttm->openc)) {
		exec(ttm,bb);
	    } else {
		/* not an invocation */
	    }
	} else if(c == ttm->openc) {
	    scanopenclose(ttm,bb);
	    break;	    
	} else {
	    /* keep moving */
	    bb->pos++;
	}
    }
}

/**
Capture beginning and end of balanced <...>
Return length starting from bb->pos.
*/
static unsigned long
scanopenclose(TTM* ttm, Buffer* bb)
{
    /* Scan for matching close; take into account nesting and escape */
    int c;
    int depth = 0;
    char* start = bb->pos;

    c = *(bb->pos++);
    assert(c == ttm->openc);

    for(;;) {
        c = *(bb->pos);
        if(c == NUL) ebuffersize(); /* End of buffer */
        if(c == ttm->escapec) {
	    passEscape(bb);
	    /* leaves bb->pos past escaped char */
        } else if(c == ttm->openc) {
	    bb->pos++;
            depth++;
        } else if(c == ttm->closec) {
	    bb->pos++;
            depth--;
            if(depth == 0)
	        break; /* we are done */
        } else {
	    bb->pos++;
        }
    }
    return (bb->pos - start);
}

static void
exec(TTM* ttm, Buffer* bb)
{
    Frame* frame;
    Function* fcn;
    char* result;

    frame = pushFrame(ttm);
    parseinvocation(ttm,bb,frame);
    if(frame->argc == 0) enullfcnname();
    if(strlen(frame->argv[0])==0) enullfcnname();
    /* Locate the function to execute */
    fcn = hashLookup(ttm->functions,frame->argv[0]);
    if(fcn == NULL) enofunction(frame->argv[0]);    
    if(fcn->minargs < (frame->argc - 1)) /* -1 to account for function name*/
	efewargs(fcn->name);
    if(fcn->builtin)
	result = fcn->fcn(ttm,frame);
    else /* invoke the pseudo function "call" */
	result = call(ttm,frame,fcn->body);
    if(result != NULL && result[0] != NUL) {
	/* insert the result */
	unsigned long len = strlen(result);
	expandBuffer(bb,len);
	memcpy(bb->pos,result,len);
	if(!frame->active)
	    bb->pos += len; /* skip past the result */
    }	
    clearFrame(frame);
    popFrame(ttm);
}


static void
parseinvocation(TTM* ttm, Buffer* bb, Frame* frame)
{
    char* begincall;
    char* endcall;
    int active;

    begincall = bb->pos;
    active = (bb->pos[1] == ttm->openc ? 1 : 0);
    bb->pos += (active ? 2 : 3);
    frame->argc = 0;
    frame->active = active;
    for(;;) {
        unsigned long len;
        char* arg;
        char* start = bb->pos;
        int lastc;
        scanR(ttm,bb,NESTED);
        /* extract the argument */
        lastc = *(bb->pos);
        len = (bb->pos - start);
        arg = (char*)malloc(len+1);
        memcpy((void*)arg,(void*)start,len);
        arg[len] = NUL;
        frame->argv[frame->argc] = arg;
        frame->argc++;
        /* skip past semi or close */
        bb->pos++;
        /* check to see if this was the last arg */
        if(lastc == ttm->closec)
            break;
    }
    endcall = bb->pos;
    /* Remove the call from the buffer */
    compressBuffer(bb,(begincall - endcall));
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
	} else
	    len++;
    }
    /* Compute the result */
    result = (char*)malloc(len+1);
    if(result == NULL) ememory("function body");
    dst[len] = NUL;
    dst = result;    
    for(p=body;(c=*p++);) {
	if(c == SEGMENT) {
	    int segindex = (int)(*p++);
	    if(segindex < frame->argc) {
		char* arg = frame->argv[segindex];
		unsigned long argsize = strlen(arg);
	        memcpy((void*)dst,(void*)arg,argsize);
		dst += argsize;
	    }
	} else
	    *dst++ = (char)c;
    }
    return result;
}

/**************************************************/
/* Builtins Support Functions */

static FILE*
includesLookup(char* filename)
{
    int i;
    FILE* file;
    int namelen = strlen(filename);

    /* first, try to look up the file directly */
    file = fopen(filename,"r");
    if(file != NULL)
	return file;
    /* if this is absolute, then fail */
    if(filename[0] == '/')
	return NULL;
    /* try prefixing includes in order */
    for(i=0;i<MAXINCLUDES;i++) {
	char* fullpath;
	int pathlen = strlen(includes[i]);
	if(includes[i] == NULL) return NULL;
	fullpath = (char*)malloc(pathlen+namelen+2);
	strcpy(fullpath,includes[i]);
	if(fullpath[pathlen-1] != '/') strcat(fullpath,"/");
	strcat(fullpath,filename);
	file = fopen(fullpath,"r");
	if(file != NULL)
	    return file;
    }
    return NULL;
}

/**************************************************/
/* Builtin functions */

/**
 Builtin function table
*/

static char* 
ttm_ds(TTM* ttm, Frame* frame)
{
    Function* function = hashLookup(ttm->functions,frame->argv[0]);
    if(function == NULL) {
	/* create a new function object */
	function = newFunction();
	function->name = frame->argv[0];
	frame->argv[0] = NULL;
	hashInsert(ttm->functions,function);
    }
    function->builtin = 0;
    if(function->body != NULL)
	free(function->body);
    function->body = frame->argv[1];
    frame->argv[1] = NULL;
    return NULL;
}

static char*
ttm_argv(TTM* ttm, Frame* frame)
{
    int index = atoi(frame->argv[1]);
    
    if(index < 0 || index >= getOptionStringLength(argoptions))
	fail1("#<argv>: illegal index: %s",frame->argv[1]);
    return strdup(argoptions[index]);
}

static char*
ttm_ss(TTM* ttm, Frame* frame)
{
    Function* function = hashLookup(ttm->functions,frame->argv[1]);
    int bodylen;
    char* body;
    int i;

    if(function == NULL)
	fail1("#<ss>: Undefined function: %s",frame->argv[1]);
    if(function->builtin)
	fail1("#<ss>: Function is builtin: %s",frame->argv[1]);
    /* The number of expected args is the same as the #segment marks
       given to this function, even if some of the marks do not
       occur in the body of the function
    */
    function->minargs = (frame->argc - 2);
    body = function->body;
    bodylen = strlen(body);
    for(i=2;i<frame->argc;i++) {
	int alen = strlen(frame->argv[i]);
	if(alen == 0)
	    fail1("#<ss>: Zero-length segment string: %s",frame->argv[i]);	
	if(alen > bodylen)
	    fail2("#<ss>: Segment string longer than function(%s) body: %s",frame->argv[1],frame->argv[i]);	
    }
    for(i=2;i<frame->argc;i++) {
	char* arg = frame->argv[i];
	int arglen = strlen(arg);
	int pos=0;
        int endpos = (bodylen - arglen);
	
	/* Search for occurrences of arg */
	while(pos < endpos) {
	    if(memcmp((void*)body+pos,(void*)arg,arglen) != 0)
		continue;
	    /* we have a match, replace match by a segment marker */
	    /* make sure we have 2 characters of space */
	    makespace(body+pos+2,body+pos,2);
	    body[pos++] = SEGMENT;
	    body[pos++] = (char)((i-1)&0xFF);
	    /* compress out the argument */
	    memcpy(body+pos,body+pos+arglen,arglen);
	    /* fix up */
	    bodylen -= (arglen - 2);
            endpos = (bodylen - arglen);
	}
    }
    return NULL;
}

static char*
ttm_include(TTM* ttm, Frame* frame)
{
    char** path;
    FILE* finclude;
    char* suffix;
    char filename[8192];
    char* startpos;
    Buffer* bb = ttm->buffer;

    suffix = frame->argv[1];
    if(strlen(suffix) == 0)
	fail1("#<include>: empty include file name: %s",suffix);
    /* try to open file as is */
    finclude = fopen(filename,"r");
    if(finclude == NULL && suffix[0] == '/')
	fail1("#<include>: Cannot read include file: %s",suffix);	
    /* Since it is a relative path, try to use -I list */
    for(path=includes;*path;) {
	strcpy(filename,*path);
	strcat(filename,"/");
	strcat(filename,suffix);
	finclude = fopen(filename,"r");
        if(finclude != NULL) break;
    }
    if(finclude == NULL)
	fail1("#<include>: Cannot find include file: %s",suffix);	
    startpos = bb->pos;
    if(!readfile(finclude,bb))
	fail1("#<include>: Cannot read include file: %s",filename);
    if(frame->active)
	bb->pos = startpos;
    return NULL;
}

static char* ttm_if(TTM* ttm, Frame* frame)
{
    return NULL;
}

static struct Builtin {
char* name;
int minargs;
TTMFCN fcn;
};

/* Define a subset of the original TTM functions */
static struct Builtin builtin_orig[] = {
/* Dictionary Operations */
{"ap",2,ttm_ap},
{"cf",2,ttm_cf},
{"cr",2,ttm_cr},
{"ds",2,ttm_ds},
{"es",2,ttm_es},
{"sc",2,ttm_sc},
{"ss",2,ttm_ss},
/* String Selection */
cc
cn
cp
cs
isc
rrp
scn
/* String Scanning Operations */
gn
zlc
zlcp
/* Character Class Operations */
ccl
dcl
dncl
ecl
scl
tcl
/* Arithmetic Operations */
abs
ad
dv
dvr
mu
su

{NULL,0,NULL} /* terminator */
};

/* Functions new to this implementation */
static struct Builtin builtin_new[] = {
{"argv",2,ttm_argv},
{"include",2,ttm_include},
{NULL,0,NULL} /* terminator */
};

static void
defineBuiltinFunction1(TTM* ttm, struct Builtin* bin)
{
    /* Make sure we did not define builtin twice */
    Function* function = hashLookup(ttm->functions,bin->name);
    assert(function == NULL);
    /* create a new function object */
    function = newFunction();
    function->builtin = 1;
    function->name = strdup(bin->name);
    function->minargs = bin->minargs;
    function->fcn = bin->fcn;
    hashInsert(ttm->functions,function);
}

static void
defineBuiltinFunctions(TTM* ttm)
{
    struct Builtin* bin;
    for(bin=builtin_orig;bin->name != NULL;bin++)
	defineBuildinFunction1(ttm,bin);
    for(bin=builtin_new;bin->name != NULL;bin++)
	defineBuildinFunction1(ttm,bin);
}

/**************************************************/
/* Utility functions */

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

static void
passEscape(Buffer* bb)
{
    int c;

    bb->pos++; /* move past the escape char */
    c = *(bb->pos);
    /* de-escape and store */
    switch (c) {
    case 'r': c = '\r'; break;
    case 'n': c = '\n'; break;
    case 't': c = '\t'; break;
    case 'b': c = '\b'; break;
    case 'f': c = '\f'; break;
    default: 
	/* Leave the escape and the following char */
	*(bb->pos) = UNIVERSAL_ESCAPE;
	bb->pos++;
	*(bb->pos) = c;
	bb->pos++;
	break;
    }
    if(bb->pos > bb->end) bb->pos = bb->end;
}

static int
fromhex(int c)
{
    if(c >= '0' && c <= '9') return (c - '0');
    if(c >= 'a' && c <= 'f') return ((c - 'a') + 10);
    if(c >= 'A' && c <= 'F') return ((c - 'a') + 10);
    return -1;
}


/**
Trace a single frame.
*/
static void
tracestack(TTM* ttm, Frame* frame, int depth, int entering, int dumping)
{
    char tag[4];
    int i;

    i = 0;
    tag[i++] = (char)ttm->sharpc;
    if(frame->active)
	tag[i++] = (char)ttm->sharpc;
    tag[i++] = (char)ttm->openc;
    tag[i] = NUL;
    fprintf(stderr,"[%02d] ",depth);
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
        tracestack(ttm,frame,i,0,1);
    }
}

/**************************************************/
/* Getopt */

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

static char* optarg;		// global argument pointer
static int   optind = 0; 	// global argv index

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

static int
fromdecimal(int c)
{
    return (c = '0');
}

static unsigned long
getsize(char* arg)
{
    int alen = strlen(arg);
    unsigned long base;

    if(alen == 0 || !isdec(*arg)) ebadbuffersize();
    base = atoi(arg);
    if(arg[alen-1] == 'm' || arg[alen-1] == 'M')
	base = base * (2<<20);	
    if(base < 0) ebadbuffersize();
    return base;
}

static void
convertDtoE(char* def)
{
    char* macro;
    char* sep;
    macro = (char*)malloc(strlen(def)+strlen("##<ds;;>")+1);
    if(macro == NULL) ememory("startup");
    strcpy(macro,"##<ds;");
    sep = strchr(def,'=');
    if(sep == NULL)
	sep = def+strlen(def);
    else
	*sep++ = NUL;
    strcat(macro,def);
    strcat(macro,";");
    strcat(macro,sep);
    strcat(macro,">");
    pushOptionString(macro,MAXEOPTIONS,eoptions);    
    free(macro);
}

static void
readinput(const char* filename,Buffer* bb)
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
    
    /* setup buffer */
    resetBuffer(bb);
    content = getBufferContent(bb);

    /* Read character by character until EOF */
    for(i=0;i<buffersize-1;i++) {
	int c = fgetc(f);
	if(c == EOF) break;
	content[i] = (char)c;
    }
    content[i] = NUL;
    setBufferLength(bb,i);
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
    resetBuffer(bb);

    buffersize = bb->alloc;

    /* setup buffer */
    resetBuffer(bb);
    content = getBufferContent(bb);

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
    setBufferLength(bb,i);
    return (i == 0 && iseof ? 0 : 1);
}

static void
printbuffer(TTM* ttm)
{
    FILE* output = ttm->output;
    Buffer* bb = ttm->buffer;
    int c;
    char* p;

    if(bb->length == 0) return;
    for(p=getBufferContent(bb);(c=*p++);) {
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

static int
readfile(FILE* file, Buffer* bb)
{
    long filesize;

    fseek(f,0,SEEK_SET); /* end of the file */
    filesize = ftell(f); /* get file length */
    rewind(f);
 
    expandBuffer(bb,(unsigned long)filesize); /* make room for the file input */
    fread(getBufferContent(bb),filesize,1,file);
    if(ferror(file)) return 0;
    bb->pos += (unsigned long)filesize;
    return 1;
}

/**************************************************/
/* Main() */

static char* options = "B:d:D:e:iI:o:V-";

int
main(int argc, char** argv)
{
    int i;
    unsigned long buffersize = MINBUFFERSIZE;
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

    while ((c = getopt(argc, argv, options)) != EOF) {
	switch(c) {
	case 'B':
	    if(buffersize == 0)
		buffersize = getsize(optarg);	    
	    break;
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

    if(buffersize == 0)
	buffersize = MINBUFFERSIZE;	    

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
    ttm = newTTM(buffersize);
    ttm->output = outputfile;

    defineBuiltinFunctions(ttm);

    /* Execute the -e strings in turn */
    for(i=0;eoptions[i] != NULL;i++) {
	char* content;
	resetBuffer(ttm->buffer);
	setBufferLength(ttm->buffer,strlen(eoptions[i]));
	content = getBufferContent(ttm->buffer);	
	strcpy(content,eoptions[i]);
	scan(ttm);
    }

    /* Now execute the inputfile, if any */
    if(inputfilename != NULL) {
        readinput(inputfilename,ttm->buffer);
        scan(ttm);
    }    

    /* If interactive, start read-eval loop */
    if(interactive) {
	for(;;) {
	    if(!readbalanced(ttm)) break;
            scan(ttm);
	}
    }
    /* Dump any output left in the buffer */
    printbuffer(ttm);

    /* cleanup */
    if(!isstdout) fclose(outputfile);    
    freeTTM(ttm);
    exit(0);
}

