#ifdef CATCH

/**************************************************/
static void
ttmbreak(TTMERR err)
{
    return;
}

static TTMERR
ttmthrow(TTM* ttm, TTMERR err, const char* file, const char* fcn, int line)
{
    UNUSED(ttm); UNUSED(file); UNUSED(fcn),UNUSED(line);
    if(err != TTM_NOERR) {
        if(ttm != NULL && ttm->debug.debug > 1) {
	    fprintf(stderr,"THROW: (%d) %s; %s.%s.%d\n",err,ttmerrmsg(err),file,fcn,line);
	    if(vslength(ttm->vs.active) > 0) {
	        fprintf(stderr,"\tactive=%s\n",vscontents(ttm->vs.active));
    	        fprintf(stderr,"\tactive[%zu]=%s\n",vsindex(ttm->vs.active),vsindexp(ttm->vs.active));
	    }
	    if(vslength(ttm->vs.passive) > 0) {
	        fprintf(stderr,"\tpassive=%s\n",vscontents(ttm->vs.passive));
    	        fprintf(stderr,"\tpassive[%zu]=%s\n",vsindex(ttm->vs.passive),vsindexp(ttm->vs.passive));
	    }
	}
	ttmbreak(err);
    }
    return err;
}
#endif

/* Wrap vxprintf same way fprintf wraps vprintf */
static void
xprintf(TTM* ttm, const char* fmt,...)
{
    va_list ap;
    va_start(ap,fmt);
    vxprintf(ttm,fmt,ap);
    va_end(ap);
}

/**
Similar to vprintf, but:
1. calls cleanstring on the outgoing text.
2. leaves the trailing '\n'
3. remembers that output did/did-not end with a newline.
@param ttm
@param fmt
@param ap
@return void
*/
static void
vxprintf(TTM* ttm, const char* fmt, va_list ap)
{
    char* xbuf = NULL;
    size_t xsize = 0;
    int hasnl = 0;
    size_t xlen = 0;
    size_t xfinal = 0;
    char* p = NULL;
    FILE* xfile = NULL;

    xfile = ttm->io._stderr->file;
    hasnl = ttm->debug.xpr.outnl;
    
    xbuf = ttm->debug.xpr.xbuf;
    xsize = sizeof(ttm->debug.xpr.xbuf);
    xlen = strlen(xbuf);
    assert(xsize >= (4*xlen)+1);

    p = &xbuf[xlen];
    (void)vsnprintf(p, xsize - xlen, fmt, ap);

    xfinal = strlen(xbuf);
    if(xfinal > 0) {
	char* tmp = NULL;
	hasnl = (xbuf[xfinal - 1] == '\n'); /* remember this */
	if(hasnl) xbuf[xfinal - 1] = '\0'; /* temporarily elide the final '\n' */
	tmp = cleanstring(xbuf,NULL,NULL);
	strncpy(xbuf,tmp,xsize);
	nullfree(tmp);
	xfinal = strlen(xbuf);
	if(hasnl) {
	    /* Restore missing \n */
	    xbuf[xfinal++] = '\n'; /* restore trailing newline */
	}
	xbuf[xfinal] = '\0'; /* ensure nul term */
	fprintf(xfile,"%s",xbuf);
        xbuf[0] = '\0'; /* reset */
    }
    fflush(stdout); fflush(stderr);
    ttm->debug.xpr.outnl = hasnl;
}

#if DEBUG > 0
void
statetracex(TTM* ttm, char* cp8)
{
#if DEBUG > 1
    fprintf(stderr,"pos=%ld cp8='%c'\n\tactive={%s}\n\tpassive=%s\n",
	ttm->vs.active->index,
	(char)cp8[0],
	printwithpos(ttm->vs.active),
	print2len(ttm->vs.passive));
#endif /*DEBUG==0*/
}
#endif /*DEBUG*/

#if DEBUG > 0
#  if DEBUG > 1
#    if 1
#define statetrace() fprintf(stderr,"pos=%ld cp8='%c'\n\tactive={%s}\n\tpassive=%s\n",ttm->vs.active->index,(char)cp8[0],printwithpos(ttm->vs.active),print2len(ttm->vs.passive))
#    else
#define statetrace() statetracex(ttm,cp8)
#    endif
#  else /*DEBUG <= 1 */
#define statetrace()
#  endif /* DEBUG <= 1*/
#else /*DEBUG == 0*/
#define statetrace()
#endif /*DEBUG==0*/

#ifdef GDB

static void
dumpentry(TTM* ttm, enum TableType tt, struct HashEntry* entry, int printvalues)
{
    switch (tt) {
    case TT_DICT: {
	Function* str = (Function*)entry;
	xprintf(ttm,"	|%s%s|",str->entry.name,(str->fcn.builtin?"*":""));
	if(printvalues) {
	    if(str->fcn.builtin)
		xprintf(ttm," = builtin");
	    else
		xprintf(ttm," = |%s|",vscontents(str->fcn.body));
	}
	} break;
    case TT_CLASSES: {
	Charclass* cl = (Charclass*)entry;
	xprintf(ttm,"	|%s|",cl->entry.name);
	if(printvalues)
	    xprintf(ttm," = [%s%s]",(cl->negative?"^":""),cl->characters);
	} break;
    case TT_PROPS: {
	Property* p = (Property*)entry;
	xprintf(ttm,"	|%s%",p->entry.name);
	if(printvalues)
	    xprintf(ttm," = |%s|",p->value);
	} break;
    }
}

/**
Dump keys of the ttm->tables.dictionary
@param ttm
@param dict to dump
@param printvalues 1=>print both key and value; 0=>print key only
@return void
*/
static void
dumpdict0(TTM* ttm, struct HashTable* dict, int printvalues, enum TableType tt)
{
    int i;
    for(i=0;i<HASHSIZE;i++) {
	int first;
	struct HashEntry* entry = dict->table[i].next;
	for(first=1;entry != NULL;first=0) {
	    if(first) fprintf(stderr,"[%3d]",i);
	    dumpentry(ttm,tt,entry,printvalues);
	    xprintf(ttm,"\n");
	    entry = entry->next;
	}
	xprintf(ttm,"\n");
    }
}

static void
dumpnames(TTM* ttm)
{
    dumpdict0(ttm,&ttm->tables.dictionary,0,TT_DICT);
}

static void
dumpnamesplus(TTM* ttm)
{
    dumpdict0(ttm,&ttm->tables.dictionary,1,TT_DICT);
}

static void
dumpcharclasses(TTM* ttm)
{
    dumpdict0(ttm,&ttm->tables.charclasses,1,TT_CLASSES);
}

static void
dumpprops(TTM* ttm)
{
    dumpdict0(ttm,&ttm->tables.properties,1,TT_PROPS);
}

static const char*
printwithpos(VString* vs)
{
    static char content[1<<14];
    char* p = vs->content;
    char* q = content;
    size_t pos = vs->index;
    size_t avail = vs->length;

    assert(vs->length < sizeof(content));
    if(pos > 0)
	memcpy(q,p,pos);
    p += pos;
    q += pos;
    *q++ = '|';
    avail -= pos;
    if(avail > 0)
	memcpy(q,p,avail);
    q += avail;
    *q = NUL8;
    return content;
}    

#endif /*GDB*/

static void
dumpframe(TTM* ttm, Frame* frame)
{
    size_t i = 0;

    xprintf(ttm,"frame{active=%d argc=%zu",frame->active,frame->argc);
    for(i=0;i<frame->argc;i++) {
	xprintf(ttm," %zu:|%s|",i,frame->argv[i]);
    }
    xprintf(ttm,"}\n");
    fflush(stderr);
}

/**
Dump the frame stack
*/
static void
dumpstack(TTM* ttm)
{
    int i;
    for(i=0;i<=ttm->frames.top;i++) {
	xprintf(ttm,"[%d] ",i);
	dumpframe(ttm,&ttm->frames.stack[i]);
	xprintf(ttm,"\n");
    }
    fflush(stderr);
}

/* Traceframe helper */
static int
chintersects(const char* significant, const char* s)
{
    int isects = 0;
    const char* p = s;
    for(;*p;p++) {
	if(strchr(significant,*p) != NULL) {isects = 1; goto done;}
    }
done:
    return isects;
}

static void
traceframe(TTM* ttm, Frame* frame, int traceargs)
{
    char tag[1+(4*MAXCP8SIZE)]; /* 4 codepoints + NUL8 */
    char* p;
    int ncp;
    size_t i = 0;

    if(frame->argc == 0) {
	xprintf(ttm,"#<empty>");
	return;
    }
    p = tag;
    ncp = u8size(ttm->meta.sharpc);
    memcpy(p,ttm->meta.sharpc,ncp); p += ncp;
    if(!frame->active) {
        memcpy(p,ttm->meta.sharpc,ncp); p += ncp;
    }
    ncp = u8size(ttm->meta.openc);
    memcpy(p,ttm->meta.openc,ncp); p += ncp;
    *p = NUL8;
    xprintf(ttm,"%s",tag);
    xprintf(ttm,"%s",frame->argv[0]);
    if(traceargs) {
	for(i=1;i<frame->argc;i++) {
	    char* cleaned = cleanstring(frame->argv[i],"\t",NULL);
	    int significant = chintersects(METACHARS,cleaned);
	    xprintf(ttm,"%s%s%s%s",
		ttm->meta.semic,
		significant?"<":"",
		cleaned,
		significant?">":"");
	    nullfree(cleaned);
	}
    }
    xprintf(ttm,"%s",ttm->meta.closec);
    fflush(stderr);
}

static void
trace1(TTM* ttm, TTMERR err, int depth, int entering, int tracing)
{
    Frame* frame;

    if(!ttm->debug.xpr.outnl) xprintf(ttm,"\n");

    if(tracing && ttm->frames.top < 0) {
	xprintf(ttm,"trace: no frame to trace\n");
	fflush(stderr);
	return;
    }	
    frame = &ttm->frames.stack[depth];
    xprintf(ttm,"[%02d] ",depth);
    if(tracing)
	xprintf(ttm,"%s ",(entering?"begin:":"end:  "));
    traceframe(ttm,frame,entering);

    /* Dump the contents of result or err if !entering */
    if(!entering) {
	if(err == TTM_NOERR)
	    xprintf(ttm," => |%s|",vscontents(ttm->vs.result));
	else	
	    xprintf(ttm," => %s",ttmerrname(err));
    } 
    xprintf(ttm,"\n");
    fflush(stderr);
}

/**
Trace a top frame in the frame stack.
*/
static void
trace(TTM* ttm, TTMERR err, int entering, int tracing)
{
    trace1(ttm, err, ttm->frames.top, entering, tracing);
}

void
tobin(unsigned char x)
{
    static char digits[8+1];
    size_t i,nbits = sizeof(x)*8;
    memset(digits,0,sizeof(digits));
    for(i=0;i<nbits;i++) {
	if(x & (1<<i)) digits[(nbits-1) - i] = '1'; else digits[(nbits-1) - i] = '0';
    }
fprintf(stderr,"%s\n",digits);
}

void
frombin(const char* digits)
{
    const char* p;
    unsigned char x = 0;
    size_t nbits = sizeof(x)*8;
    if(strlen(digits) > nbits) {
	fprintf(stderr,"too many digits: |%s|\n",digits);
	return;
    }
    for(p=digits;*p;p++) {
	x = (x << 1);
	if(*p == '1') x |= 1;
	else if(*p == '0') x &= 0xfffffffe;
    }
fprintf(stderr,"%08x\n",x);
}

/**************************************************/
/* Error reporting */

static TTMERR
failx(TTM* ttm, TTMERR eno, const char* file, int line, const char* fmt, ...)
{
    TTMERR err = TTM_NOERR;
    va_list ap;

    err = failxcxt(ttm,eno,file,line);
    if(fmt != NULL) {
	va_start(ap, fmt);
	vfprintf(stderr,fmt,ap);
	va_end(ap);
    }
    return err;
}

/* shorten text to given length + "..." */
static char*
shorten(const char* text,size_t len)
{
    size_t tlen, outlen;
    char* output = NULL;
    if(text == NULL) text = "";
    tlen = strlen(text);
    if(tlen < len) len = tlen;
    outlen = len+strlen("...");
    output = (char*)malloc(outlen+1);
    assert(output != NULL);
    memcpy(output,text,len);
    strcpy(output+len,"...");
    return output;
}

/* Print context */
static TTMERR
failxcxt(TTM* ttm, TTMERR eno, const char* file, int line)
{
    fprintf(stderr,"Fatal error: %s(%d) %s\n",ttmerrname(eno),(int)eno,ttmerrmsg(eno));
    fprintf(stderr,"\twhere: %s:%d\n",file,line);
    fprintf(stderr,"\tinput line=%d\n",ttm->flags.lineno);
    if(ttm != NULL) {
	char* passivetext = NULL;
	char* activetext = NULL;
	/* Dump the frame stack */
        fprintf(stderr,"frame stack:\n-------------------------\n");
        dumpstack(ttm);
	fprintf(stderr,"-------------------------\n");
	/* Dump passive and active strings*/
	xprintf(ttm,"failx.context:\n");
	passivetext = cleanstring(vscontents(ttm->vs.passive),"",NULL);
	activetext = cleanstring(vscontents(ttm->vs.active)+vsindex(ttm->vs.active),"",NULL);
	if(!ttm->opts.verbose) { /* Shorten active and passive printout */
	    char* shorted = NULL;
	    shorted = shorten(passivetext,SHORTTEXTLEN);
	    nullfree(passivetext);
	    passivetext = shorted;
	    shorted = shorten(activetext,SHORTTEXTLEN);
	    nullfree(activetext);
	    activetext = shorted;
	}
	xprintf(ttm,"\tpassive=|%s|\n",passivetext);
	xprintf(ttm,"\tactive=|%s|\n",activetext);
	nullfree(passivetext);
	nullfree(activetext);
    }
    fflush(stderr);
    return THROW(eno);
}

static void
fail(TTM* ttm, TTMERR eno, const char* file, int line)
{
    failx(ttm,eno,file,line,NULL);
    exit(1);
}

/* Name an message for each error.
   Indexed by |err|.
   WARNING: there are holes in this list, so array indexing cannot be used.
*/

static struct TTMERRINFO {
    TTMERR err;
    const char* errname;
    const char* errmsg;
} ttmerrinfo[] = {
{TTM_NOERR, "TTM_NOERR", "No error"},
{TTM_ERROR, "TTM_ERROR", "Unknown Error"},
{TTM_ENONAME, "TTM_ENONAME", "Dictionary Name Not Found"},
{TTM_EDUPNAME, "TTM_EDUPNAME", "Attempt to create duplicate name"},
{TTM_ENOPRIM, "TTM_ENOPRIM", "Primitives Not Allowed"},
{TTM_EFEWPARMS, "TTM_EFEWPARMS", "Too Few Parameters Given"},
{TTM_EFORMAT, "TTM_EFORMAT", "Incorrect Format"},
{TTM_EQUOTIENT, "TTM_EQUOTIENT", "Quotient Is Too Large"},
{TTM_EDECIMAL, "TTM_EDECIMAL", "Decimal Integer Required"},
{TTM_EMANYDIGITS, "TTM_EMANYDIGITS", "Too Many Digits"},
{TTM_EMANYSEGMARKS, "TTM_EMANYSEGMARKS", "Too Many Segment Marks"},
{TTM_EMEMORY, "TTM_EMEMORY", "Dynamic Storage Overflow"},
{TTM_EPARMROLL, "TTM_EPARMROLL", "Parm Roll Overflow"},
{TTM_EINPUTROLL, "TTM_EINPUTROLL", "Input Roll Overflow"},
{TTM_EIO, "TTM_EIO", "An I/O Error Occurred"},
{TTM_ETTM, "TTM_ETTM", "A TTM Processing Error Occurred"},
{TTM_ENOTNEGATIVE, "TTM_ENOTNEGATIVE", "Only unsigned decimal integers"},
/* messages new to this implementation */
{TTM_ESTACKOVERFLOW, "TTM_ESTACKOVERFLOW", "Stack overflow"},
{TTM_ESTACKUNDERFLOW, "TTM_ESTACKUNDERFLOW", "Stack Underflow"},
{TTM_EEXECCOUNT, "TTM_EEXECCOUNT", "too many executions"},
{TTM_EMANYINCLUDES, "TTM_EMANYINCLUDES", "Too many includes"},
{TTM_EINCLUDE, "TTM_EINCLUDE", "Cannot read Include file"},
{TTM_ERANGE, "TTM_ERANGE", "index out of legal range"},
{TTM_EMANYPARMS, "TTM_EMANYPARMS", "Number of parameters greater than MAXARGS"},
{TTM_EEOS, "TTM_EEOS", "Unexpected end of string"},
{TTM_EASCII, "TTM_EASCII", "ASCII characters only"},
{TTM_EUTF8, "TTM_EUTF8", "Illegal UTF-8 codepoint"},
{TTM_ETTMCMD, "TTM_ETTMCMD", "Illegal #<ttm> command"},
{TTM_ETIME, "TTM_ETIME", "Gettimeofday() failed"},
{TTM_EEMPTY, "TTM_EEMPTY", "Empty argument"},
{TTM_ENOCLASS, "TTM_ENOCLASS", "Character Class Name Not Found"},
{TTM_EINVAL, "TTM_EINVAL", "Invalid argument"},
{TTM_ELOCKED, "TTM_ELOCKED", "Attempt to modify/erase a locked function"},
{TTM_EEOF, "TTM_EEOF", "EOF encountered on input"},
{TTM_EACCESS, "TTM_EACCESS", "File not accessible or wrong mode"},
{TTM_EBADCALL, "TTM_EBADCALL", "Malformed function call"},
{TTM_NOERR, NULL, NULL}, /* List termination signal */
};

static const char*
ttmerrmsg(TTMERR err)
{
    const char* msg = NULL;
    static char msg0[128];

    /* System error? */
    if(((int)err) > 0) /* System error from (usually) errno */
    {
	msg = (const char *) strerror((int)err);
	if(msg == NULL) msg = "Unknown Sysem Error";
    } else {
	struct TTMERRINFO* tei = NULL;
	/* Linear search */
	for(tei=ttmerrinfo;tei->errname != NULL;tei++) {/* stop at end of list signal */	
	    if(tei->err == err) {msg = tei->errmsg; break;}
	}	
	if(msg == NULL) {
	    snprintf(msg0,sizeof(msg0),"Unknown error: %d",(int)err);
	    msg = msg0;
	}
    }
    return msg;
}

static const char*
ttmerrname(TTMERR err)
{
    const char* errname = NULL;
    static char ename0[128];
    struct TTMERRINFO* tei = NULL;

    /* Linear search */
    for(tei=ttmerrinfo;tei->errname != NULL;tei++) {/* stop at end of list signal */	
	if(tei->err == err) {errname = tei->errname; break;}
    }	
    if(errname == NULL) {
	snprintf(ename0,sizeof(ename0),"TTM_E_%d",-(int)err);
	errname = ename0;
    }
    return errname;
}

/**************************************************/

/* Hack to suppress compiler warnings about selected unused static functions */
static void
dbgsuppresswarnings(void)
{
    void* ignore;
    (void)ignore;
    ignore = (void*)dbgsuppresswarnings;
    (void)xprintf;
    (void)traceframe;
    (void)trace1;
    (void)trace;
#ifdef GDB
    (void)printwithpos;
    (void)dumpdict0;
    (void)dumpnames;
    (void)dumpnamesplus;
    (void)dumpcharclasses;
    (void)dumpprops;
    (void)dumpframe;
    (void)dumpstack;
#endif
}
