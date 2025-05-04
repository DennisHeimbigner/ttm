#ifdef CATCH

/**************************************************/
static void
ttmbreak(TTMERR err)
{
    return;
}

static TTMERR
ttmthrow(TTMERR err)
{
    if(err != TTM_NOERR) {
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

    xfile = ttm->io.stderr->file;
    hasnl = ttm->debug.xpr.outnl;
    
    xbuf = ttm->debug.xpr.xbuf;
    xsize = sizeof(ttm->debug.xpr.xbuf);
    xlen = strlen(xbuf);
    assert(xsize >= (4*xlen)+1);

    p = &xbuf[xlen];
    (void)vsnprintf(p, xsize - xlen, fmt, ap);

    xfinal = strlen(xbuf);
    if(xfinal > 0) {
	utf8* tmp = NULL;
	hasnl = (xbuf[xfinal - 1] == '\n'); /* remember this */
	if(hasnl) xbuf[xfinal - 1] = '\0'; /* temporarily elide the final '\n' */
	tmp = cleanstring((utf8*)xbuf,NULL,NULL);
	strncpy(xbuf,(const char*)tmp,xsize);
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
    fflush(stderr);
    ttm->debug.xpr.outnl = hasnl;
}

#if DEBUG > 0
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

#ifdef GDB
/**
Dump keys of the ttm->tables.dictionary
@param ttm
@param dict to dump
@param printvalues 1=>print both key and value; 0=>print key only
@return void
*/
static void
dumpdict0(TTM* ttm, struct HashTable* dict, int printvalues)
{
    int i;
    for(i=0;i<HASHSIZE;i++) {
	int first;
	struct HashEntry* entry = dict->table[i].next;
	for(first=1;entry != NULL;first=0) {
	    Function* str = (Function*)entry;
	    if(first) fprintf(stderr,"[%3d]",i);
	    xprintf(ttm,"	|%s%s|",str->entry.name,(str->fcn.builtin?"*":""));
	    if(printvalues) {
		if(str->fcn.builtin)
		    xprintf(ttm," = builtin");
		else
		    xprintf(ttm," = |%s|",vscontents(str->fcn.body));
	    }
	    xprintf(ttm,"\n");
	    entry = entry->next;
	}
	xprintf(ttm,"\n");
    }
}

static void
dumpnames(TTM* ttm)
{
    dumpdict0(ttm,&ttm->tables.dictionary,0);
}

static void
dumpnamesplus(TTM* ttm)
{
    dumpdict0(ttm,&ttm->tables.dictionary,1);

}

static void
dumpcharclasses(TTM* ttm)
{
    dumpdict0(ttm,&ttm->tables.charclasses,1);
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
Dump the stack
*/
static void
dumpstack(TTM* ttm)
{
    int i;
    for(i=0;i<=ttm->frames.top;i++) {
	xprintf(ttm,"[%zu] ",i);
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
    utf8 tag[1+(4*MAXCP8SIZE)]; /* 4 codepoints + NUL8 */
    utf8* p;
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
	    char* cleaned = (char*)cleanstring(frame->argv[i],"\t",NULL);
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
trace1(TTM* ttm, int depth, int entering, int tracing)
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
    /* Dump the contents of result if !entering */
    if(!entering) {
	xprintf(ttm," => |%s|",vscontents(frameresult(ttm)));
    } 
    xprintf(ttm,"\n");
    fflush(stderr);
}

/**
Trace a top frame in the frame stack.
*/
static void
trace(TTM* ttm, int entering, int tracing)
{
    trace1(ttm, ttm->frames.top, entering, tracing);
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

/* Print context */
static TTMERR
failxcxt(TTM* ttm, TTMERR eno, const char* file, int line)
{
    fprintf(stderr,"Fatal error:");
    fprintf(stderr," where: %s:%d",file,line);
    fprintf(stderr," input line=%d",ttm->flags.lineno);
    fprintf(stderr," (errno = %d) %s\n",eno,errstring(eno));
    if(ttm != NULL) {
	/* Dump the frame stack */
	dumpstack(ttm);
	/* Dump passive and active strings*/
	xprintf(ttm,"failx.context: passive=|%s| active=|%s|\n",
		vscontents(ttm->vs.passive),
		vscontents(ttm->vs.active)+vsindex(ttm->vs.active));
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

static const char*
errstring(TTMERR err)
{
    const char* msg = NULL;

   /* System error? */
   if(((int)err) > 0) /* System error from (usually) errno */
   {
      msg = (const char *) strerror((int)err);
      if(msg == NULL) msg = "Unknown Sysem Error";
   } else switch(err) {
    case TTM_NOERR: msg="No error"; break;
    case TTM_ERROR: msg="Unknown Error"; break;
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
    case TTM_ELOCKED: msg="Attempt to modify/erase a locked function"; break;
    case TTM_EEOF: msg = "EOF encountered on input"; break;
    case TTM_EACCESS: msg = "File not accessible or wrong mode"; break;
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
    (void)dumpnamesplus;
    (void)dumpcharclasses;
    (void)dumpframe;
    (void)dumpstack;
#endif
}
