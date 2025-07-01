/**
This software is released under the terms of the Apache License version 2.
For details of the license, see http://www.apache.org/licenses/LICENSE-2.0.
*/

/**************************************************/

#define VERSION "1.0"

/**************************************************/

#define CATCH
/* Debug has a level attached: 0 is equivalent to undef */
#define DEBUG 0
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

/* Define if the equivalent of the standard Unix memmove() is available */
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
#include <locale.h>
#include <wchar.h>

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


/**************************************************/

#include "const.h"
#include "types.h"
#include "decls.h"
#include "macros.h"
#include "forward.h"
#include "hash.h"
#include "va.h"
#include "io.h"
#include "utf8.h"
#include "debug.h"
#include "builtins.h"

/**************************************************/
/* Provide subtype specific wrappers for the HashTable operations. */

static Function*
dictionaryLookup(TTM* ttm, const char* name)
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
dictionaryRemove(TTM* ttm, const char* name)
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
charclassLookup(TTM* ttm, const char* name)
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
charclassRemove(TTM* ttm, const char* name)
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

static Property*
newProperty(TTM* ttm, const char* key)
{
    Property* f = (Property*)calloc(1,sizeof(Property));
    if(f == NULL) FAIL(ttm,TTM_EMEMORY);
    assert(f->entry.name == NULL);
    f->entry.name = strdup(key);
    f->entry.hash = computehash(key);
    return f;
}

static void
freeProperty(TTM* ttm, Property* f)
{
    assert(f != NULL);
    nullfree(f->value);
    clearHashEntry(&f->entry);
    free(f);
}

static void
clearproperties(TTM* ttm, struct HashTable* props)
{
    size_t i;
    for(i=0;i<HASHSIZE;i++) {
	struct HashEntry* cur = props->table[i].next; /* First entry is a placeholder */
	while(cur != NULL) {
	    struct HashEntry* next = cur->next;
	    struct Property* p = (struct Property*)cur;
	    freeProperty(ttm,p);
	    cur = next;
	}
	props->table[i].next = NULL;
    }
    memset(props,0,sizeof(struct HashTable));
}

static const char*
propertyLookup(TTM* ttm, const char* key)
{
    struct HashTable* table = &ttm->tables.properties;
    struct HashEntry* prev;
    struct HashEntry* entry;
    const char* value = NULL;

    if(hashLocate(table,key,&prev)) {
	entry = prev->next;
	value = ((Property*)entry)->value;
    } /*else Not found */
    return value;
}

static Property*
propertyRemove(TTM* ttm, const char* key)
{
    struct HashTable* table = &ttm->tables.properties;
    struct HashEntry* prev;
    struct HashEntry* entry;
    Property* prop = NULL;

    if(hashLocate(table,key,&prev)) {
	entry = prev->next;
	hashRemove(table,prev,entry);
	entry->next = NULL;
	prop = (Property*)entry;
    } /*else Not found */
    return prop;
}

static int
propertyInsert(TTM* ttm, const char* key, const char* value)
{
    struct HashTable* table = &ttm->tables.properties;
    struct HashEntry* prev;
    Property* prop = NULL;

    if(!hashLocate(table,key,&prev)) { /* Not already exists */
        prop = newProperty(ttm,key);
	hashInsert(table,prev,(struct HashEntry*)prop);
    } else {
	prop = (Property*)prev->next;
    }
    nullfree(prop->value);
    prop->value = (char*)nulldup(value);
    return 1;
}

/**************************************************/

static TTM*
newTTM(struct Properties* initialprops)
{
    TTM* ttm = (TTM*)calloc(1,sizeof(TTM));
    if(ttm == NULL) return NULL;
    ascii2u8('#',ttm->meta.sharpc);
    ascii2u8(';',ttm->meta.semic);
    ascii2u8('\n',ttm->meta.metac);
    ascii2u8('<',ttm->meta.openc);
    ascii2u8('>',ttm->meta.closec);
    memcpy(ttm->meta.lbrc,ttm->meta.openc,sizeof(utf8cpa));
    memcpy(ttm->meta.rbrc,ttm->meta.closec,sizeof(utf8cpa));
    /* There are two different single character escape characters:
       1. TTM escape -- this is the escape used by e.g. scan() function.
       2. IO escape -- this is the escape character used to escape input/output characterss; normally '\\'.
    */
    ascii2u8('@',ttm->meta.escapec);

    ttm->vs.active = vsnew();
    ttm->vs.passive = vsnew();
    ttm->vs.tmp = vsnew();
    ttm->vs.result = vsnew();
    ttm->frames.top = -1;
    memset((void*)&ttm->tables.dictionary,0,sizeof(ttm->tables.dictionary));
    memset((void*)&ttm->tables.charclasses,0,sizeof(ttm->tables.charclasses));
    memset((void*)&ttm->tables.properties,0,sizeof(ttm->tables.properties));
#if DEBUG > 0
    ttm->debug.trace = TR_UNDEF;
#endif
    /* Fill in pre-defined properties */
    defaultproperties(ttm);
    cmdlineproperties(ttm);

    return ttm;
}

static void
freeTTM(TTM* ttm)
{
    clearFramestack(ttm);
    vsfree(ttm->vs.active);
    vsfree(ttm->vs.passive);
    vsfree(ttm->vs.tmp);
    vsfree(ttm->vs.result);
    clearDictionary(ttm,&ttm->tables.dictionary);
    clearcharclasses(ttm,&ttm->tables.charclasses);
    clearproperties(ttm,&ttm->tables.properties);
    closeio(ttm);
    nullfree(ttm->opts.programfilename);
    free(ttm);
}

/**************************************************/

/* Manage the frame stack */
static Frame*
pushFrame(TTM* ttm)
{
    Frame* frame;
    ttm->frames.top++;
    if(ttm->frames.top >= (int)ttm->properties.stacksize)
	FAIL(ttm,TTM_ESTACKOVERFLOW);
    frame = &ttm->frames.stack[ttm->frames.top];
    frame->argc = 0;
    frame->active = 0;
    frame->result = vsnew();
    return frame;
}

/**
Release contents of ttm->frames.results[ttm->frames.top]
*/
static void
popFrame(TTM* ttm)
{
    Frame* frame;
    int top = ttm->frames.top--;
    if(top < 0) FAIL(ttm,TTM_ESTACKUNDERFLOW); /* Actually an internal error */
    frame = &ttm->frames.stack[top];
    clearFrame(ttm,frame);
}

static void
clearFrame(TTM* ttm, Frame* frame)
{
    if(frame == NULL) return;
    clearArgv(frame->argv,frame->argc);
    vsfree(frame->result);
}

static void
clearFramestack(TTM* ttm)
{
    int i;
    int top = ttm->frames.top;
    for(i=0;i<=top;i++) {
	Frame* f = &ttm->frames.stack[i];
	clearFrame(ttm,f);
    }
}

static void
clearArgv(char** argv, size_t argc)
{
    size_t i;
    for(i=0;i<argc;i++) {
	char* arg = argv[i];
	if(arg == NULL) break; /* trailing null */
	free(arg);
	argv[i] = NULL;
    }
}

/**************************************************/

static Function*
newFunction(TTM* ttm, const char* name)
{
    Function* f = (Function*)calloc(1,sizeof(Function));
    if(f == NULL) FAIL(ttm,TTM_EMEMORY);
    f->fcn.nextsegindex = SEGINDEXFIRST;
    assert(f->entry.name == NULL);
    f->entry.name = strdup(name);
    return f;
}

static void
resetFunction(TTM* ttm, Function* f)
{
    if(f->fcn.body != NULL) vsfree(f->fcn.body);
    memset(&f->fcn,0,sizeof(struct FcnData));
    f->fcn.nextsegindex = SEGINDEXFIRST;
}

static void
freeFunction(TTM* ttm, Function* f)
{
    assert(f != NULL);
    resetFunction(ttm,f);
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
newCharclass(TTM* ttm, const char* name)
{
    Charclass* cl = (Charclass*)calloc(1,sizeof(Charclass));
    if(cl == NULL) FAIL(ttm,TTM_EMEMORY);
    assert(cl->entry.name == NULL);
    cl->entry.name = strdup(name);
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
clearcharclasses(TTM* ttm, struct HashTable* charclasses)
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

/* Return ptr to first char not matching charclass
   taking negative into account.
*/
static const char*
charclassmatch(const char* cp, const char* charclass, int negative)
{
    const char* p = NULL;
    const char* q = NULL;
    for(p=cp;*p;p+=u8size(p)) {
	q = strchr8(charclass,p);
	/* 4 cases: */
 	     if(q == NULL && !negative)	/* p not in class && !negative => !match */
		{break;}
	else if(q != NULL && !negative)	/* p in class     && !negative =>  match */
		{}
	else if(q == NULL &&  negative)	/* p not in class &&  negative =>  match */
		{}
	else if(q != NULL &&  negative)	/* p in class     &&  negative => !match */
		{break;}
    }
    return p;
}

/* Predefined Property enum detector */
static enum PropEnum
propenumdetect(const char* s)
{
    if(strcmp("stacksize",(const char*)s)==0) return PE_STACKSIZE;
    if(strcmp("execcount",(const char*)s)==0) return PE_EXECCOUNT;
    if(strcmp("showfinal",(const char*)s)==0) return PE_SHOWFINAL;
    if(strcmp("showcall",(const char*)s)==0)  return PE_SHOWCALL;
    return PE_UNDEF;
}

/* MetaEnum detector */
static enum MetaEnum
metaenumdetect(const char* s)
{
    if(strcmp("sharp",(const char*)s)==0) return ME_SHARP;
    if(strcmp("semi",(const char*)s)==0) return ME_SEMI;
    if(strcmp("escape",(const char*)s)==0) return ME_ESCAPE;
    if(strcmp("meta",(const char*)s)==0) return ME_META;
    if(strcmp("eol",(const char*)s)==0) return ME_META;
    if(strcmp("open",(const char*)s)==0) return ME_OPEN;
    if(strcmp("close",(const char*)s)==0) return ME_CLOSE;
    if(strcmp("lbr",(const char*)s)==0) return ME_LBR;
    if(strcmp("rbr",(const char*)s)==0) return ME_RBR;
    return ME_UNDEF;
}

/* MetaEnum detector */
static enum TTMEnum
ttmenumdetect(const char* s)
{
    if(strcmp("meta",s)==0) return TE_META;
    if(strcmp("info",s)==0) return TE_INFO;
    if(strcmp("name",s)==0) return TE_NAME;
    if(strcmp("class",s)==0) return TE_CLASS;
    if(strcmp("string",s)==0) return TE_STRING;
    if(strcmp("list",s)==0) return TE_LIST;
    if(strcmp("all",s)==0) return TE_ALL;
    if(strcmp("builtin",s)==0) return TE_BUILTIN;
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
    char* cp8;
    int ncp;

    TTMCP8SET(ttm); 
    for(;;) {
	TTMCP8SET(ttm); /* note that we do not bump here */
	if(isnul(cp8)) { /* End of buffer */
	    break;
	} else if(isascii(cp8) && strchr(NPIDEPTH0,*cp8)) {
	    /* non-printable ignored chars must be ASCII */
	    TTMCP8NXT(ttm);
	} else if(isescape(cp8)) {
	    TTMCP8NXT(ttm);
	    vsindexappendn(ttm->vs.passive,cp8,ncp); /* pass the escaped char */
	    TTMCP8NXT(ttm); /* skip escaped char */
	} else if(u8equal(cp8,ttm->meta.sharpc)) {/* Start of call? */
	    if(u8equal(peek(ttm->vs.active,1),ttm->meta.openc)
		|| (u8equal(peek(ttm->vs.active,1),ttm->meta.sharpc)
		    && (u8equal(peek(ttm->vs.active,2),ttm->meta.openc)))) {
		/* It is a real call */
		TTMCP8SET(ttm);
		err = exec(ttm);
		if(err != TTM_NOERR) EXIT(err);
		if(ttm->flags.exit) goto done;
	    } else {/* not an call; just pass the # along passively */
		vsindexappendn(ttm->vs.passive,cp8,ncp);
		TTMCP8NXT(ttm);
	    }
	} else if(u8equal(cp8,ttm->meta.lbrc)) { /* start of <...> escaping */
	    int depth = 1;
	    TTMCP8NXT(ttm); /* skip outermost '<' */
	    while(depth > 0) {
		if(isnul(cp8)) EXIT(TTM_EEOS); /* unexpected eof */
		if(isescape(cp8)) {
		    vsindexappendn(ttm->vs.passive,cp8,ncp); /* Keep the escape char */
		    TTMCP8NXT(ttm); /* Skip escape char */
		    if(isnul(cp8)) EXIT(TTM_EEOS); /* unexpected eof */
		    vsindexappendn(ttm->vs.passive,cp8,ncp); /* Keep the escaped char */
		    TTMCP8NXT(ttm); /* Skip escaped char */
		} else if(u8equal(cp8,ttm->meta.lbrc)) {
		    depth++;
		    vsindexappendn(ttm->vs.passive,cp8,ncp); /* Keep lbrc */
		    TTMCP8NXT(ttm); /* Skip lbrc */
		} else if(u8equal(cp8,ttm->meta.rbrc)) {
		    if(--depth > 0) { /* pass the rbrc */
			vsindexappendn(ttm->vs.passive,cp8,ncp); /* Keep rbrc */
		    }
		    TTMCP8NXT(ttm); /* Skip rbrc */
		} else { /*ordinary char */
		    vsindexappendn(ttm->vs.passive,cp8,ncp); /* Keep lbrc */
		    TTMCP8NXT(ttm); /* Skip lbrc */
		}
	    } /*<...> while*/
	} else { /* non-signficant character */
	    vsindexappendn(ttm->vs.passive,(char*)cp8,ncp);
	    TTMCP8NXT(ttm);
	}
    } /*scan for*/

    if(ttm->properties.showfinal && !ttm->flags.starting && ttm->flags.catchdepth == 0) {
	/* Print out final contents */
	printf("%s\n",vscontents(ttm->vs.passive));
    }

done:
    return THROW(err);
}

/**
Compute a function and leave the result into:
1. ttm->vs.active if the function is active or
2. ttm->vs.passive
@param ttm
@return TTMERR
*/
static TTMERR
exec(TTM* ttm)
{
    TTMERR err = TTM_NOERR;
    Frame* frame = NULL;
    Function* fcn = NULL;
    size_t savepassive;
    TRACE tracebefore = TR_UNDEF;
    char* cp8 = NULL;
    int ncp;

    UNUSED(ncp);

    if(ttm->properties.execcount-- <= 0) EXIT(TTM_EEXECCOUNT);

    frame = pushFrame(ttm);

    TTMCP8SET(ttm);    
    
    /* Skip to the start of the function name */
    if(u8equal(peek(ttm->vs.active,1),ttm->meta.openc)) {
	TTMCP8NXT(ttm); /* skip '#' */
	TTMCP8NXT(ttm); /* skip '<' */
	frame->active = 1;
    } else {
	TTMCP8NXT(ttm); /* skip '#' */
	TTMCP8NXT(ttm); /* skip '#' */
	TTMCP8NXT(ttm); /* skip '<' */
	frame->active = 0;
    }

    /* Parse and store relevant pointers into frame. */
    savepassive = vslength(ttm->vs.passive);
    collectargs(ttm,frame);
    vssetlength(ttm->vs.passive,savepassive);
    if(ttm->flags.exit) goto done;

    if(ttm->debug.debug > 1) {
	xprintf(ttm,"exec: ");
	dumpframe(ttm,frame);
    }

    /* Now execute this function, which will leave result in bb->result */
    if(frame->argc == 0) EXIT(TTM_EBADCALL);
    if(strlen((char*)frame->argv[0])==0) EXIT(TTM_EBADCALL);
    /* Locate the function to execute */
    fcn = dictionaryLookup(ttm,frame->argv[0]);
    if(fcn == NULL) EXIT(TTM_ENONAME);
    if(fcn->fcn.minargs > (frame->argc - 1)) /* -1 to account for function name*/
	EXIT(TTM_EFEWPARMS);
    /* Reset the result buffer */
    vsclear(ttm->vs.result);

    if(ttm->debug.trace == TR_UNDEF)
	tracebefore = fcn->fcn.trace;
    else
	tracebefore = ttm->debug.trace;

    /* Trace frame on entry */
    if(tracebefore == TR_ON)
	trace(ttm,TTM_NOERR,1,TRACING);

    if(fcn->fcn.builtin) {
	err = fcn->fcn.fcn(ttm,frame,ttm->vs.result);
	if(fcn->fcn.novalue) vsclear(ttm->vs.result);
    } else /* invoke the pseudo function "call" */
	call(ttm,frame,vscontents(fcn->fcn.body),ttm->vs.result);

    /* Trace exit result iff traced entry */
    if(tracebefore == TR_ON) {
	trace(ttm,err,0,TRACING);
    }
    
    if(err != TTM_NOERR && ttm->flags.catchdepth == 0) {
	vsclear(ttm->vs.result);
	EXIT(err);
    }

    if(ttm->properties.showcall && !ttm->flags.starting && ttm->flags.catchdepth == 0) {
	char* u8;
	/* Print out results of a function call */
	if(vslength(ttm->vs.result) > 0) {
	    printf("%s",vscontents(ttm->vs.result));
	    u8 = vsgetp(ttm->vs.result,vslength(ttm->vs.result)-1);
	    if(*u8 != '\n') printf("\n");
	}
    }

    if(ttm->flags.exit) goto done;

    /* Remove the scanned characters in ttm->vs.active (index => 0) */
    {
	size_t elide = (size_t)(vsindexp(ttm->vs.active) - vscontents(ttm->vs.active));
	vsremoven(ttm->vs.active,0,elide);
	assert(vsindex(ttm->vs.active)==0);
    }
    
    /* Now, put the result into the buffer */
    if(!fcn->fcn.novalue && vslength(ttm->vs.result) > 0) {
	/* We insert the result as follows:
	   frame->passive => insert in ttm->vs.passive
	   frame->active => insert at ttm->vs.active index
	*/
	if(frame->active) {
	    (void)vsinsertn(ttm->vs.active, 0, vscontents(ttm->vs.result), vslength(ttm->vs.result));
	} else { /*frame->passive*/
	    vsindexappendn(ttm->vs.passive,vscontents(ttm->vs.result),vslength(ttm->vs.result));
	}
	vsclear(ttm->vs.result);
	TTMCP8SET(ttm); /* update */
    }
done:
    if(err == TTM_NOERR || ttm->flags.catchdepth > 0)  popFrame(ttm);
    return THROW(err);
}

/**
Construct a frame; leave ttm->vs.active pointing just past the closing '>'
@param ttm
@param frame
@return TTMERR
*/
static TTMERR
collectargs(TTM* ttm, Frame* frame)
{
    TTMERR err = TTM_NOERR;
    int done,depth;
    char* argp = NULL;
    size_t argoff = 0;
    char* cp8 = NULL;
    int ncp;

#if 0
{
const char* p;
TTMCP8SET(ttm);
for(p=cp8;*p;p++) {
if(strchr("	 \r\n",*p) != NULL)
p++;
}
fprintf(stderr,"@@@ |%50s|\n",p);
dumpstack(ttm);
}
#endif
dumpstack(ttm);
    argoff = vsindex(ttm->vs.passive);
    done = 0; depth = 0;
    while(!done) { /* Loop until all args are collected */
	TTMCP8SET(ttm);
	if(isnul(cp8)) EXIT(TTM_EEOS); /* Unexpected end of buffer */
	if(isescape(cp8)) {
	    TTMCP8NXT(ttm);
	    vsindexappendn(ttm->vs.passive,cp8,ncp);
	    TTMCP8NXT(ttm);
	} else if(u8equal(cp8,ttm->meta.semic) || u8equal(cp8,ttm->meta.closec)) {
	    /* End of an argument */
	    /* move to next arg */
	    if(u8equal(cp8,ttm->meta.closec)) done=1;
	    if(frame->argc >= MAXARGS) EXIT(TTM_EMANYPARMS)
	    vsindexset(ttm->vs.passive,argoff);
	    argp = vsindexp(ttm->vs.passive);
	    frame->argv[frame->argc++] = strdup(argp);
	    vssetlength(ttm->vs.passive,argoff);
	    TTMCP8NXT(ttm); /* skip the semi or close */
	    if(!done)
		argoff = vsindex(ttm->vs.passive);
	} else if(u8equal(cp8,ttm->meta.sharpc)) {
	    /* check for call within call */
	    const char* peek1 = peek(ttm->vs.active,1);
	    const char* peek2 = peek(ttm->vs.active,2);
	    if(u8equal(peek1,ttm->meta.openc)
	       || (u8equal(peek1,ttm->meta.sharpc)
		   && u8equal(peek2,ttm->meta.openc))) {
		/* Recurse to compute inner call */
		TTMCP8SET(ttm);
		err = exec(ttm);
		if(err != TTM_NOERR) EXIT(err);
		TTMCP8SET(ttm);
		if(ttm->flags.exit) goto done;
	    }
	} else if(u8equal(cp8,ttm->meta.lbrc)) {/* <...> nested brackets */
	    depth = 1;
	    TTMCP8NXT(ttm); /* skip '<' */
	    for(;;) {
		if(isnul(cp8)) EXIT(TTM_EEOS); /* Unexpected EOF */
		if(isescape(cp8)) {
		    vsindexappendn(ttm->vs.passive,(char*)cp8,ncp); /* append escape */
		    TTMCP8NXT(ttm);
		    vsindexappendn(ttm->vs.passive,cp8,ncp); /* append escaped char */
		    TTMCP8NXT(ttm);
		} else if(u8equal(cp8,ttm->meta.lbrc)) {
		    vsindexappendn(ttm->vs.passive,cp8,ncp);
		    TTMCP8NXT(ttm);
		    depth++;
		} else if(u8equal(cp8,ttm->meta.rbrc)) {
		    if(--depth > 0)
			vsindexappendn(ttm->vs.passive,cp8,ncp);
		    TTMCP8NXT(ttm);
		    if(depth == 0) break; /* we are done */
		} else {
		    vsindexappendn(ttm->vs.passive,cp8,ncp);
		    TTMCP8NXT(ttm);
		}
	    }/*<...> for*/
	} else {
	    /* keep moving */
	    vsindexappendn(ttm->vs.passive,cp8,ncp);
	    TTMCP8NXT(ttm);
	}
    } /* collect argument for */
done:
    return THROW(err);
}
/**************************************************/
/**
Execute a non-builtin function
@param ttm
@param frame args for function call
@param body for user-defined functions.
*/
static TTMERR
call(TTM* ttm, Frame* frame, char* body, VString* result)
{
    TTMERR err = TTM_NOERR;
    char crval[CREATELEN+1];
    char* b8;

    /* Compute the body using result  */
    crval[0] = '\0'; /* also use as a flag to indicate create value was created */
    for(b8=body;!isnul(b8);b8+=u8size(b8)) {
	if(issegmark(b8)) {
	    size_t segindex = segmarkindex(b8);
	    if(iscreateindex(segindex)) {
		if(crval[0] == '\0') { /* create the create value once only */
		    ttm->flags.crcounter++;
		    snprintf(crval,sizeof(crval),CREATEFORMAT,ttm->flags.crcounter);
		}
		vsappendn(result,crval,CREATELEN);
	    } else if(segindex < frame->argc) {
		char* arg = frame->argv[segindex];
		vsappendn(result,arg,strlen(arg));
	    } /* else treat as null string */
	} else
	    vsappendn(result,b8,u8size(b8));
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
printstring(TTM* ttm, const char* s8arg, TTMFILE* output)
{
    TTMERR err = TTM_NOERR;
    int slen = 0;
    char* s8 = NULL;
    char* p = NULL;

    if(s8arg == NULL) goto done;
    if((slen = strlen(s8arg))==0) goto done;
    s8 = unescape(s8arg);
	
    for(p=s8;*p;p += u8size(p)) {
	if(issegmark(p)) {
	    char info[16];
	    size_t segindex = (size_t)segmarkindex(p);
	    if(iscreateindex(segindex))
		snprintf(info,sizeof(info),"^{CR}");
	    else
		snprintf(info,sizeof(info),"^{%x}",(unsigned)segindex);
	    fputs(info,output->file);
	} else {
#ifdef DEE
	    if(deescape(p,cpa) < 0) {err = THROW(TTM_EUTF8); goto done;}
	    /* print codepoint */
	    ttmputc8(ttm,cpa,output);
#else
	    /* print codepoint */
	    ttmputc8(ttm,p,output);
#endif
	}
    }
    fflush(output->file);
done:
    nullfree(s8);
    return THROW(err);
}

#ifdef GDB
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
#endif

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
static char*
cleanstring(const char* s8, char* ctrls, size_t* pfinallen)
{
    char* clean = NULL;
    const char* p = NULL;
    char* q = NULL;
    size_t len = 0;

    if(ctrls == NULL) ctrls = "\t\n\f";
    len = strlen(s8);
    clean = (char*)calloc(sizeof(char),((4*len)+1)); /* max possible */
    for(p=s8,q=clean;*p;) {
	len = u8size(p);
	switch (len) {
	case 0: /* illegal utf8 char */
	    nullfree(clean);
	    clean = NULL;
	    goto done;
	case 2: /* non-ascii utf8 char */
	    memcpycp(q,p); /* pass as is */
	    p += len; q += len;
	    break;
	case 3: case 4:	 /* either non-ascii utf8 char or segment/creation mark */
	    if(issegmark(p)) {
		char info[16];
		size_t segindex = (size_t)segmarkindex(p);
		if(iscreateindex(segindex))
		    snprintf(info,sizeof(info),"^{CR}");
		else
		    snprintf(info,sizeof(info),"^{%x}",(unsigned)segindex);
		memcpy(q,info,strlen(info));
		p += SEGMARKSIZE; q += strlen(info);
	    } else {
		memcpycp(q,p); /* pass as is */
		p += len; q += len;
	    }
	    break;
	case 1: /* ascii char */
	    char c = *p;
	    if(c < ' ' || c == '\177') { /* c is a control char */
		if(strchr(ctrls,c) == NULL) { /* de-controlize */
		    char escaped[3+1] = {'\0','\0','\0','\0'};
#if 1
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
#else
		    escaped[0] = c;
#endif
		    len = strlen(escaped);
		    memcpy(q,escaped,len);
		    p++; q += len;
		} else
		    *q++ = *p++; /* pass control as is (ascii) */
	    } else { /* ordinary char */
		len = memcpycp(q,p);
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

#ifdef DEE
/* Convert '\\' escaped characters using standard C conventions.
The converted result is returned. Note that this is independent
of the TTM '@' escape mechanism.
@param src utf8 string
@param pfinallen store the final length of result
@return copy of s8 with escaped characters modified || NULL if non codepoint encountered
*/
static char*
deescape(const char* s8, size_t* pfinallen)
{
    char* deesc = NULL;
    char* p = NULL;
    char* q = NULL;
    size_t len = 0;

    if(s8 == NULL) goto done;
    len = strlen(s8);
    deesc = (char*)malloc(sizeof(char)*(len+1)); /* max possible */
    for(p=(char*)s8,q=(char*)deesc;*p;) {
	len = u8size(p);
	if(len == 0) goto fail; /* illegal utf8 char */
	if(len > 1) {
	    /* non-ascii utf8 char */
	    memcpycp(q,p); /* pass as is */
	    p += len; q += len;
	} else {/* len == 1 => ascii */
	    if(*p == '\\') { /* this is a C escape character */
		len = u8size(++p); /* escaped char might be any utf8 char */
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
		    case 'x': case 'X': /* apparently hex */
			if(1!=scanf(p,"%02x",&c)) goto fail;
			p += 2; /* skip the hex digits */
		    default: break;
		    }
		    *q++ = (char)c; /* pass escaped ascii char */
		} else {/* pass the utf8 char */
		    len = memcpycp(q,p);
		    p+=len; q += len;
		}
	    } else { /* ordinary char */
		len = memcpycp(q,p);
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
#endif /*DEE*/

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
    if(str && str->fcn.builtin) FAIL(ttm,TTM_ENOPRIM);
    return str;
}

/**************************************************/

/* Reset the ttm state to execute the next full line of text. */
static void
ttmreset(TTM* ttm)
{
    vsclear(ttm->vs.active);
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
    memmove((void*)dst,(void*)src,len*sizeof(char));
#else
    src += len;
    dst += len;
    while(len--) {*(--dst) = *(--src);}
#endif
}

/* Convert a string containing '\\' escaped characters using standard C conventions.
The converted result is returned. Note that this is independent
of the TTM '@' escape character because it is only used by readline on external data.
@param src utf8 string
@return copy of s8 with escaped characters modified || NULL if non codepoint encountered
*/
static char*
unescape(const char* s8)
{
    TTMERR err = TTM_NOERR;
    char* deesc = NULL;
    const char* p = NULL;
    char* q = NULL;
    size_t len = 0;

    if(s8 == NULL) goto done;
    len = strlen(s8);
    deesc = (char*)malloc(sizeof(char)*((4*len)+1)); /* max possible */
    for(p=s8,q=deesc;*p;) {
	len = u8size(p);
	if(len <= 0) EXITX(TTM_EUTF8); /* illegal utf8 char */
	if(len > 1) {
	    /* non-ascii utf8 char */
	    memcpycp(q,p); /* pass as is */
	    p += len; q += len;
	} else {/* len == 1 => ascii */
	    if(*p == '\\') { /* this is a C escape character */
		len = u8size(++p); /* escaped char might be any utf8 char */
		if(len == 1) {/* escaped ascii char */
		    int c = *p;
		    switch (c) {
		    case 'r': c = '\r'; p++; break;
		    case 'n': c = '\n'; p++; break;
		    case 'b': c = '\b'; p++; break;
		    case 'f': c = '\f'; p++; break;
		    case 't': c = '\t'; p++; break;
		    case '1': case '0': /* apparently octal */
			if(1!=scanf(p,"%03o",&c)) EXITX(TTM_EDECIMAL);
			if(c > '\177') {err = TTM_EUTF8; goto done;}
			p += 3; /* skip the octal digits */
		    case 'x': case 'X': /* apparently hex */
			if(1!=scanf(p,"%02x",&c)) EXITX(TTM_EDECIMAL);
			p += 2; /* skip the hex digits */
		    default: p++; break;
		    }
		    *q++ = (char)c; /* pass escaped ascii char */
		} else {/* pass the utf8 char */
		    len = memcpycp(q,p);
		    p+=len; q += len;
		}
	    } else { /* ordinary char */
		len = memcpycp(q,p);
		p += len; q += len;
	    }
	}
    }
    *q = '\0';
done:
    if(err) {
	nullfree(deesc);
	deesc = NULL;
    }
    return deesc;
}

/* Remove unescaped occurrences of "//" comment.
The converted result is returned. Note that this is independent
of the TTM '@' escape mechanism because it is only used by readline.
@param src utf8 string
@return copy of line with comments elided || NULL if non codepoint encountered
*/
static char*
uncomment(const char* line)
{
    TTMERR err = TTM_NOERR;
    char* decmt = NULL;
    const char* p = NULL;
    char* q = NULL;
    size_t len = 0;
    int ncp;

    if(line == NULL) goto done;
    len = strlen(line);
    decmt = (char*)malloc(sizeof(char)*((4*len)+1)); /* max possible */
    for(p=line,q=decmt;*p;) {
	ncp = u8size(p);
	if(ncp <= 0) EXITX(TTM_EUTF8); /* illegal utf8 char */
	if(ncp == 1 && *p == SLASH && p[1] == SLASH) {/* look for comment */
	    size_t rem = strlen(p); /* length of the comment */
	    const char* pe = (p) + rem; /* point to trailing nul char */
	    pe = u8backup(pe,line);
	    ncp = u8size(pe);
	    if(ncp != 1 || *pe != '\n') pe += ncp; /* no trailing newline */
	    /* wipe not pass the comment */
	    p = pe;
	}
	if(!isnul(p)) {
	    /* pass codepoint */
	    memcpycp(q,p); /* pass as is */
	    p += ncp; q += ncp;
	}
    }
    *q = '\0';
done:
    if(err) {
	nullfree(decmt);
	decmt = NULL;
    }
    return decmt;
}

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

/**
Peek at the nth char past the index.
Return a pointer to that char.
Leave index unchanged.
If EOS is encountered, then no advancement occurs
@param vs
@param n number of codepoint to peek ahead
@return cpa ptr to n'th codepoint
*/
static const char*
peek(VString* vs, size_t n)
{
    size_t saveindex = vsindex(vs);
    size_t i;
    char* p = NULL;

    p = vsindexp(vs);
    for(i=0;i<n;i++) {
	if(isnul(p)) break;
	vsindexskip(vs,(size_t)u8size(p));
	p = vsindexp(vs);
    }
    vsindexset(vs,saveindex);
    return p;
}

/**
Convert a residual count to a codepoint count.
@param ttm
@param u8 utf8 string
@param rp residual count associated with u8 in units of utf8
@return residual count in units of codepoints
*/
static size_t
rptocp(TTM* ttm, const char* u8, size_t rp)
{
    const char* p;
    size_t count = 0;
    int ncp;

    if(u8 != NULL) {
	const char* u8end = u8 + rp;
	for(p=u8;*p;) {
	    ncp = u8size(p);
	    assert(ncp > 0);
	    if(p < u8end) {p += ncp; count++; continue;}
	    if(p == u8end) {break;}
	    if(p > u8end) {break;} /* technically indicates a malformed utf8 codepoint */
	}
    }
    return count;
}

/**
Convert a codepoint count to a residual count (i.e. bytes).
@param ttm
@param u8 utf8 string
@parem residual count in units of codepoints
@return residual count associated with u8 in units of bytes
*/
static size_t
cptorp(TTM* ttm, const char* u8, size_t residual)
{
    const char* p;
    size_t count = 0;
    if(u8 != NULL) {
	size_t i;
	const char* u8end = u8 + strlen(u8);
	for(i=0,p=u8;i<residual;i++) {
	    int ncp = u8size(p);
	    assert(ncp > 0);
	    if(p < u8end) {p += ncp; continue;}
	    if(p >= u8end || *p == NUL8) break;
	}
	count = (p - u8);
	assert(p <= u8end);
    }
    return count;
}

/* Convert a variety of values to 1|0 representing true false */
static int
tfcvt(const char* value)
{
    unsigned tf = 0;
    if(value == NULL || strlen(value)==0) {
	tf = 1;
	goto done;
    }
    if(1==sscanf(value,"%u",&tf)) {
	if(tf != 0) tf = 1;
	goto done;
    }
    if(strcasecmp(value,"true")==0
	|| strcasecmp(value,"t")==0) {
	tf = 1;
	goto done;
    }
    if(strcasecmp(value,"false")==0
	|| strcasecmp(value,"f")==0) {
	tf = 0;
	goto done; 	
    }
    tf = 0;
done:
    return (int)tf;
}

/**************************************************/
/* Main() Support functions */

static void
initTTM()
{
    argoptions = vlnew();
    propoptions = vlnew();
    /* Set the locale to support UTF8 */
    if(setlocale(LC_ALL, "en_US.UTF-8") == NULL) usage("setlocale failed");
}

static void
reclaimglobals()
{
    vlfreeall(argoptions);
    vlfreeall(propoptions);
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
"		     '[0..9]*' -- set debug level\n"
"[-f inputfile]	  -- defaults to stdin\n"
"[-o file]	  -- defaults to stdout\n"
"[-p programfile] -- main program to execute\n"
"[-q]		  -- operate in quiet mode\n"
"[-B]		  -- bare executionl; suppress startup commands\n"
"[-P tag=value]	  -- set interpreter properties\n"
"[-V]		  -- print version\n"
"[--]		  -- stop processing command line options\n"
"[arg...]	  -- arbitrary string arguments; accessible by argv/argc TTM function"
);
    fprintf(stderr,"Options may be repeated\n");
    if(msg != NULL) exit(1); else exit(0);
}

/**
Read one full line of iput from specified file.
Note for each line read if it ends in an escapechar+newline
then another line is read and appended to current line.
This repeats until a line is read that does not end in escapechar+newline.
Note that encountering a bare EOF/EOS also terminates the line.
Note that comments are allowed using "//" style comments although
     this may leave cruft. Escaping of '/' is allowed to pass '/' characters 
@param ttm
@param f file from which to read
@param linep hold line(s) read from stdin
@return TTM_NOERR if data; TTM_EXXX if error
*/
static TTMERR
readline(TTM* ttm, TTMFILE* f, char** linep)
{
    TTMERR err = TTM_NOERR;
    int np8;
    utf8cpa p8;
    char* result = NULL;
    char* decom = NULL;
    VString* line = vsnew();
    
    vsclear(line);
    for(;;) { /* Read thru next \n or \0 (EOF) */
	if((np8=ttmnonl(ttm,f,p8)) <= 0) {err = THROW(TTM_EUTF8); goto done;}
	if(isnul(p8)) {err = TTM_EEOF; goto done;}
	if(*p8 == '\\') { /* Don't use ttm->meta.escapec */
	    /* peek to see if this escape at end of line: '\''\n' */
	    if((np8=ttmnonl(ttm,f,p8)) <= 0) {err = THROW(TTM_EUTF8); goto done;}
	    if(u8equal(p8,"\n")) {
		/* escape of \n => elide escape and \n and continue reading */
	    } else { /* pass the escape and the escaped char */
		vsappendn(line,"\\",1);
		vsappendn(line,p8,np8);
	    }
	    break;
	} else if(u8equal(p8,"\n")) {
	    vsappendn(line,p8,np8);
	    break;
	} else { /* char other than nul or escape */
	    vsappendn(line,p8,np8);
	}
    }
    /* Check for comments */
    decom = uncomment(vscontents(line));
    /* Convert any escapes to produce final result */
    result = unescape(decom);
    if(linep) {*linep = result; result = NULL;}
done:
    nullfree(decom);
    nullfree(result);
    vsfree(line);
    return err;
}


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
    char* oneline = NULL;
    int quit = 0;
    f = ttmopen(ttm,fname,"rb");
    if(f == NULL) {err = errno; goto done;}
    while(!quit) {
	switch (err=readline(ttm,f,&oneline)) {
	case TTM_NOERR: vsappendn(buf,oneline,0); break;
	case TTM_EEOF: quit = 1; break; /* no more input */
	default: goto done;
	}
	nullfree(oneline); oneline = NULL;
    }
    ttmclose(ttm,f);
done:
    nullfree(oneline);
    return err;
}

static void
setproperty(TTM* ttm, const char* key, const char* value)
{
    /* Set property  */
    propertyInsert(ttm,key,value);
    syncproperty(ttm,key,value);
}

static void
syncproperty(TTM* ttm, const char* key, const char* value)
{
    size_t n;
    switch (propenumdetect(key)) {
    case PE_STACKSIZE:
	sscanf(value,"%zu",&n);
	ttm->properties.stacksize = n;
	break;
    case PE_EXECCOUNT:
	sscanf(value,"%zu",&n);
	ttm->properties.execcount = n;
	break;
   case PE_SHOWFINAL:
	ttm->properties.showfinal = (tfcvt(value)?1:0);
	break;
   case PE_SHOWCALL:
	ttm->properties.showcall = (tfcvt(value)?1:0);
	break;
    default: break; /* user defined property */
    }
}

static const char*
propdfalt2str(enum PropEnum dfalt, size_t n)
{
    static char s[256];
    UNUSED(dfalt);
    snprintf(s,sizeof(s),"%zu",n);
    return s;
}

static size_t
propdfalt(enum PropEnum key)
{
    switch (key) {
    case PE_STACKSIZE: return DFALTSTACKSIZE;
    case PE_EXECCOUNT: return DFALTEXECCOUNT;
    case PE_SHOWFINAL: return DFALTSHOWFINAL;
    case PE_SHOWCALL:  return DFALTSHOWCALL;
    default: break;
    }
    return 0;
}

/* Force all pre-defined properties to default settings */
static void
defaultproperties(TTM* ttm)
{
    const char* s = NULL;
    s = propdfalt2str(PE_STACKSIZE,DFALTSTACKSIZE);
    setproperty(ttm,"stacksize",s);
    s = propdfalt2str(PE_EXECCOUNT,DFALTEXECCOUNT);
    setproperty(ttm,"execcount",s);
    s = propdfalt2str(PE_SHOWFINAL,DFALTSHOWFINAL);
    setproperty(ttm,"showfinal",s);
    s = propdfalt2str(PE_SHOWCALL,DFALTSHOWCALL);
    setproperty(ttm,"showcall",s);
}

/* Insert any command line -P option */
static void
cmdlineproperties(TTM* ttm)
{
    const char* key = NULL;
    const char* value = NULL;
    size_t i;

    for(i=0;i<vslength(propoptions);i+=2) {
	key = (const char*)vlget(propoptions,i);
	value = (const char*)vlget(propoptions,i+1);
	setproperty(ttm,key,value);
    }
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
	default:
	    int level = -1;
	    sscanf(p,"%d",&level);
	    if(level >= 0) {
		ttm->debug.debug = level;
	    } else {
		fprintf(stderr,"Unknown debug flag: '%c'\n",*p);
		exit(1);
	    }; break;
	}
    }
}


/**************************************************/
/* Helper functions for main */


/**
Execute a string for side effects and throw away any result.
*/
static TTMERR
execcmd(TTM* ttm, const char* cmd)
{
    TTMERR err = TTM_NOERR;
    int savetrace = ttm->debug.trace;
    size_t cmdlen = strlen(cmd);

    ttmreset(ttm);
    vsinsertn(ttm->vs.active,0,cmd,cmdlen);
    vsindexset(ttm->vs.active,0);
    if((err = scan(ttm))) goto done;
    ttmreset(ttm);
    ttm->debug.trace = savetrace;
done:
    return THROW(err);
}

static TTMERR
startup(TTM* ttm)
{
    TTMERR err = TTM_NOERR;
    char* cmd = NULL;

    if(!ttm->opts.bare) {
	char** cmdp;
#ifndef GDB
	TRACE trprev = ttm->debug.trace;
#endif
#ifdef GDB
	fprintf(stderr,"Begin startup commands\n");
#else
	ttm->debug.trace = TR_OFF;
#endif
	ttm->flags.starting = 1;
	for(cmdp=startup_commands;*cmdp != NULL;cmdp++) {
	    cmd = strdup(*cmdp);
	    if((err = execcmd(ttm,cmd))) EXIT(err);
	    nullfree(cmd); cmd = NULL;
	}
#ifdef GDB
	fprintf(stderr,"End startup commands\n");
	fflush(stderr);
#else
	ttm->debug.trace = trprev;
#endif
    }
done:
    ttm->flags.starting = 0;
    nullfree(cmd);
    return THROW(err);
}

static TTMERR
eval(TTM* ttm)
{
    TTMERR err = TTM_NOERR;
    char* cmd = NULL;

    /* Now execute the programfile, if any, and print collected passive output */
    if(ttm->opts.programfilename != NULL) {
	readfile(ttm,ttm->opts.programfilename,ttm->vs.tmp); /* read whole execute file */
	/* Remove '\\' escaped */
#ifdef DEE
	cmd = (char*)deescape(vscontents(ttm->vs.tmp),NULL);
#else
	cmd = vscontents(ttm->vs.tmp);
	cmd = nulldup(cmd); /* avoid compiler complaint */
#endif
	if(cmd == NULL) {err = THROW(TTM_EMEMORY); goto done;}
#if DEBUG > 0
	if(ttm->debug.trace) {
	    char* tmp = (char*)cleanstring(cmd,"\t",NULL);
	    xprintf(ttm,"scan: %s\n",tmp);
	    nullfree(tmp); tmp = NULL;
	}
#endif
	if((err=execcmd(ttm,cmd))) EXIT(err);
	if(ttm->flags.exit) goto done;
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
    struct OPTS opts;
    struct Properties option_props;
#ifndef TTMGLOBAL
    TTM* ttm = NULL;
#endif

    memset(&opts,0,sizeof(struct OPTS));

    if(argc == 1)
	usage(NULL);

    initTTM();

    /* Stash argv[0] */
    vlpush(argoptions,strdup(argv[0]));

    /* Option processing */
    while ((c = getopt(argc, argv, "d:f:io:p:qvP:VB-")) != EOF) {
	switch(c) {
	case 'd':
	    strcat(debugargs,optarg);
	    break;
	case 'f':
	    if(inputfilename == NULL)
		inputfilename = strdup(optarg);
	    break;
	case 'o':
	    if(outputfilename == NULL)
		outputfilename = strdup(optarg);
	    break;
	case 'p':
	    if(opts.programfilename == NULL)
		opts.programfilename = strdup(optarg);
	    break;
	case 'q': opts.quiet = 1; break;
	case 'P': /* Set properties*/
	    if(optarg == NULL) usage("Illegal -P key");
	    if(strlen(optarg) == 0) {
		usage("Illegal -P key");
	    } else {
		char* debopt = NULL;
		char *p;
		debopt = debash(optarg);
		p = strchr(debopt,'=');
		/* get pointer to value or NULL if missing */
		if(p != NULL) {*p = '\0'; p++;}
		vlpush(propoptions,strdup(debopt));
		vlpush(propoptions,nulldup(p));
		nullfree(debopt);
	    }
	    break;
	case 'v': opts.verbose = 1; break;
	case 'B': opts.bare = 1; break;
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

    if((err=setupio(ttm,inputfilename,outputfilename))) {
	fprintf(stderr,"IO setup failure: (%d) %s\n",(int)err,ttmerrmsg(err));
	exit(1);
    }

    ttm->opts = opts; memset(&opts,0,sizeof(struct OPTS));

    if((err = startup(ttm))) goto done;
    if((err = eval(ttm))) goto done;

done:
    exitcode = ttm->flags.exitcode;
    if(err) exitcode = 1;
    if(err) FAIL(ttm,err);
    /* cleanup */
    closeio(ttm);
    /* Clean up misc state */
    nullfree(outputfilename);
    nullfree(inputfilename);
    freeTTM(ttm);
    reclaimglobals();
    return (exitcode?1:0); // exit(exitcode);
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
@param p8 dst for the codepoint (not nul terminated)
@return size of the codepoint
*/
static int
ttmgetc8(TTM* ttm, TTMFILE* f, char* p8)
{
    int c;
    int i, cplen;

    if(f->npushed > 0) {
	memcpycp(p8,f->stack[--f->npushed]);
	cplen = u8size(p8);
    } else {
	c = fgetc(f->file); /* read first char of codepoint */
	if(ttmeof(ttm,f)) c = '\0';
	if(ttmerror(ttm,f)) FAIL(ttm,errno);
	p8[0] = c;
	cplen = u8sizec(c);
	for(i=1;i<cplen;i++) { /* read rest of the codepoint */
	    c=fgetc(f->file);
	    p8[i] = (char)c;
	    if(c == EOF) {p8[0] = NUL8; cplen = 1; break;}
	}
	if(ttmerror(ttm,f)) FAIL(ttm,TTM_EIO);
	cplen = u8size(p8);
	if(cplen == 0) FAIL(ttm,TTM_EUTF8);
	if(u8validcp(p8) < 0) FAIL(ttm,TTM_EUTF8);
    }
    return cplen;
}

/**
Push back a codepoint; the pushed codepoints form a stack.
@param ttm
@param f the file being read
@param p8 the codepoint to push
@return void
*/
static void
ttmpushbackc(TTM* ttm, TTMFILE* f, char* p8)
{
    if(f->npushed >= (MAXPUSHBACK)) FAIL(ttm,TTM_EIO); /* too many pushes */
    memcpycp(f->stack[f->npushed++],p8); /* push to stack */
}

/**
Convert occurrences of \r\n -> \n.
A wrapper arount ttmgetc8, so has same signature.
Uses pushback.
@param ttm
@param f FILE* from which to read.
@param p8 dst for the codepoint (not nul terminated)
@return size of the codepoint
*/
static int
ttmnonl(TTM* ttm, TTMFILE* f, char* p8)
{
    size_t np1,np2;
    utf8cpa char1;
    utf8cpa char2;
    np1 = ttmgetc8(ttm,f,char1);
    if(np1 == 1 && char1[0] == '\r') {
	/* check for following '\n' */
	np2 = ttmgetc8(ttm,f,char2);
	if(np2 != 1 || char2[0] != '\n') {
	   ttmpushbackc(ttm,f,char2); /* pushback second char if not \n and let \r pass	 thru */
	}
    }
    /* send the original char */
    memcpy(p8,char1,np1); /* send the \r or \n */
    return np1;
}

/*
Writes one codepoint
All writing of characters should go thru this procedure.
@param f FILE* to which to write.
@param p8 src for the codepoint (not nul terminated)
@param f file from which to read
@return size of the codepoint
*/
static int
ttmputc8(TTM* ttm, const char* p8, TTMFILE* f)
{
    int i, cplen;
    int c;

    cplen = u8size(p8);
    if(cplen == 0) FAIL(ttm,TTM_EUTF8);
    for(i=0;i<cplen;i++) {
	c = (UTF8P(p8)[i] & 0xFF);
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
