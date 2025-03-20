#ifdef CATCH

#define THROW(err) ttmthrow(err)
#define FAILX(ttm,eno,fmt,...) failx(ttm,eno,__LINE__,fmt  __VA_OPT__(,) __VA_ARGS__)
#define FAIL(ttm,eno) fail(ttm,eno,__LINE__)

static void
ttmbreak(TTMERR err)
{
    return;
}

static TTMERR
ttmthrow(TTMERR err)
{
    if(err != TTM_NOERR)
	ttmbreak(err);
    return err;
}
#else
#define THROW(err) (err)
#endif

static void
xprintf(const char* fmt,...)
{
    va_list ap;
    va_start(ap,fmt);
    vxprintf(fmt,ap);
    va_end(ap);
}

static void
vxprintf(const char* fmt,va_list ap)
{
    static char xbuf[1<<14] = {'\0'};
    size_t xlen = strlen(xbuf);
    size_t xfinal = 0;
    char* p = &xbuf[xlen];

    assert(sizeof(xbuf) >= (4*xlen)+1);
    (void)vsnprintf(p, sizeof(xbuf) - xlen, fmt, ap);
    xfinal = strlen(xbuf);
    if(xfinal > 0) {
	utf8* segtmp = NULL;
	utf8* ctltmp = NULL;
	int hasnewline = (xbuf[xfinal - 1] == '\n'); /* remember this */
	if(hasnewline) xbuf[xfinal - 1] = '\0'; /* temporarily elide the final '\n' */
	segtmp = desegmark((utf8*)xbuf,NULL);
	ctltmp = decontrol(segtmp,"",&xfinal); /* escape all controls */
	strncpy(xbuf,(const char*)ctltmp,sizeof(xbuf));
	nullfree(segtmp); nullfree(ctltmp);
	if(hasnewline) xbuf[xfinal] = '\n'; /* restore trailing newline */
	fprintf(stderr,"%s",xbuf);
        xbuf[0] = '\0'; /* reset */
    }
    fflush(stderr);
}

#if DEBUG > 0
void
scanx(TTM* ttm, utf8** cp8p, int* ncpp, int* stopp, TTMERR* errp)
{
    utf8* cp8 = *cp8p;
    int ncp = u8size(cp8);
#if DEBUG > 1
    fprintf(stderr,"consume=%c",*(char*)cp8);
#endif
    if((*errp=scan1(ttm,&cp8,&ncp)))
        {*stopp = 1; goto done;}
#if DEBUG > 1
    else
        fprintf(stderr," next=%c\n",*(char*)cp8);
#endif
done:
    *cp8p = cp8;
    *ncpp = ncp;
}

void
statetracex(TTM* ttm, utf8* cp8)
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

#if DEBUG > 0
#define SCAN1 scanx(ttm,&cp8,&ncp,&stop,&err)
#else /*DEBUG==0*/
#define SCAN1 do{if((err=scan1(ttm,&cp8,&ncp))) {stop = 1; goto done;}}while(0)
#endif /*DEBUG==0*/

#ifdef GDB
/**
Dump keys of the ttm->tables.dictionary
@param ttm
@param dict to dump
@param printvalues 1=>print both key and value; 0=>print key only
@return void
*/
static void
dumpdict0(struct HashTable* dict, int printvalues)
{
    int i;
    for(i=0;i<HASHSIZE;i++) {
	int first;
	struct HashEntry* entry = dict->table[i].next;
	for(first=1;entry != NULL;first=0) {
	    Function* str = (Function*)entry;
	    if(first) fprintf(stderr,"[%3d]",i);
	    xprintf(" |%s%s|",str->entry.name,(str->builtin?"*":""));
	    if(printvalues) {
		if(str->builtin)
		    xprintf(" = builtin");
		else
		    xprintf(" = |%s|",str->body);
	    }
	    entry = entry->next;
	}
	xprintf("\n");
    }
}

static void
dumpnames(TTM* ttm)
{
    dumpdict0(&ttm->tables.dictionary,0);
}

static void
dumpcharclasses(TTM* ttm)
{
    dumpdict0(&ttm->tables.charclasses,1);
}

static const utf8*
printwithpos(VString* vs)
{
    static utf8 content[1<<14];
    utf8* p = (utf8*)vs->content;
    utf8* q = content;
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
    *q = '\0';
    return content;
}    
#endif /*GDB*/

static void
dumpframe(Frame* frame)
{
    size_t i = 0;

    xprintf("frame{active=%d argc=%zu",frame->active,frame->argc);
    for(i=0;i<frame->argc;i++) {
	xprintf(" [%zu] |%s|",i,frame->argv[i]);
    }
    xprintf("}\n");
    fflush(stderr);
}

/**
Dump the stack
*/
static void
dumpstack(TTM* ttm)
{
    size_t i;
    for(i=1;i<=ttm->stack.next;i++) {
	xprintf("[%zu] ",i);
	dumpframe(&ttm->stack.stack[i]);
	xprintf("\n");
    }
    fflush(stderr);
}

static void
traceframe(TTM* ttm, Frame* frame, int traceargs)
{
    utf8 tag[1+(4*MAXCP8SIZE)]; /* 4 codepoints + NUL8 */
    utf8* p;
    int ncp;
    size_t i = 0;

    if(frame->argc == 0) {
	xprintf("#<empty frame>");
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
    xprintf("%s",tag);
    xprintf("%s",frame->argv[0]);
    if(traceargs) {
	for(i=1;i<frame->argc;i++) {
	    xprintf("%s%s",ttm->meta.semic,frame->argv[i]);
	}
    }
    xprintf("%s",ttm->meta.closec);
    fflush(stderr);
}

static void
trace1(TTM* ttm, int depth, int entering, int tracing)
{
    Frame* frame;

    if(!ttm->flags.newline) xprintf("\n");

    if(tracing && ttm->stack.next == 0) {
	xprintf("trace: no frame to trace\n");
	fflush(stderr);
	return;
    }	
    frame = &ttm->stack.stack[depth];
    xprintf("[%02d] ",depth);
    if(tracing)
	xprintf("%s ",(entering?"begin:":"end:  "));
    traceframe(ttm,frame,entering);
    /* Dump the contents of result if !entering */
    if(!entering) {
	xprintf(" => |%s|",vscontents(ttm->vs.result));
    } 
    xprintf("\n");
    fflush(stderr);
}

/**
Trace a top frame in the frame stack.
*/
static void
trace(TTM* ttm, int entering, int tracing)
{
    trace1(ttm, ttm->stack.next-1, entering, tracing);
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
failx(TTM* ttm, TTMERR eno, int line, const char* fmt, ...)
{
    TTMERR err = TTM_NOERR;
    va_list ap;

    if(fmt != NULL) {
	va_start(ap, fmt);
	vxprintf(fmt,ap);
	va_end(ap);
    }
    if((err = failxcxt(ttm,eno,line))) goto done;
done:
    return err;
}

/* Print context */
static TTMERR
failxcxt(TTM* ttm, TTMERR eno, int line)
{
    fprintf(stderr,"Fatal error:");
    if(line >= 0) fprintf(stderr," line=%d",line);
    fprintf(stderr," (%d) %s\n",eno,errstring(eno));
    if(ttm != NULL) {
	/* Dump the frame stack */
	dumpstack(ttm);
	/* Dump passive and active strings*/
	xprintf("failx.context: passive=|%s| active=|%s|\n",
		vscontents(ttm->vs.passive),
		vscontents(ttm->vs.active)+vsindex(ttm->vs.active));
    }
    fflush(stderr);
    return THROW(eno);
}

static void
fail(TTM* ttm, TTMERR eno, int line)
{
    failx(ttm,eno,line,NULL);
    exit(1);
}

static const char*
errstring(TTMERR err)
{
    const char* msg = NULL;
    switch(err) {
    case TTM_NOERR: msg="No error"; break;
    case TTM_ENONAME: msg="Dictionary Name Not Found"; break;
    case TTM_EDUPNAME: msg="Attempt to create duplicate name"; break;
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
    case TTM_EIO: msg="An I/O Error Occurred"; break;
    case TTM_ETTM: msg="A TTM Processing Error Occurred"; break;
    case TTM_ENOTNEGATIVE: msg="Only unsigned decimal integers"; break;
    /* messages new to this implementation */
    case TTM_ESTACKOVERFLOW: msg="Stack overflow"; break;
    case TTM_ESTACKUNDERFLOW: msg="Stack Underflow"; break;
    case TTM_EEXECCOUNT: msg="too many executions"; break;
    case TTM_EMANYINCLUDES: msg="Too many includes"; break;
    case TTM_EINCLUDE: msg="Cannot read Include file"; break;
    case TTM_ERANGE: msg="index out of legal range"; break;
    case TTM_EMANYPARMS: msg="Number of parameters greater than MAXARGS"; break;
    case TTM_EEOS: msg="Unexpected end of string"; break;
    case TTM_EASCII: msg="ASCII characters only"; break;
    case TTM_EUTF8: msg="Illegal UTF-8 codepoint"; break;
    case TTM_ETTMCMD: msg="Illegal #<ttm> command"; break;
    case TTM_ETIME: msg="Gettimeofday() failed"; break;
    case TTM_EEMPTY: msg="Empty argument"; break;
    case TTM_ENOCLASS: msg="Character Class Name Not Found"; break;
    case TTM_EINVAL: msg="Invalid argument"; break;
    case TTM_EOTHER: msg="Unknown Error"; break;
    }
    return msg;
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
    (void)dumpcharclasses;
    (void)dumpframe;
    (void)dumpstack;
#endif
}
