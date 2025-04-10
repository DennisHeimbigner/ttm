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
#undef GDB

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

#include "const.h"
#include "types.h"
#include "decls.h"
#include "forward.h"
#include "vsl.h"
#include "debug.h"

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
    ascii2u8('\n',ttm->meta.metac);
    ascii2u8('<',ttm->meta.openc);
    ascii2u8('>',ttm->meta.closec);
    memcpy(ttm->meta.lbrc,ttm->meta.openc,sizeof(utf8cpa));
    memcpy(ttm->meta.rbrc,ttm->meta.closec,sizeof(utf8cpa));
    ttm->vs.active = vsnew();
    ttm->vs.passive = vsnew();
    ttm->vs.tmp = vsnew();
    ttm->frames.top = -1;
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
    clearFramestack(ttm);
    vsfree(ttm->vs.active);
    vsfree(ttm->vs.passive);
    vsfree(ttm->vs.tmp);
    clearDictionary(ttm,&ttm->tables.dictionary);
    clearCharclasses(ttm,&ttm->tables.charclasses);
    closeio(ttm);
    nullfree(ttm->opts.programfilename);
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
    ttm->frames.top++;
    if(ttm->frames.top >= ttm->properties.stacksize)
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
clearArgv(utf8** argv, size_t argc)
{
    size_t i;
    for(i=0;i<argc;i++) {
	utf8* arg = argv[i];
	if(arg == NULL) break; /* trailing null */
	free(arg);
	argv[i] = NULL;
    }
}

/**************************************************/
static Function*
newFunction(TTM* ttm)
{
    Function* f = (Function*)calloc(1,sizeof(Function));
    if(f == NULL) FAIL(ttm,TTM_EMEMORY);
    f->fcn.nextsegindex = SEGINDEXFIRST;
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
    if(strcmp("eol",(const char*)s)==0) return ME_META;
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
		vsappendn(frameresult(ttm),(const char*)ttm->meta.escapec,u8size(ttm->meta.escapec));
	    } else {
		vsappendn(frameresult(ttm),(const char*)cp8,ncp);
	    }
	    cp8 += ncp;
	} else if(u8equal(cp8,ttm->meta.sharpc)) {/* possible fcn call */
	    TTMSETPTR(cp8);
	    if((err = processfcn(ttm))) goto done;
	    TTMGETPTR(cp8); ncp = u8size(cp8);
	} else if(u8equal(cp8,ttm->meta.openc)) {/* <...> nested brackets */
	    TTMSETPTR(cp8);
	    if((err=collectescaped(ttm,frameresult(ttm)))) goto done;
	    TTMGETPTR(cp8); ncp = u8size(cp8);
	    /* collected string will have been appended to end of frameresult */
	} else { /* All other characters */
	    vsappendn(frameresult(ttm),(const char*)cp8,ncp);
	    cp8 += ncp;
	}
    } while(!stop);

    /** When we get here, we are finished, so clean up. */
    vsindexset(ttm->vs.active,0); /* make sure ttm->vs.active->index is set */

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

/**
Compute a function and leave the result into:
1. ttm->vs.active if the function is active or
2. frameresult
@param ttm
@param frame
@param dst
@return TTMERR
*/
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

    /* Now execute this function, which will leave result in frameresult() */
    if(frame->argc == 0) {err = FAILX(ttm,TTM_EFEWPARMS,NULL); goto done;}
    if(strlen((const char*)frame->argv[0])==0) {err = FAILX(ttm,TTM_EEMPTY,NULL); goto done;}
    /* Locate the function to execute */
    if((fcn = dictionaryLookup(ttm,frame->argv[0]))==NULL) {
	fprintf(stderr,"Missing dictionary name: %s\n",frame->argv[0]);
        err = FAILNONAME(0);
	goto done;
    }
    if(fcn->fcn.minargs > (frame->argc - 1)) /* -1 to account for function name*/
	{err = THROW(TTM_EFEWPARMS); goto done;}
    /* Trace frame on entry */
    if(ttm->debug.trace || fcn->fcn.trace)
	trace(ttm,1,TRACING);

    if(fcn->fcn.builtin) {
	fcn->fcn.fcn(ttm,frame,frameresult(ttm));
	if(fcn->fcn.novalue) vsclear(frameresult(ttm));
	if(ttm->flags.exit) goto done;
    } else /* invoke the pseudo function "call" */
	call(ttm,frame,frameresult(ttm),fcn->fcn.body);

    /* Trace exit result */
    if(ttm->debug.trace || fcn->fcn.trace)
	trace(ttm,0,TRACING);

#if DEBUG > 1
xprintf(ttm,"context:\n\result=|%s|\n\tactive=|%s|\n",
	vscontents(frameresult(ttm)),vsindexp(ttm->vs.active));
#endif
done:
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

    /* Always need a frame */
    frame = pushFrame(ttm);

    if(fcncase == FCN_PASSIVE || fcncase == FCN_ACTIVE) {
	/* Parse and collect the args of the function call */
	if((err=collectargs(ttm,frame))) goto done;
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
	break;
    case FCN_ONE: /* Pass one char */
	cp8 = (utf8*)vsindexskip(ttm->vs.active,u8size(cp8));
	vsappendn(frameresult(ttm),(const char*)cp8,u8size(cp8));
	break;
    case FCN_TWO: /* pass two chars */
	cp8 = (utf8*)vsindexskip(ttm->vs.active,u8size(cp8));
	vsappendn(frameresult(ttm),(const char*)cp8,u8size(cp8));
	cp8 = (utf8*)vsindexskip(ttm->vs.active,u8size(cp8));
	vsappendn(frameresult(ttm),(const char*)cp8,u8size(cp8));
	break;
    case FCN_THREE: /* pass three chars */
	cp8 = (utf8*)vsindexskip(ttm->vs.active,u8size(cp8));
	vsappendn(frameresult(ttm),(const char*)cp8,u8size(cp8));
	cp8 = (utf8*)vsindexskip(ttm->vs.active,u8size(cp8));
	vsappendn(frameresult(ttm),(const char*)cp8,u8size(cp8));
	cp8 = (utf8*)vsindexskip(ttm->vs.active,u8size(cp8));
	vsappendn(frameresult(ttm),(const char*)cp8,u8size(cp8));
	break;
    case FCN_UNDEF: abort(); /* Should never happen */
    }

    /* Now, put the result into the appropriate buffer */
    if(vslength(frameresult(ttm)) > 0) {
	if(frame->active) {
	    /* insert result at current index so it will be actively scanned */
	    /* WARNING: invalidates any pointers into active */
	    (void)vsindexinsertn(ttm->vs.active,vscontents(frameresult(ttm)),vslength(frameresult(ttm)));
	} else {/*!frame->active*/
	    /* add frameresult to parent frameresult */
	    vsappendn(frameparent(ttm),vscontents(frameresult(ttm)),0);
	}
    }

    /* Reclaim the frame */
    popFrame(ttm);
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
collectargs(TTM* ttm, Frame* frame)
{
    TTMERR err = TTM_NOERR;
    int stop;
    utf8* cp8;
    int ncp;
    VString* arg = NULL;

    TTMGETPTR(cp8); ncp = u8size(cp8);

    arg = frame->result; /* Does dual duty: as arg collector and as execution result collector */

    /* Scan() will have left the index for active buffer at the
       start of function name */

    stop = 0;
statetrace();
#if DEBUG > 1
fprintf(stderr,"\targ=|%s|\n",printsubseq(frameresult(ttm),argstart,vslength(frameresult(ttm))));
#endif /* DEBUG > 1 */

    do { /* Collect 1 arg */
	/* Look for special chars */
	if(isnul(cp8)) { /* EOS => stop */
	    stop = 1;
	} else if(isascii(cp8) && strchr(NONPRINTIGNORE,*cp8)) {
	    /* non-printable ignored chars must be ASCII */
	    cp8 += ncp; ncp = u8size(cp8);
	} else if(isescape(cp8)) {
	    cp8 += ncp; ncp = u8size(cp8);
	    if(isnul(cp8)) { /* escape as very last character in buffer */
		vsappendn(arg,(const char*)ttm->meta.escapec,u8size(ttm->meta.escapec));
	    } else {
		vsappendn(arg,(const char*)cp8,ncp);
	    }
	} else if(u8equal(cp8,ttm->meta.sharpc)) {
	    TTMSETPTR(cp8);
	    if((err = processfcn(ttm))) goto done;
	    TTMGETPTR(cp8); ncp = u8size(cp8);
	} else if(u8equal(cp8,ttm->meta.lbrc)) {
	    TTMSETPTR(cp8);
	    if((err=collectescaped(ttm,arg))) goto done;
	    TTMGETPTR(cp8);
	    ncp = u8size(cp8);
	} else if(u8equal(cp8,ttm->meta.semic) || u8equal(cp8,ttm->meta.rbrc)) {
	    if(frame->argc >= MAXARGS) {err = THROW(TTM_EMANYPARMS); goto done;}
#if DEBUG > 1
xprintf(ttm,"collectargs: argv[%d]=|%s|\n",frame->argc,vscontents(arg));
#endif /* DEBUG > 1 */
	    frame->argv[frame->argc++] = (utf8*)vsextract(arg);
	    if(u8equal(cp8,ttm->meta.rbrc)) stop = 1;
	    cp8 += ncp; ncp = u8size(cp8); /* move past the ';' or '>' */
	} else { /* ordinary char */
	    vsappendn(arg,(const char*)cp8,ncp);
	    cp8 += ncp; ncp = u8size(cp8);
	}
    } while(!stop);

    /* add null to end of the arg list as signal */
    frame->argv[frame->argc] = NULL;

    TTMSETPTR(cp8);
done:
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
	ncp = u8size(cp8);
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
call(TTM* ttm, Frame* frame, VString* result, utf8* body)
{
    TTMERR err = TTM_NOERR;
    utf8* p;
    char crval[CREATELEN+1];
    int crlen = -1; /* signals not created */
    int ncp = 0;

    /* Compute the body using result  */
    vsclear(result);
    for(p=body;;p+=ncp) {
	ncp = u8size(p);
	if(isnul(p))
	    break;
	else if(issegmark(p)) {
	    size_t segindex = (size_t)segmarkindex(p);
	    if(segindex == CREATEINDEXONLY) {
		/* Construct the cr value unless already created */
		if(crlen < 0) {
		    snprintf(crval,sizeof(crval),CREATEFORMAT,++ttm->flags.crcounter);
		    crlen = strlen(crval);
		}
		vsappendn(result,crval,crlen);
	    } else if(segindex < frame->argc) {
		utf8* arg = frame->argv[segindex]; /* lowest segindex is 1; also skips argv[0] */
		vsappendn(result,(const char*)arg,strlen((const char*)arg));
	    } /* else treat as null string */
	} else { /* pass the codepoint */
	    int ncp = u8size(p);
	    vsappendn(result,(char*)p,ncp);
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
		snprintf(info,sizeof(info),"^CR");
	    else
		snprintf(info,sizeof(info),"^%02x",(unsigned)segindex);
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
static utf8*
printclean(const utf8* s8, char* ctrls, size_t* pfinallen)
{
    utf8* clean = NULL;
    char* p = NULL;
    char* q = NULL;
    size_t len = 0;

    if(ctrls == NULL) ctrls = "\t\n\f";
    len = strlen((const char*)s8);
    clean = (utf8*)calloc(sizeof(utf8),((4*len)+1)); /* max possible */
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

#ifdef DEE
/* Convert '\\' escaped characters using standard C conventions.
The converted result is returned. Note that this is independent
of the TTM '@' escape mechanism.
@param src utf8 string
@param pfinallen store the final length of result
@return copy of s8 with escaped characters modified || NULL if non codepoint encountered
*/
static utf8*
deescape(const utf8* s8,size_t* pfinallen)
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
		    case 'x': case 'X': /* apparently hex */
			if(1!=scanf(p,"%02x",&c)) goto fail;
			p += 2; /* skip the hex digits */
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
    if(str->fcn.builtin) FAIL(ttm,TTM_ENOPRIM);
    return str;
}

/**************************************************/

#include "io.h"

/**************************************************/
/* Builtin functions */

/* Original TTM functions */

#include "builtins.h"

/**************************************************/

/* Reset the ttm state to execute the next full line of text. */
static void
ttmreset(TTM* ttm)
{
    vsclear(ttm->vs.active);
    vsclear(ttm->vs.passive);
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
    memmove((void*)dst,(void*)src,len*sizeof(utf8));
#else
    src += len;
    dst += len;
    while(len--) {*(--dst) = *(--src);}
#endif
}

#if 0
/* Insert a string into a frame at after a given position */
static TTMERR
insertframearg(TTM* ttm, Frame* frame, int pos, utf8* newarg)
{
    TTMERR err = TTM_NOERR;
    VList* newargv = NULL;
    int i;
    void** contents = NULL;

    if(pos < 0) return THROW(TTM_EINVAL);
    if(pos > (int)frame->argc) pos = frame->argc;
    for(i=0;i<pos;i++) vlpush(newargv,frame->argv[i]);
    vlpush(newargv,newarg);
    for(;i<(int)frame->argc;i++) vlpush(newargv,frame->argv[i]);
    vlpush(newargv,NULL);
    vlsetlength(newargv,vllength(newargv)-1); /* do not include trailing null */
    if(vllength(newargv) > MAXARGS) {err = TTM_EPARMROLL; goto done;}
    clearFrame(ttm,frame);
    frame->argc = vllength(newargv);
    contents = vlcontents(newargv);
    memcpy(frame->argv,contents,sizeof(utf8*)*frame->argc);
done:
    vlfree(newargv);
    return THROW(err);
}
#endif

/* Remove a string from a frame at after a given position */
static TTMERR
removeframearg(TTM* ttm, Frame* frame, int pos)
{
    TTMERR err = TTM_NOERR;
    VList* newargv = NULL;
    int i;
    void** contents = NULL;

    if(frame->argc < 3) return THROW(TTM_EFEWPARMS);
    if(pos < 0) return THROW(TTM_EINVAL);
    if(pos > (int)frame->argc) pos = frame->argc - 1;

    /* short circuit */
    if(pos == (int)(frame->argc - 1)) {
	utf8* argargc = frame->argv[frame->argc-1];
	nullfree(argargc);
	frame->argc--;
	goto done;
    }
    
    /* Copy over the frame argv below and including pos */
    for(i=0;i<pos;i++) vlpush(newargv,frame->argv[i]);
    /* copy over the part bove pos+1 */
    for(i++;i<(int)frame->argc;i++) vlpush(newargv,frame->argv[i]);
    vlpush(newargv,NULL);
    vlsetlength(newargv,vllength(newargv)-2); /* do not include trailing null or the removed element */
    if(vllength(newargv) > MAXARGS) {err = TTM_EPARMROLL; goto done;}
    clearFrame(ttm,frame);
    frame->argc = vllength(newargv);
    contents = vlcontents(newargv);
    memcpy(frame->argv,contents,sizeof(utf8*)*frame->argc);
done:
    vlfree(newargv);
    return THROW(err);
}

/* Convert a string containing '\\' escaped characters using standard C conventions.
The converted result is returned. Note that this is independent
of the TTM '@' escape mechanism because it is only used by readline.
@param src utf8 string
@return copy of s8 with escaped characters modified || NULL if non codepoint encountered
*/
static utf8*
unescape(const utf8* s8)
{
    utf8* deesc = NULL;
    char* p = NULL;
    char* q = NULL;
    size_t len = 0;

    if(s8 == NULL) goto done;
    len = strlen((const char*)s8);
    deesc = (utf8*)malloc(sizeof(utf8)*((4*len)+1)); /* max possible */
    for(p=(char*)s8,q=(char*)deesc;*p;) {
	len = u8size((const utf8*)p);
	if(len <= 0) goto fail; /* illegal utf8 char */
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
		    case 'x': case 'X': /* apparently hex */
			if(1!=scanf(p,"%02x",&c)) goto fail;
			p += 2; /* skip the hex digits */
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
done:
    return deesc;
fail:
    nullfree(deesc);
    deesc = NULL;
    goto done;
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

/**************************************************/
/* Main() Support functions */

static void
initglobals()
{
    argoptions = vlnew();
}

static void
reclaimglobals()
{
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
"[-f inputfile]	  -- defaults to stdin\n"
"[-o file]	  -- defaults to stdout\n"
"[-p programfile] -- main program to execute\n"
"[-q]		  -- operate in quiet mode\n"
"[-B]		  -- bare executionl; suppress startup commands\n"
"[-V]		  -- print version\n"
"[-P tag=value]	  -- set interpreter properties\n"
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
@param ttm
@param f file from which to read
@param linep- hold line(s) read from stdin
@return TTM_NOERR if data; TTM_EXXX if error
*/
static TTMERR
readline(TTM* ttm, TTMFILE* f, utf8** linep)
{
    TTMERR err = TTM_NOERR;
    int ncp;
    utf8cpa cp8;
    utf8* result = NULL;
    VString* line = vsnew();
    
    vsclear(line);
    for(;;) { /* Read thru next \n or \0 (EOF) */
	if((ncp=ttmnonl(ttm,f,cp8)) <= 0) {err = THROW(TTM_EUTF8); goto done;}
	if(isnul(cp8)) {err = THROW(TTM_EEOF); goto done;}
	if(*cp8 == '\\') { /* Don't use ttm->meta.escapec */
	    /* peek to see if this escape at end of line: '\''\n' */
	    if((ncp=ttmnonl(ttm,f,cp8)) <= 0) {err = THROW(TTM_EUTF8); goto done;}
	    if(u8equal(cp8,(utf8*)"\n")) {
		/* escape of \n => elide escape and \n and continue reading */
	    } else { /* pass the escape and the escaped char */
		vsappendn(line,"\\",1);
		vsappendn(line,(const char*)cp8,ncp);
	    }
	    break;
	} if(u8equal(cp8,(const utf8*)"\n")) {
	    vsappendn(line,(const char*)cp8,ncp);
	    break;
	} else { /* char other than nul or escape */
	    vsappendn(line,(const char*)cp8,ncp);
	}
    }
    /* Convert any escapes to produce final result */
#if 0
    result = vsextract(line);
#else
    result = unescape((utf8*)vscontents(line));
#endif
    if(linep) {*linep = result; result = NULL;}
done:
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
    utf8* oneline = NULL;
    int quit = 0;
    f = ttmopen(ttm,(const char*)fname,"rb");
    if(f == NULL) {err = errno; goto done;}
    while(!quit) {
	nullfree((char*)oneline); oneline = NULL;
	switch (err=readline(ttm,f,&oneline)) {
	case TTM_NOERR: vsappendn(buf,(const char*)oneline,0); break;
	case TTM_EEOF: quit = 1; break; /* no more input */
	default: goto done;
	}
    }
    ttmclose(ttm,f);
    nullfree(oneline);
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


/**
Execute a string for side effects and throw away any result.
*/
static TTMERR
execcmd(TTM* ttm, const char* cmd)
{
    TTMERR err = TTM_NOERR;
    int savetrace = ttm->debug.trace;
    size_t cmdlen = strlen(cmd);

#ifdef GDB
    ttm->debug.trace = 1;
#else
    ttm->debug.trace = 0;
#endif
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

    /* Now execute the programfile, if any, and print collected passive output */
    if(ttm->opts.programfilename != NULL) {
	readfile(ttm,ttm->opts.programfilename,ttm->vs.tmp); /* read whole execute file */
	/* Remove '\\' escaped */
#ifdef DEE
	cmd = (char*)deescape((const utf8*)vscontents(ttm->vs.tmp),NULL);
#else
	cmd = vscontents(ttm->vs.tmp);
	cmd = nulldup(cmd); /* avoid compiler complaint */
#endif
	if(cmd == NULL) {err = THROW(TTM_EUTF8); goto done;}
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

    /* Option processing */
    while ((c = getopt(argc, argv, "d:e:f:io:p:qVB-")) != EOF) {
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
	fprintf(stderr,"IO setup failure: (%d) %s\n",(int)err,errstring(err));
	exit(1);
    }

    ttm->opts = opts; memset(&opts,0,sizeof(struct OPTS));

    if((err = execall(ttm))) goto fail;

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

    UNUSED(tic);
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
