/* Forward Types */

struct Builtin;

/**************************************************/

/* Define hdr/trailer actions for ttm_XXX builtin function calls */
#if 1
#define TTMFCN_DECLS(ttm,frame) size_t unused
#define TTMFCN_BEGIN(ttm,frame,vsresult) UNUSED(unused)
#define TTMFCN_END(ttm,frame,vsresult)
#else
#define TTMFCN_DECLS(ttm,frame) size_t ttmfcn_mark = 0
#define TTMFCN_BEGIN(ttm,frame,vsresult) do{ttmfcn_mark = vslength(vsresult);}while(0)
#define TTMFCN_END(ttm,frame,vsresult) do{vssetlength(vsresult,ttmfcn_mark);}while(0)
#endif

/* Forward */
static void defineBuiltinFunction1(TTM* ttm, struct Builtin* bin);
static void defineBuiltinFunctions(TTM* ttm);

/* Dictionary Operations */
static TTMERR
ttm_ap(TTM* ttm, Frame* frame, VString* result) /* Append to a string */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    VString* body = NULL;
    utf8* apstring = NULL;
    Function* str = NULL;
    size_t aplen;

    TTMFCN_BEGIN(ttm,frame,result);
    if((str = dictionaryLookup(ttm,frame->argv[1]))==NULL) {/* Define the string */
	err = ttm_ds(ttm,frame,result);
	goto done;
    }
    if(str->fcn.builtin) {err = TTM_ENOPRIM; goto done;}
    apstring = frame->argv[2];
    aplen = strlen((const char*)apstring);
    body = str->fcn.body;
    vsappendn(body,(const char*)apstring,aplen);
    vsindexset(body,vslength(body));
done:
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

/**
The semantics of #<cf>
have been changed. See ttm.html
*/

static TTMERR
ttm_cf(TTM* ttm, Frame* frame, VString* result) /* Copy a function */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    utf8* newname = frame->argv[1];
    utf8* oldname = frame->argv[2];
    Function* newfcn = dictionaryLookup(ttm,newname);
    Function* oldfcn = dictionaryLookup(ttm,oldname);
    struct HashEntry saveentry;

    TTMFCN_BEGIN(ttm,frame,result);
    if(oldfcn == NULL) {err = FAILNONAMES(oldname); goto done;}
    if(newfcn == NULL) {
	/* create a new string object */
	newfcn = newFunction(ttm);
	assert(newfcn->entry.name == NULL);
	newfcn->entry.name = (utf8*)strdup((const char*)newname);
	dictionaryInsert(ttm,newfcn);
    }
    saveentry = newfcn->entry;
    *newfcn = *oldfcn;
    /* Keep new hash entry */
    newfcn->entry = saveentry;
    /* Do pointer fixup */
    if(newfcn->fcn.body != NULL)
	newfcn->fcn.body = vsclone(newfcn->fcn.body);
done:
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_cr(TTM* ttm, Frame* frame, VString* result) /* Mark for creation */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);

    TTMFCN_BEGIN(ttm,frame,result);
    ttm_ss0(ttm,frame,CREATEINDEXONLY,NULL);
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_ds(TTM* ttm, Frame* frame, VString* result)
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    Function* str = NULL;
    TTMFCN_BEGIN(ttm,frame,result);
    str = dictionaryLookup(ttm,frame->argv[1]);
    if(str != NULL && str->fcn.locked) {err = TTM_ELOCKED; goto done;}
    if(str != NULL) { /* clean for re-use */
	nullfree(str->fcn.body);
	memset(&str->fcn,0,sizeof(struct FcnData));
    } else {
	/* create a new string object */
	str = newFunction(ttm);
	assert(str->entry.name == NULL);
	str->entry.name = (utf8*)strdup((const char*)frame->argv[1]);
	dictionaryInsert(ttm,str);
    }
    str->fcn.trace = ttm->debug.trace;
    str->fcn.builtin = 0;
    str->fcn.maxargs = ARB;
    str->fcn.sv = SV_SV;
    str->fcn.novalue = 0;
    str->fcn.body = vsnew();
    vsappendn(str->fcn.body,(const char*)frame->argv[2],0);
done:
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_es(TTM* ttm, Frame* frame, VString* result) /* Erase string */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    size_t i;
    TTMFCN_BEGIN(ttm,frame,result);
    for(i=1;i<frame->argc;i++) {
	utf8* strname = frame->argv[i];
	Function* str = dictionaryLookup(ttm,strname);
	if(str != NULL) {
	    if(str->fcn.locked) {err = TTM_ELOCKED; goto done;}
	    dictionaryRemove(ttm,strname);
	    freeFunction(ttm,str); /* reclaim the string */
	}
    }
done:
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

/**
Helper function for #<sc> and #<ss> and #<cr>.
Copies from str->fcn.body to result
while substituting segment or creation marks.
@param ttm
@param frame
@param segmark for substituting 
@param segcountp # of segment counters inserted.
@return TTMERR
*/
static TTMERR
ttm_ss0(TTM* ttm, Frame* frame, size_t segmark, size_t* segcountp)
{
    TTMERR err = TTM_NOERR;
    Function* str;
    size_t segcount = 0;
    size_t i,nextsegindex,bodylen;
    size_t residual;
    size_t argc;
    int iscr = (segmark == CREATEINDEXONLY ? 1 : 0);
    const char* body = NULL;

    if(frame->argc < 2) goto done; /* Nothing to do */

    if((str = getdictstr(ttm,frame,1))==NULL) {err = FAILNONAME(1); goto done;}
    body = vscontents(str->fcn.body);
    if(body == NULL) bodylen = 0; else bodylen = strlen(body);
    residual = vsindex(str->fcn.body);
    if(residual >= bodylen) goto done; /* no substitution possible */

    /* copy the whole of str->fcn.body to ttm->vs.tmp for repeated replacement */
    vssetlength(ttm->vs.tmp,0);
    vsappendn(ttm->vs.tmp,body,bodylen);
  
    /* mimic str->fcn.residual => ttm->vs.tmp.index */
    vsindexset(ttm->vs.tmp,residual);

    /* Remember how many new segmarks were used and starting where */
    segcount = 0;
    nextsegindex = (iscr ? CREATEINDEXONLY : str->fcn.nextsegindex);

    /* Compute actual last argument+1 (3 for cr frame->argc for ss) */
    argc = (iscr ? 3 : frame->argc);

    /* Iterate over replacement args */
    for(i=2;i<argc;i++) {
	size_t segcount1 = 0; /* no. of hits for this arg */
	utf8* arg = frame->argv[i];
	segcount1 = ttm_mark1(ttm,ttm->vs.tmp,arg,nextsegindex);
	/* If we matched at least once, then bump the segindex for next match */
	if(!iscr && segcount1 > 0) nextsegindex++;
	segcount += segcount1;
    }
    if(!iscr) str->fcn.nextsegindex = nextsegindex; /* Remember the next segment mark to use */
    /* Replace the str body (after residual) with contents of ttm->vs.tmp */
    vsclear(str->fcn.body);
    vsappendn(str->fcn.body,vscontents(ttm->vs.tmp),vslength(ttm->vs.tmp));
    vsclear(ttm->vs.tmp);
done:
    if(segcountp) *segcountp = segcount;
    return THROW(err);
}

/**
Scan a string for a string and replace with a segment or creation mark.
@param ttm
@param vs holding the string being marked
@param target
@param pattern the string for which to search
@param mark the segment/create index for replacement
@return count of the number of replacements
*/
static size_t
ttm_mark1(TTM* ttm, VString* target, utf8* pattern, size_t mark)
{
    size_t segcount = 0;
    utf8 seg[SEGMARKSIZE] = empty_segmark;
    size_t patternlen;
    char* p;
    char* q;
    size_t residual; /* always start search from here */

    residual = vsindex(target);
    patternlen = strlen((const char*)pattern);
    if(patternlen > 0) { /* search only if possible success */
	/* Search for occurrences of pattern */
 	/* Note: p and q may change after replacement) */
	 for(;;) {
	    size_t delta;
	    p = vsindexp(target); /* Start search at residual */
	    q = strstr(p,(const char*)pattern); /* q points to start of next match or NULL */
	    if(q == NULL) break; /* Not found => done with this pattern => break */
	    /* we have a match, replace match by the mark */
	    delta = (size_t)(q - p);
	    /* Move index by delta */
	    vsindexskip(target,delta);
	    /* remove the matching string */
	    vsremoven(target,vsindex(target),patternlen);
	    /* rebuild seg mark and insert*/
	    segmark2utf8(seg,mark);
	    vsinsertn(target,vsindex(target),(const char*)seg,SEGMARKSIZE);
	    vsindexskip(target,SEGMARKSIZE); /* Skip past the segmark */
	    segcount++;
	}
    }
    vsindexset(target,residual); /* reset start point for search */
    return segcount;
}

static TTMERR
ttm_sc(TTM* ttm, Frame* frame, VString* result) /* Segment and count */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    char count[64];
    size_t nsegs;

    TTMFCN_BEGIN(ttm,frame,result);
    ttm_ss0(ttm,frame,SEGINDEXFIRST,&nsegs);
    snprintf(count,sizeof(count),"%zu",nsegs);
    /* Insert into result */
    vsappendn(result,(const char*)count,strlen(count));
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_ss(TTM* ttm, Frame* frame, VString* result) /* Segment and count */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);

    TTMFCN_BEGIN(ttm,frame,result);
    ttm_ss0(ttm,frame,SEGINDEXFIRST,NULL);
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

/* String Selection */

static TTMERR
ttm_cc(TTM* ttm, Frame* frame, VString* result) /* Call one character */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    Function* fcn;

    TTMFCN_BEGIN(ttm,frame,result);
    if((fcn = getdictstr(ttm,frame,1))==NULL) {err = FAILNONAME(1); goto done;}
    /* Check for pointing at trailing NUL */
    if(vsindex(fcn->fcn.body) < strlen((const char*)fcn->fcn.body)) {
	int ncp;
	utf8* p = (utf8*)vsindexp(fcn->fcn.body);
	ncp = u8size(p);
	vsappendn(result,(const char*)p,ncp);
	vsindexskip(fcn->fcn.body,ncp);
    }
done:
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_cn(TTM* ttm, Frame* frame, VString* result) /* Call n characters (codepoints) */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    Function* fcn;
    long long ln;
    int n;
    int ncp, nbytes;
    const utf8* p = NULL;

    TTMFCN_BEGIN(ttm,frame,result);
    if((fcn = getdictstr(ttm,frame,2))==NULL) {err = FAILNONAME(2); goto done;}

    /* Get number of codepoints to extract */
    if(1 != sscanf((const char*)frame->argv[1],"%lld",&ln)) {err = TTM_EDECIMAL; goto done;}
    if(ln < 0) {err = TTM_ENOTNEGATIVE; goto done;}

    vsclear(result);
    n = (int)ln;
    p = (utf8*)vsindexp(fcn->fcn.body);
    nbytes = strlen((const char*)p);
    if(n == 0 || nbytes == 0) goto done;
    /* copy up to n codepoints or EOS encountered */
    while(n-- > 0) {
	p = (utf8*)vsindexp(fcn->fcn.body);
        if(isnul(p)) break; /* EOS */
	if((ncp = u8size(p))<=0) return TTM_EUTF8;
	vsappendn(result,(const char*)p,ncp);
	vsindexskip(fcn->fcn.body,ncp);
    }

done:
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_sn(TTM* ttm, Frame* frame, VString* result) /* Skip n characters */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    long long num;
    Function* str;
    const utf8* p;

    TTMFCN_BEGIN(ttm,frame,result);
    if((str = getdictstr(ttm,frame,2))==NULL) {err = FAILNONAME(2); goto done;}
    if(1 != sscanf((const char*)frame->argv[1],"%lld",&num)) {err = TTM_EDECIMAL; goto done;}
    if(num < 0) {err = TTM_ENOTNEGATIVE; goto done;}

    for(p=(utf8*)vsindexp(str->fcn.body);num-- > 0;) {
	int ncp = u8size(p);
	if(ncp < 0) {err = TTM_EUTF8; goto done;}
	if(isnul(p)) break;
	p += ncp;
	vsindexskip(str->fcn.body,ncp);
    }
done:
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_isc(TTM* ttm, Frame* frame, VString* vsresult) /* Initial character scan; moves residual pointer */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    Function* str;
    utf8* t;
    utf8* f;
    utf8* value;
    utf8* arg;
    size_t arglen;
    size_t slen;

    TTMFCN_BEGIN(ttm,frame,vsresult);
    str = getdictstr(ttm,frame,2);

    arg = frame->argv[1];
    arglen = strlen((const char*)arg);
    t = frame->argv[3];
    f = frame->argv[4];

    /* check for initial string match */
    if(strncmp(vsindexp(str->fcn.body),(const char*)arg,arglen)==0) {
	value = t;
	vsindexskip(str->fcn.body,arglen);
	slen = vslength(str->fcn.body);
	if(vsindex(str->fcn.body) > slen) vsindexset(str->fcn.body,slen);
    } else
	value = f;
    vsclear(vsresult);
    vsappendn(vsresult,(const char*)value,strlen((const char*)value));
    TTMFCN_END(ttm,frame,vsresult);
    return THROW(err);
}

static TTMERR
ttm_scn(TTM* ttm, Frame* frame, VString* result) /* Character scan */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    Function* str;
    size_t arglen;
    utf8* f;
    utf8* arg;
    utf8* p;
    utf8* p0;

    TTMFCN_BEGIN(ttm,frame,result);
    str = getdictstr(ttm,frame,2);

    arg = frame->argv[1];
    arglen = strlen((const char*)arg);
    f = frame->argv[3];

    /* check for sub string match */
    p0 = (utf8*)vsindexp(str->fcn.body);
    p = (utf8*)strstr((const char*)p0,(const char*)arg);
    vsclear(result);
    if(p == NULL) {/* no match; return argv[3] */
	vsappendn(result,(const char*)f,strlen((const char*)f));
    } else {/* return chars from residual ptr to location of string */
	ptrdiff_t len = (p - p0);
	if(len > 0)
	    vsappendn(result,(const char*)p0,(size_t)len);
	/* move rp past matched string */
	vsindexskip(str->fcn.body,(len + arglen));
    }
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_cp(TTM* ttm, Frame* frame, VString* result) /* Call parameter */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    Function* fcn;
    size_t delta;
    utf8* rp;
    utf8* rp0;
    int depth;

    TTMFCN_BEGIN(ttm,frame,result);
    fcn = getdictstr(ttm,frame,1);

    rp0 = (utf8*)vsindexp(fcn->fcn.body);
    rp = rp0;
    depth = 0;
    for(;;) {
	int ncp = u8size(rp);
	if(isnul(rp)) {
	    break;
	} else if(u8equal(rp,ttm->meta.semic)) {
	    if(depth == 0) break; /* reached unnested semicolon*/
	} else if(u8equal(rp,ttm->meta.lbrc)) {
	    depth++;
	} else if(u8equal(rp,ttm->meta.rbrc)) {
	    depth--;
	}
	rp += ncp;
    }
    delta = (rp - rp0);
    vsclear(result);
    if(delta > 0)
	vsappendn(result,(const char*)rp0,delta);
    vsindexskip(fcn->fcn.body,delta);
    if(*rp != NUL8) vsindexskip(fcn->fcn.body,1);
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_cs(TTM* ttm, Frame* frame, VString* result) /* Call segment */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    Function* fcn;
    utf8* p;
    utf8* p0;
    size_t delta;

    TTMFCN_BEGIN(ttm,frame,result);
    fcn = getdictstr(ttm,frame,1);

    /* Locate the next segment mark */
    /* Unclear if create marks also qualify; assume yes */
    p0 = (utf8*)vsindexp(fcn->fcn.body);
    p = p0;
    for(;;) {
	int ncp;
	if((ncp = u8size(p))<=0) {err = TTM_EUTF8; goto done;}
	if(isnul(p) || issegmark(p) || iscreatemark(p)) break;
	p += ncp;
    }
    delta = (p - p0);
    if(delta > 0) {
	vsclear(result);
	vsappendn(result,(const char*)p0,delta);
    }
    /* set residual pointer correctly */
    vsindexskip(fcn->fcn.body,delta);
    if(*p != NUL8) vsindexskip(fcn->fcn.body,SEGMARKSIZE);
done:
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_rrp(TTM* ttm, Frame* frame, VString* result) /* Reset residual pointer */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    Function* str = getdictstr(ttm,frame,1);
    TTMFCN_BEGIN(ttm,frame,result);
    vsindexset(str->fcn.body,0);
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_eos(TTM* ttm, Frame* frame, VString* vsresult) /* Test for end of string */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    Function* str;
    utf8* t;
    utf8* f;
    utf8* result;
    size_t bodylen;

    TTMFCN_BEGIN(ttm,frame,vsresult);
    str = getdictstr(ttm,frame,1);
    t = frame->argv[2];
    f = frame->argv[3];
    bodylen = strlen((const char*)str->fcn.body);
    result = (vsindex(str->fcn.body) >= bodylen ? t : f);
    vsclear(vsresult);
    vsappendn(vsresult,(const char*)result,strlen((const char*)result));
    TTMFCN_END(ttm,frame,vsresult);
    return THROW(err);
}

/* String Scanning Operations */

static TTMERR
ttm_gn(TTM* ttm, Frame* frame, VString* result) /* Give n characters from argument string*/
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    utf8* snum = frame->argv[1];
    utf8* s = frame->argv[2];
    long long num;
    int nbytes = 0;

    TTMFCN_BEGIN(ttm,frame,result);
    if(1 != sscanf((const char*)snum,"%lld",&num)) {err = TTM_EDECIMAL; goto done;}
    if(num > 0) {
	nbytes = u8ith(s,(int)num);
	if(nbytes <= 0) {err = TTM_EUTF8; goto done;}
	vsappendn(result,(const char*)s,(size_t)nbytes);
    } else if(num < 0) {
	num = -num;
	nbytes = u8ith(s,(int)num);
	if(nbytes <= 0) {err = TTM_EUTF8; goto done;}
	s += nbytes;
	vsappendn(result,(const char*)s,strlen((const char*)s));
    }
done:
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_zlc(TTM* ttm, Frame* frame, VString* result) /* Zero-level commas */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    utf8* s;
    utf8* p;
    int depth;

    TTMFCN_BEGIN(ttm,frame,result);
    s = frame->argv[1];
    vsclear(result);
    for(depth=0,p=s;*p;) {
	int count = u8size(p);
	if(isescape(p)) {
	    p += count;
	    count = u8size(p);
	    vsappendn(result,(const char*)p,count); /* suppress escape and copy escaped char */
	} else if(*p == COMMA && depth == 0) {
	    p += count; /* skip comma */
	    vsappendn(result,(const char*)ttm->meta.semic,u8size(ttm->meta.semic)); /*replace ',' */
	} else if(*p == LPAREN) {
	    depth++;
	    vsappendn(result,(const char*)p,count);
	    p += count; /* pass LPAREN */
	} else if(*p == RPAREN) {
	    depth--;
	    vsappendn(result,(const char*)p,count);
	    p += count; /* pass RPAREN */
	} else {
	    vsappendn(result,(const char*)p,count);
	    p += count; /* pass char */
	}
    }
    *p = NUL8; /* make sure it is terminated */
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_zlcp(TTM* ttm, Frame* frame, VString* result) /* Zero-level commas and parentheses; exact algorithm is unknown */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    /* A(B) and A,B will both give A;B and (A),(B),C will give A;B;C */
    utf8* s;
    utf8* p;
    int depth;
    int ncp;
    utf8cpa cpa;

    TTMFCN_BEGIN(ttm,frame,result);
    s = frame->argv[1];

    vsclear(result);
    for(depth=0,p=s;*p;p+=u8size(p)) {
	ncp = u8size(p);
	if(isescape(p)) {
	    vsappendn(result,(const char*)p,ncp);
	    p += ncp; ncp = u8size(p);
	    vsappendn(result,(const char*)p,ncp);
	} else if(depth == 0 && *p == COMMA) {
	    if((err = u8peek(p,1,cpa))) goto done;
	    if(*cpa != LPAREN)
		vsappendn(result,(const char*)ttm->meta.semic,u8size(ttm->meta.semic));
	} else if(*p == LPAREN) {
	    if(depth == 0 && p > s)
		vsappendn(result,(const char*)ttm->meta.semic,u8size(ttm->meta.semic));
	    if(depth > 0)
		vsappendn(result,(const char*)p,ncp);
	    depth++;
	} else if(*p == RPAREN) {
	    depth--;
	    if((err = u8peek(p,1,cpa))) goto done;
	    if(depth == 0 && *cpa == COMMA) {
		/* do nothing */
	    } else if(depth == 0 && *cpa == NUL8) {
		/* do nothing */
	    } else if(depth == 0) {
		vsappendn(result,(const char*)ttm->meta.semic,u8size(ttm->meta.semic));
	    } else /* depth > 0 */
		vsappendn(result,(const char*)p,ncp);
	} else
	    vsappendn(result,(const char*)p,ncp);
    }
done:
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_flip(TTM* ttm, Frame* frame, VString* result) /* Flip a string */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    utf8* s;
    utf8* p;
    int slen,i;

    TTMFCN_BEGIN(ttm,frame,result);
    s = frame->argv[1];
    slen = strlen((const char*)s);
    vsclear(result);
    p = s + slen;
    for(i=0;i<slen;i++) {
	int ncp;
	p = (utf8*)u8backup(p,s); /* backup 1 codepoint */
	ncp = u8size(p);
	vsappendn(result,(const char*)p,ncp);
    }
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_trl(TTM* ttm, Frame* frame, VString* result) /* translate to lower case */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    utf8* s;

    TTMFCN_BEGIN(ttm,frame,result);
    vsclear(result);
    s = frame->argv[1];
    {
#ifdef MSWINDOWS
#else
	size_t i,swlen,s8len;
	wchar_t* sw = NULL;

	/* Determine the size needed for the wide character string */
	if((swlen = (mbstowcs(NULL, (char*)s, 0) + 1))==((size_t)-1)) {err = TTM_EUTF8; goto done;}
	/* Allocate memory for the wide character string */
	if((sw = (wchar_t*)malloc(swlen * sizeof(wchar_t)))==NULL) {err = TTM_EMEMORY; goto done;}
	/* convert utf8 string to wide char (utf16 or utf32) */
	(void)mbstowcs(sw,(char*)s, swlen);
	/* Convert to lower case */
	for(i=0;i<swlen;i++)
	    sw[i] = (wchar_t)towlower(sw[i]);
	/* Convert back to utf8 */
	/* Get space required */
	if((s8len = (wcstombs(NULL, sw, 0) + 1))==((size_t)-1)) {err = TTM_EUTF8; goto done;}
	/* Allocate memory for the utf8 character string */
	vssetalloc(result,s8len);
	/* Insert into the result buffer */
	(void)wcstombs(vscontents(result),sw,swlen);
	/* Make length consistent */
	vssetlength(result,s8len-1);
#endif
    }
done:
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_thd(TTM* ttm, Frame* frame, VString* result) /* convert hex to decimal */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    char hex[8+1];
    char* p = NULL;
    char* q = NULL;
    unsigned long long un;
    char dec[32];

    TTMFCN_BEGIN(ttm,frame,result);
    p = (char*)frame->argv[1];
    for(q=hex;*p;) {
	if(!ishex(*p)) {err = TTM_EINVAL; goto done;}        
	*q++ = *p++;
    }
    *q = '\0';
    if(1 != sscanf(hex,"%llx",&un)) {err = TTM_EINVAL; goto done;}
    snprintf(dec,sizeof(dec),"%lld",(long long)un);
    vsappendn(result,dec,0);
done:
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

/* Shared helper for dcl and dncl */
static TTMERR
ttm_dcl0(TTM* ttm, Frame* frame, int negative, VString* result)
{
    TTMERR err = TTM_NOERR;
    Charclass* cl = NULL;
    cl = charclassLookup(ttm,frame->argv[1]);
    if(cl == NULL) {
	/* create a new charclass object */
	cl = newCharclass(ttm);
	assert(cl->entry.name == NULL);
	cl->entry.name = (utf8*)strdup((const char*)frame->argv[1]);
	charclassInsert(ttm,cl);
    }
    nullfree(cl->characters);
    cl->characters = (utf8*)strdup((const char*)frame->argv[2]);
    cl->negative = negative;
    return THROW(err);
}

static TTMERR
ttm_dcl(TTM* ttm, Frame* frame, VString* result) /* Define a negative class */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    TTMFCN_BEGIN(ttm,frame,result);
    ttm_dcl0(ttm,frame,0,result);
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_dncl(TTM* ttm, Frame* frame, VString* result) /* Define a negative class */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    TTMFCN_BEGIN(ttm,frame,result);
    ttm_dcl0(ttm,frame,1,result);
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_ecl(TTM* ttm, Frame* frame, VString* result) /* Erase a class */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    size_t i;

    TTMFCN_BEGIN(ttm,frame,result);
    for(i=1;i<frame->argc;i++) {
	utf8* clname = frame->argv[i];
	Charclass* cl = charclassRemove(ttm,clname);
	if(cl != NULL) {
	    freeCharclass(ttm,cl); /* reclaim the character class */
	}
    }
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_ccl(TTM* ttm, Frame* frame, VString* result) /* call class */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    Charclass* cl = NULL;
    Function* str = NULL;
    utf8* p;
    size_t rr0,rr1,delta;

    TTMFCN_BEGIN(ttm,frame,result);
    if((str = getdictstr(ttm,frame,2))==NULL) {err = FAILNONAME(2); goto done;}
    if((cl = charclassLookup(ttm,frame->argv[1]))==NULL) FAILNOCLASS(1);

    /* Starting at vsindex(str->fcn.body), locate first char not in class (or negative) */
    vsclear(result);
    rr1= (rr0 = vsindex(str->fcn.body));
    for(;;) {
	p = (utf8*)vsindexp(str->fcn.body);
	if(cl->negative && charclassMatch(p,cl->characters)) break;
	if(!cl->negative && !charclassMatch(p,cl->characters)) break;
	vsindexskip(str->fcn.body,u8size(p));
    }
    rr1 = vsindex(str->fcn.body);
    delta = (size_t)(rr1 - rr0);
    vsindexset(str->fcn.body,rr0); /* temporary */
    vsappendn(result,vsindexp(str->fcn.body),delta);
    vsindexset(str->fcn.body,rr1); /* final residual index */
done:
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_scl(TTM* ttm, Frame* frame, VString* result) /* skip class */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    Charclass* cl = NULL;
    Function* str = NULL;
    utf8* p;

    TTMFCN_BEGIN(ttm,frame,result);
    if((str = getdictstr(ttm,frame,2))==NULL) {err = FAILNONAME(2); goto done;}
    if((cl = charclassLookup(ttm,frame->argv[1]))==NULL) FAILNOCLASS(1);

    /* Starting at vsindex(str->fcn.body), locate first char not in class (or negative) */
    vsclear(result);
    for(;;) {
	p = (utf8*)vsindexp(str->fcn.body);
	if(cl->negative && charclassMatch(p,cl->characters)) break;
	if(!cl->negative && !charclassMatch(p,cl->characters)) break;
	vsindexskip(str->fcn.body,u8size(p));
    }
done:
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_tcl(TTM* ttm, Frame* frame, VString* result) /* Test class */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    Charclass* cl = NULL;
    Function* str = NULL;
    utf8* retval;
    utf8* t;
    utf8* f;

    TTMFCN_BEGIN(ttm,frame,result);
    if((cl = charclassLookup(ttm,frame->argv[1]))==NULL) FAILNOCLASS(1);
    if((str = getdictstr(ttm,frame,2))==NULL) {err = FAILNONAME(2); goto done;}

    t = frame->argv[3];
    f = frame->argv[4];
    if(str == NULL)
	retval = f;
    else {
	utf8* p = (utf8*)vsindexp(str->fcn.body);
	/* see if char at vsindex(str->fcn.body) is in class */
	if(cl->negative && !charclassMatch(p,cl->characters))
	    retval = t;
	else if(!cl->negative && charclassMatch(p,cl->characters))
	    retval = t;
	else
	    retval = f;
    }
    vsappendn(result,(const char*)retval,strlen((const char*)retval));
done:
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

/* Arithmetic Operators */

static TTMERR
ttm_abs(TTM* ttm, Frame* frame, VString* result) /* Obtain absolute value */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    utf8* slhs;
    long long lhs;
    char value[128];

    TTMFCN_BEGIN(ttm,frame,result);
    slhs = frame->argv[1];
    if(1 != sscanf((const char*)slhs,"%lld",&lhs)) {err = TTM_EDECIMAL; goto done;}
    if(lhs < 0) lhs = -lhs;
    snprintf(value,sizeof(value),"%lld",lhs);
    vsclear(result);
    vsappendn(result,value,strlen(value));
done:
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_ad(TTM* ttm, Frame* frame, VString* result) /* Add */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    utf8* srhs;
    long long lhs;
    long long rhs;
    char value[128];
    size_t i;

    TTMFCN_BEGIN(ttm,frame,result);
    for(lhs=0,i=1;i<frame->argc;i++) {
	srhs = frame->argv[i];
	if(1 != sscanf((const char*)srhs,"%lld",&rhs)) {err = TTM_EDECIMAL; goto done;}
	lhs += rhs;
    }
    snprintf(value,sizeof(value),"%lld",lhs);
    vsclear(result);
    vsappendn(result,value,strlen(value));
done:
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_dv(TTM* ttm, Frame* frame, VString* result) /* Divide and give quotient */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    utf8* slhs;
    long long lhs;
    utf8* srhs;
    long long rhs;
    char value[128];

    TTMFCN_BEGIN(ttm,frame,result);
    slhs = frame->argv[1];
    srhs = frame->argv[2];
    if(1 != sscanf((const char*)slhs,"%lld",&lhs)) {err = TTM_EDECIMAL; goto done;}
    if(1 != sscanf((const char*)srhs,"%lld",&rhs)) {err = TTM_EDECIMAL; goto done;}
    snprintf(value,sizeof(value),"%lld",lhs / rhs);
    vsclear(result);
    vsappendn(result,value,strlen(value));
done:
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_dvr(TTM* ttm, Frame* frame, VString* result) /* Divide and give remainder */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    utf8* slhs;
    long long lhs;
    utf8* srhs;
    long long rhs;
    char value[128];

    TTMFCN_BEGIN(ttm,frame,result);
    slhs = frame->argv[1];
    srhs = frame->argv[2];
    if(1 != sscanf((const char*)slhs,"%lld",&lhs)) {err = TTM_EDECIMAL; goto done;}
    if(1 != sscanf((const char*)srhs,"%lld",&rhs)) {err = TTM_EDECIMAL; goto done;}
    snprintf(value,sizeof(value),"%lld",lhs % rhs);
    vsclear(result);
    vsappendn(result,value,strlen(value));
done:
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_mu(TTM* ttm, Frame* frame, VString* result) /* Multiply */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    utf8* srhs;
    long long lhs;
    long long rhs;
    char value[128];
    size_t i;

    TTMFCN_BEGIN(ttm,frame,result);
    for(lhs=1,i=1;i<frame->argc;i++) {
	srhs = frame->argv[i];
	if(1 != sscanf((const char*)srhs,"%lld",&rhs)) {err = TTM_EDECIMAL; goto done;}
	lhs *= rhs;
    }
    snprintf(value,sizeof(value),"%lld",lhs);
    vsclear(result);
    vsappendn(result,value,strlen(value));
done:
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_su(TTM* ttm, Frame* frame, VString* result) /* Substract */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    utf8* slhs;
    long long lhs;
    utf8* srhs;
    long long rhs;
    char value[128];

    TTMFCN_BEGIN(ttm,frame,result);
    slhs = frame->argv[1];
    srhs = frame->argv[2];
    if(1 != sscanf((const char*)slhs,"%lld",&lhs)) {err = TTM_EDECIMAL; goto done;}
    if(1 != sscanf((const char*)srhs,"%lld",&rhs)) {err = TTM_EDECIMAL; goto done;}
    snprintf(value,sizeof(value),"%lld",lhs - rhs);
    vsclear(result);
    vsappendn(result,value,strlen(value));
done:
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_eq(TTM* ttm, Frame* frame, VString* result) /* Compare numeric equal */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    utf8* slhs;
    utf8* srhs;
    long long lhs,rhs;
    utf8* t;
    utf8* f;
    utf8* value;

    TTMFCN_BEGIN(ttm,frame,result);
    slhs = frame->argv[1];
    srhs = frame->argv[2];
    t = frame->argv[3];
    f = frame->argv[4];

    if(1 != sscanf((const char*)slhs,"%lld",&lhs)) {err = TTM_EDECIMAL; goto done;}
    if(1 != sscanf((const char*)srhs,"%lld",&rhs)) {err = TTM_EDECIMAL; goto done;}
    value = (lhs == rhs ? t : f);
    vsclear(result);
    vsappendn(result,(char*)value,strlen((const char*)value));
done:
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_gt(TTM* ttm, Frame* frame, VString* result) /* Compare numeric greater-than */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    utf8* slhs;
    utf8* srhs;
    long long lhs,rhs;
    utf8* t;
    utf8* f;
    utf8* value;

    TTMFCN_BEGIN(ttm,frame,result);
    slhs = frame->argv[1];
    srhs = frame->argv[2];
    t = frame->argv[3];
    f = frame->argv[4];

    if(1 != sscanf((const char*)slhs,"%lld",&lhs)) {err = TTM_EDECIMAL; goto done;}
    if(1 != sscanf((const char*)srhs,"%lld",&rhs)) {err = TTM_EDECIMAL; goto done;}
    value = (lhs > rhs ? t : f);
    vsclear(result);
    vsappendn(result,(const char*)value,strlen((const char*)value));
done:
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_lt(TTM* ttm, Frame* frame, VString* result) /* Compare numeric less-than */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    utf8* slhs;
    utf8* srhs;
    long long lhs,rhs;
    utf8* t;
    utf8* f;
    utf8* value;

    TTMFCN_BEGIN(ttm,frame,result);
    slhs = frame->argv[1];
    srhs = frame->argv[2];
    t = frame->argv[3];
    f = frame->argv[4];

    if(1 != sscanf((const char*)slhs,"%lld",&lhs)) {err = TTM_EDECIMAL; goto done;}
    if(1 != sscanf((const char*)srhs,"%lld",&rhs)) {err = TTM_EDECIMAL; goto done;}
    value = (lhs < rhs ? t : f);
    vsclear(result);
    vsappendn(result,(const char*)value,strlen((const char*)value));
done:
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_ge(TTM* ttm, Frame* frame, VString* result) /* Compare numeric greater-than or equal */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    utf8* slhs;
    utf8* srhs;
    long long lhs,rhs;
    utf8* t;
    utf8* f;
    utf8* value;

    TTMFCN_BEGIN(ttm,frame,result);
    slhs = frame->argv[1];
    srhs = frame->argv[2];
    t = frame->argv[3];
    f = frame->argv[4];

    if(1 != sscanf((const char*)slhs,"%lld",&lhs)) {err = TTM_EDECIMAL; goto done;}
    if(1 != sscanf((const char*)srhs,"%lld",&rhs)) {err = TTM_EDECIMAL; goto done;}
    value = (lhs >= rhs ? t : f);
    vsclear(result);
    vsappendn(result,(const char*)value,strlen((const char*)value));
done:
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_le(TTM* ttm, Frame* frame, VString* result) /* Compare numeric less-than or equal */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    utf8* slhs;
    utf8* srhs;
    long long lhs,rhs;
    utf8* t;
    utf8* f;
    utf8* value;

    TTMFCN_BEGIN(ttm,frame,result);
    slhs = frame->argv[1];
    srhs = frame->argv[2];
    t = frame->argv[3];
    f = frame->argv[4];

    if(1 != sscanf((const char*)slhs,"%lld",&lhs)) {err = TTM_EDECIMAL; goto done;}
    if(1 != sscanf((const char*)srhs,"%lld",&rhs)) {err = TTM_EDECIMAL; goto done;}
    value = (lhs <= rhs ? t : f);
    vsclear(result);
    vsappendn(result,(const char*)value,strlen((const char*)value));
done:
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_eql(TTM* ttm, Frame* frame, VString* result) /* ? Compare logical equal */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    utf8* slhs;
    utf8* srhs;
    utf8* t;
    utf8* f;
    utf8* value;

    TTMFCN_BEGIN(ttm,frame,result);
    slhs = frame->argv[1];
    srhs = frame->argv[2];
    t = frame->argv[3];
    f = frame->argv[4];

    value = (strcmp((const char*)slhs,(const char*)srhs) == 0 ? t : f);
    vsclear(result);
    vsappendn(result,(const char*)value,strlen((const char*)value));
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_gtl(TTM* ttm, Frame* frame, VString* result) /* ? Compare logical greater-than */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    utf8* slhs;
    utf8* srhs;
    utf8* t;
    utf8* f;
    utf8* value;

    TTMFCN_BEGIN(ttm,frame,result);
    slhs = frame->argv[1];
    srhs = frame->argv[2];
    t = frame->argv[3];
    f = frame->argv[4];

    value = (strcmp((const char*)slhs,(const char*)srhs) > 0 ? t : f);
    vsclear(result);
    vsappendn(result,(const char*)value,strlen((const char*)value));
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_ltl(TTM* ttm, Frame* frame, VString* result) /* ? Compare logical less-than */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    utf8* slhs;
    utf8* srhs;
    utf8* t;
    utf8* f;
    utf8* value;

    TTMFCN_BEGIN(ttm,frame,result);
    slhs = frame->argv[1];
    srhs = frame->argv[2];
    t = frame->argv[3];
    f = frame->argv[4];

    value = (strcmp((const char*)slhs,(const char*)srhs) < 0 ? t : f);
    vsclear(result);
    vsappendn(result,(const char*)value,strlen((const char*)value));
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

/* Peripheral Input/Output Operations */

/* Helper functions */
static TTMERR
selectfile(TTM* ttm, const utf8* fname, IOMODE required_modes, TTMFILE** targetp)
{
    TTMERR err = TTM_NOERR;
    TTMFILE* target = NULL;
    if(strcmp("stdin",(const char*)fname)==0) {
	target = ttm->io.stdin;
    } else if(strcmp("stdout",(const char*)fname)==0) {
	target = ttm->io.stdout;
    } else if(strcmp("stderr",(const char*)fname)==0) {
	target = ttm->io.stderr;
    } else if(strcmp("-",(const char*)fname)==0) {
	target = ttm->io.stdin; 
    } else
    	{err = TTM_EACCESS; goto done;}
    /* Check the modes */
    if((target->mode & required_modes) != required_modes)
	{err = TTM_EACCESS; goto done;}
    if(targetp) *targetp = target;
done:
    return THROW(err);
}

/**
Common code for ttm_ps and ttm_psr.
In order to a void spoofing, the string to be output is modified
to convert all control characters except '\n' and '\r' ('\t').
Also, segment and create marks, are converted to a printable form.
It also takes an optional file argument.
*/
static TTMERR
ttm_ps0(TTM* ttm, TTMFILE* target, int argc, utf8** argv, VString* result) /* Print a Function/String */
{
    TTMERR err = TTM_NOERR;
    int i;
    
    for(i=0;i<argc;i++) {
	utf8* cleaned = printclean(argv[i],"\n\r\t",NULL);
	printstring(ttm,cleaned,target);
	nullfree(cleaned);
    }
#ifdef GDB
    ttmflush(ttm,target);
#endif
    return THROW(err);
}

/**
In order to a void spoofing, the string to be output is modified
to convert all control characters except '\n' and '\r' ('\t').
Also, segment and create marks, are converted to a printable form.
It also takes an optional file argument.
*/
static TTMERR
ttm_ps(TTM* ttm, Frame* frame, VString* result) /* Print a Function/String */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    TTMFILE* target;

    TTMFCN_BEGIN(ttm,frame,result);
    target = ttm->io.stdout; /* choose stdout as default target */
    if((err = ttm_ps0(ttm,target,1,frame->argv+1,result))) goto done;
done:
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_rs0(TTM* ttm, TTMFILE* target, VString* result) /* Read a String from optional file name */
{
    TTMERR err = TTM_NOERR;
    int len;

    for(len=0;;len++) {
	int ncp;
	utf8cpa cp8;
	ncp = ttmnonl(ttm,target,cp8);
	if(isnul(cp8)) break;
	if(u8equal(cp8,ttm->meta.metac)) break;
	vsappendn(result,(const char*)cp8,ncp);
    }
    return THROW(err);
}

/**
Common code to ttm_rs and ttm_psr.
*/
static TTMERR
ttm_rs(TTM* ttm, Frame* frame, VString* result) /* Read a Function/String */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    TTMFILE* target;

    TTMFCN_BEGIN(ttm,frame,result);
    if(frame->argc > 1) {
	if((err = selectfile(ttm,frame->argv[1],IOM_READ,&target))!=TTM_NOERR) {
	    target = ttm->io.stdin; /* choose stdin as default target */
	}
    } else
        target = ttm->io.stdin; /* default */
    if((err=ttm_rs0(ttm,target,result))) goto done;
done:
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

/* This only makes sense when using /dev/tty; but use stdin+stdout if necessary */
static TTMERR
ttm_psr(TTM* ttm, Frame* frame, VString* result) /* Print a string and then read from input */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    TTMFILE* reader = NULL;
    TTMFILE* writer = NULL;

    TTMFCN_BEGIN(ttm,frame,result);
    reader = ttm->io.stdin;
    writer = ttm->io.stdout;
    ttm_ps0(ttm,writer,frame->argc-1,frame->argv+1,result);
    ttmflush(ttm,ttm->io.stdout);
    ttm_rs0(ttm,reader,result);
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_cm(TTM* ttm, Frame* frame, VString* result) /* Change meta character */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    utf8* smeta = NULL;

    TTMFCN_BEGIN(ttm,frame,result);
    smeta = frame->argv[1];
    if(strlen((const char*)smeta) > 0) {
	if(u8size(smeta) > 1) {err = TTM_EASCII; goto done;}
	memcpycp(ttm->meta.metac,smeta);
    }
done:
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

/* printf helper functions */

static TTMERR
parsespec(const char* fmt, int* leadzerop, int* lcountp, char* typp, int* ispcentp, size_t* speclenp)
{
    TTMERR err = TTM_NOERR;
    const char* p = fmt;
    int leadzero = 0;
    int lcount = 0;
    char typ = 'd';
    int ispcent = 0;

    p++; /* skip leading '%' */
    if(isnul(p)) {err = TTM_EIO; goto done;}
    if(*p == '%') {p++; ispcent = 1; goto havevalue;}
    if(*p == '0') {leadzero = 1; p++;}
    if(isnul(p)) {err = TTM_EIO; goto done;}	    
    if(*p == 'l') {lcount++; p++;}
    if(isnul(p)) {err = TTM_EIO; goto done;}
    if(*p == 'l') {lcount++; p++;}
    if(isnul(p)) {err = TTM_EIO; goto done;}
    switch (*p) {
    case 'd': case 'u': case 'o': case 'x': case 's': typ = *p; p++; break;
    default: break;
    }
havevalue:
    if(ispcentp) *ispcentp = ispcent;
    if(leadzerop) *leadzerop = leadzero;
    if(lcountp) *lcountp = lcount;
    if(typp) *typp = typ;
    if(speclenp) *speclenp = (size_t)(p - fmt);
done:
    return THROW(err);
}

static void
buildspec(int leadzero, int lcount, char typ, char* spec)
{
    char* q = spec;
    *q++ = '%';
    if(leadzero) *q++ = '0';
    if(lcount > 0) *q++ = 'l';
    if(lcount > 1) *q++ = 'l';
    *q++ = typ;
    *q = '\0';
}

static void
buildscanfmt(char typ, char* spec)
{
    char fmt[8] = "%ll_\0";
    fmt[3] = typ;
    memcpy(spec,fmt,strlen(fmt));
}

static TTMERR
intconvert(char* arg, int leadzero, int lcount, char typ, unsigned long long * llup)
{
    TTMERR err = TTM_NOERR;
    char* legal = NULL;
    int issigned = 1;
    int count;
    unsigned long long llu;
    char* p = NULL;
    char scanfmt[8];

    if(arg == NULL || arg[0] == '\0') {err = TTM_EINVAL; goto done;}
    switch (typ) {
    case 'd': issigned = 1; legal = "0123456789"; break;
    case 'u': issigned = 0; legal = "0123456789"; break;
    case 'o': issigned = 1; legal = "01234567"; break;
    case 'x': issigned = 0; legal = "0123456789abcdefABCDEF"; break;
    default: goto done; /* not an int type */
    }
    p = arg;

    if(issigned && (*p == '+' || *p == '-')) p++;

    /* Verify digits */
    while(*p) {
	if(strchr(legal,*p++) == NULL) {err = TTM_EINVAL; goto done;}
    }

    /* Convert */
    buildscanfmt(typ,scanfmt);
    switch (typ) {
    case 'd': count = sscanf(arg,scanfmt,(signed long long*)&llu);
    case 'u': count = sscanf(arg,scanfmt,(unsigned long long*)&llu);
    case 'o': count = sscanf(arg,scanfmt,(signed long long*)&llu);
    case 'x': count = sscanf(arg,scanfmt,(unsigned long long*)&llu);
    }
    if(count != 1) {err = TTM_EDECIMAL; goto done;}
    if(llup) *llup = llu;
done:
    return THROW(err);
}

static void
strcvt(int leadzero, int lcount, char typ, unsigned long long llu, char* svalue, size_t svallen)
{
    char spec[64]; /* big enough to hold any legal spec */
    buildspec(leadzero,lcount,typ,spec);
    /* Print according to spec */
    switch (typ) {
    case 'd':   switch (lcount) {
		case 0: snprintf(svalue,svallen,spec,(int)((long long)llu)); break;
		case 1: snprintf(svalue,svallen,spec,(long)((long long)llu)); break;
		case 2: snprintf(svalue,svallen,spec,((long long)llu)); break;
		}
    case 'u': case 'o': case 'x':
	switch (lcount) {
	case 0: snprintf(svalue,svallen,spec,(unsigned)(llu)); break;
	case 1: snprintf(svalue,svallen,spec,(unsigned long)(llu)); break;
	case 2: snprintf(svalue,svallen,spec,(llu)); break;
	}
    }
}

/**
Common code for fprintf and printf.
@param ttm
@param target on which to write
@param fmt
@param argc
@param argv
*/
static TTMERR
ttm_vprintf(TTM* ttm, TTMFILE* target, const utf8* fmt8, size_t argc, utf8** argv, VString* result) /* Print a Function/String */
{
    TTMERR err = TTM_NOERR;
    const char* fmt = (const char*)fmt8;
    size_t argi;
    char* p = NULL;
    int leadzero,lcount,ispcent;
    char typ;
    int stop;
    size_t speclen;
	    
    argi=0;
    p = (char*)fmt;
    /* Walk the fmt and parse each spec in turn */
    do {
        int ncp = u8size((const utf8 *)p);
	stop = 0;
	if(isnul(p)) {stop = 1;} /* EOS => stop */
	else if(*p == '%') { /* collect the %... spec */
	    leadzero = 0; lcount = 0; typ = '\0'; ispcent = 0; /* re-initialize */
	    if((err = parsespec(p,&leadzero,&lcount,&typ,&ispcent,&speclen))) goto done;
	    if(ispcent) {  /* special case for "%%" */
	        vsappend(result,'%');
		p += 2;
		continue;
	    }
	    if(argi >= argc) {err = TTM_EFEWPARMS; goto done;}
	    /* check the type */
	    if(typ == 's') {/* Handle string separately */
		vsappendn(result,(char*)argv[argi++],0);
	    } else { /* Integer type */
		unsigned long long llu = 0;
		char svalue[256]; /* big enough to hold single integer value string */
		if((err = intconvert((char*)argv[argi],leadzero,lcount,typ,&llu))) goto done;
		strcvt(leadzero,lcount,typ,llu,svalue,sizeof(svalue));
		vsappendn(result,svalue,0);
	    }
	    p += speclen; /* skip fmt in input string */
	} else {/* pass the char */
	    vsappendn(result,p,ncp);
	    p += ncp;
	}
    } while(!stop);
    /* print the result string (see ttm_ps) */
    printstring(ttm,(utf8*)vscontents(result),target);
#ifdef GDB
    ttmflush(ttm,target);
#endif
done:
    return THROW(err);
}

/**
Emulate constrained **nix fprintf function with:
- target file as argv[1]: legal values are "stdout"|"stderr"|("-" | "") [to signal ttm->io.output]
- format as argv[2].
    -- format tags are "%[0]?[l|ll]?[duoxs]"
In order to a void spoofing, the string to be output is modified
to convert all control characters except '\n' and '\r' and '\t'.
Also, segment and create marks, are converted to a printable form.
*/
static TTMERR
ttm_fprintf(TTM* ttm, Frame* frame, VString* result) /* Print a Function/String */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    TTMFILE* target = NULL;
    utf8* fmt = NULL;
    const utf8* fname = NULL;
    
    TTMFCN_BEGIN(ttm,frame,result);
    if(frame->argc < 3) {err = TTM_EFEWPARMS; goto done;}
    /* Figure out the target file */
    fname = frame->argv[1];
    if((err = selectfile(ttm,fname,IOM_WRITE,&target))!=TTM_NOERR) {
	target = ttm->io.stdout; /* default */
    }

    /* Get the fmt */
    fmt = frame->argv[2];

    err = ttm_vprintf(ttm,target,fmt,frame->argc-3,frame->argv+3,result);

done:
    return THROW(err);
}

/**
Emulate constrained **nix printf function with:
- format as argv[1].
    -- format tags are "%[0]?[l|ll]?[duoxs]"
In order to a void spoofing, the string to be output is modified
to convert all control characters except '\n' and '\r' and '\t'.
Also, segment and create marks, are converted to a printable form.
*/
static TTMERR
ttm_printf(TTM* ttm, Frame* frame, VString* result) /* Print a Function/String */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    TTMFILE* target = NULL;
    utf8* fmt = NULL;
    
    TTMFCN_BEGIN(ttm,frame,result);
    if(frame->argc < 2) {err = TTM_EFEWPARMS; goto done;}
    /* Printf always writes to stdout */
    target = ttm->io.stdout; /* default */

    /* Get the fmt */
    fmt = frame->argv[2];

    err = ttm_vprintf(ttm,target,fmt,frame->argc-3,frame->argv+3,result);

done:
    return THROW(err);
}

static TTMERR
ttm_pf(TTM* ttm, Frame* frame, VString* result) /* Flush stdout and/or stderr */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    utf8* stdxx = NULL;

    TTMFCN_BEGIN(ttm,frame,result);
    stdxx = (frame->argc == 1 ? NULL : frame->argv[1]);
    if(stdxx == NULL || strcmp((const char*)stdxx,"stdout")==0)
	ttmflush(ttm,ttm->io.stderr);
    if(stdxx == NULL || strcmp((const char*)stdxx,"stderr")==0)
	ttmflush(ttm,ttm->io.stderr);
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_tru(TTM* ttm, Frame* frame, VString* result) /* translate to upper case */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    utf8* s;

    TTMFCN_BEGIN(ttm,frame,result);
    vsclear(result);
    s = frame->argv[1];
    {
#ifdef MSWINDOWS
#else
	size_t i,swlen,s8len;
	wchar_t* sw = NULL;

	/* Determine the size needed for the wide character string */
	if((swlen = (mbstowcs(NULL, (char*)s, 0) + 1))==((size_t)-1)) {err = TTM_EUTF8; goto done;}
	/* Allocate memory for the wide character string */
	if((sw = (wchar_t*)malloc(swlen * sizeof(wchar_t)))==NULL) {err = TTM_EMEMORY; goto done;}
	/* convert utf8 string to wide char (utf16 or utf32) */
	(void)mbstowcs(sw,(char*)s, swlen);
	/* Convert to lower case */
	for(i=0;i<swlen;i++)
	    sw[i] = (wchar_t)towupper(sw[i]);
	/* Convert back to utf8 */
	/* Get space required */
	if((s8len = (wcstombs(NULL, sw, 0) + 1))==((size_t)-1)) {err = TTM_EUTF8; goto done;}
	/* Allocate memory for the utf8 character string */
	vssetalloc(result,s8len);
	/* Insert into the result buffer */
	(void)wcstombs(vscontents(result),sw,swlen);
	/* Make length consistent */
	vssetlength(result,s8len-1);
#endif
    }
done:
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_tdh(TTM* ttm, Frame* frame, VString* result) /* convert hex to decimal */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    char hex[8+1];
    char* p = NULL;
    char* q = NULL;
    long long ln;
    char dec[32];

    TTMFCN_BEGIN(ttm,frame,result);
    p = (char*)frame->argv[1];
    q = dec;
    if(*p == '+') p++;
    else if(*p == '-') *q++ = *p++;
    while(*p) {
	if(!isdec(*p)) {err = TTM_EINVAL; goto done;}        
	*q++ = *p++;
    }
    *q = '\0';
    if(1 != sscanf(dec,"%lld",&ln)) {err = TTM_EINVAL; goto done;}
    snprintf(hex,sizeof(hex),"%llx",(unsigned long long)ln);
    vsappendn(result,hex,0);
done:
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_rp(TTM* ttm, Frame* frame, VString* result) /* return residual pointer*/
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    Function* str;
    utf8* s;
    size_t rp = 0;
    char srp[128];
    
    TTMFCN_BEGIN(ttm,frame,result);
    if((str = getdictstr(ttm,frame,1))==NULL) {err = FAILNONAME(1); goto done;}
    s = (utf8*)vscontents(str->fcn.body);
    switch(err = strsubcp(s,vsindex(str->fcn.body),&rp)) {
    case TTM_NOERR: case TTM_EEOS: break;
    default: goto done;
    }
    snprintf(srp,sizeof(srp),"%lld",(long long)rp);
    vsappendn(result,srp,0);

done:
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static int
stringveccmp(const void* a, const void* b)
{
    const utf8** sa = (const utf8**)a;
    const utf8** sb = (const utf8**)b;
    return strcmp((const char*)(*sa),(const char*)(*sb));
}

static int
fcncmp(const void* a, const void* b)
{
    const struct Function** fa = (const struct Function**)a;
    const struct Function** fb = (const struct Function**)b;
    return strcmp((const char*)(*fa)->entry.name,(const char*)(*fb)->entry.name);
}

static TTMERR
ttm_names(TTM* ttm, Frame* frame, VString* result) /* Obtain all dictionary instance names in sorted order */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    size_t i;
    VList* nameset = vlnew();
    const char* arg = NULL;
    int klass = 0;
#	define TTM_NAMES_ALL	  1
#	define TTM_NAMES_STRINGS  2
#	define TTM_NAMES_BUILTIN  3
#	define TTM_NAMES_SPECIFIC 4

    TTMFCN_BEGIN(ttm,frame,result);
    /* Collect the class of names to be returned:
       * #<names> -- all names
       * #<names;strings> -- all ##<ds;string> names
       * #<names;builtin> -- all builtin names
       * #<names;;...> -- specific names
    */
    i = frame->argc;
    switch (i) {
    case 0: err = TTM_ETTM; goto done; /* should never happen */
    case 1: klass = TTM_NAMES_ALL; break;
    case 2:
	arg = (const char*)frame->argv[i];
	if(strcmp(arg,"strings")==0)
	    klass = TTM_NAMES_STRINGS;
	else if(strcmp(arg,"builtin")==0)
	    klass = TTM_NAMES_BUILTIN;
	else if(arg == NULL || strcmp(arg,"")==0)
	    klass = TTM_NAMES_SPECIFIC;
	break;
    default: klass = TTM_NAMES_SPECIFIC; break;
    }

    if(klass == 0) {err = TTM_EFEWPARMS; goto done;}

    /* Collect all the relevant Functions. */
    for(i=0;i<HASHSIZE;i++) {
	struct Function* fcn = (struct Function*)ttm->tables.dictionary.table[i].next;
	while(fcn != NULL) {
	    if(klass == TTM_NAMES_ALL
	       || (klass == TTM_NAMES_STRINGS && !fcn->fcn.builtin)
	       || (klass == TTM_NAMES_BUILTIN && fcn->fcn.builtin)) {
		vlpush(nameset,fcn);
	    } else if(klass == TTM_NAMES_SPECIFIC) {
		size_t j;
		for(j=2;j<frame->argc;j++) {
		    if(strcmp((const char*)frame->argv[j],(const char*)fcn->entry.name)==0)
			{vlpush(nameset,fcn); break;}
		}
	    }
	    fcn = (struct Function*)fcn->entry.next;
	}
    }
    /* Quick sort the list */
    {
        void* content = vlcontents(nameset);
	qsort(content, vllength(nameset), sizeof(struct Function*), fcncmp);
    }
    /* print names comma separated */
    for(i=0;i<vllength(nameset);i++) {
	const utf8* nm = ((const struct HashEntry*)vlget(nameset,i))->name;
	if(i > 0) vsappend(result,',');
	vsappendn(result,(const char*)nm,0);
    }
done:
    vlfree(nameset);
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_exit(TTM* ttm, Frame* frame, VString* result) /* Return from TTM */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    long long exitcode = 0;
    TTMFCN_BEGIN(ttm,frame,result);
    ttm->flags.exit = 1;
    if(frame->argc > 1) {
	if(1!=sscanf((const char*)frame->argv[1],"%lld",&exitcode)) {err = TTM_EDECIMAL; goto done;}
	if(exitcode < 0) exitcode = - exitcode;
    }
    ttm->flags.exitcode = (int)exitcode;
    /* Clear any ttm->vs.active contents */
    vsclear(ttm->vs.active);
done:
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

/* Utility Operations */

static TTMERR
ttm_ndf(TTM* ttm, Frame* frame, VString* result) /* Determine if a name is defined */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    Function* str;
    utf8* t;
    utf8* f;
    utf8* value;

    TTMFCN_BEGIN(ttm,frame,result);
    str = dictionaryLookup(ttm,frame->argv[1]);
    t = frame->argv[2];
    f = frame->argv[3];
    value = (str == NULL ? f : t);
    vsappendn(result,(const char*)value,strlen((const char*)value));
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_norm(TTM* ttm, Frame* frame, VString* result) /* Obtain the Norm of a string in units of codepoints */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    utf8* s;
    char value[32];
    size_t count;

    TTMFCN_BEGIN(ttm,frame,result);
    s = frame->argv[1];
    switch (err = strsubcp(s,strlen((const char*)s),&count)) {
    case TTM_NOERR: case TTM_EEOS: break;
    default: goto done;
    }
    snprintf(value,sizeof(value),"%zu",count);
    vsappendn(result,value,strlen(value));
done:
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_time(TTM* ttm, Frame* frame, VString* result) /* Obtain time of day */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    char value[MAXINTCHARS+1];
    struct timeval tv;
    long long time;

    TTMFCN_BEGIN(ttm,frame,result);
    if(timeofday(&tv) < 0)
	{err = TTM_ETIME; goto done;}
    time = (long long)tv.tv_sec;
    time *= 1000000; /* convert to microseconds */
    time += tv.tv_usec;
    time = time / 10000; /* Need time in 100th second */
    snprintf(value,sizeof(value),"%lld",time);
    vsappendn(result,value,strlen(value));
done:
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_xtime(TTM* ttm, Frame* frame, VString* result) /* Obtain Execution Time */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    long long time;
    char value[MAXINTCHARS+1];

    TTMFCN_BEGIN(ttm,frame,result);
    time = getRunTime();
    snprintf(value,sizeof(value),"%lld",time);
    vsappendn(result,value,strlen(value));
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_ctime(TTM* ttm, Frame* frame, VString* result) /* Convert ##<time> to printable string */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    utf8* stod;
    long long tod;
    char value[1024];
    time_t ttod;
    int i;

    TTMFCN_BEGIN(ttm,frame,result);
    stod = frame->argv[1];
    if(1 != sscanf((const char*)stod,"%lld",&tod)) {err = TTM_EDECIMAL; goto done;}
    tod = tod/100; /* need seconds */
    ttod = (time_t)tod;
    snprintf(value,sizeof(value),"%s",ctime(&ttod));
    /* ctime adds a trailing new line; remove it */
    i = strlen(value);
    for(i--;i >= 0;i--) {
	if(value[i] != '\n' && value[i] != '\r') break;
    }
    value[i+1] = NUL8;
    vsappendn(result,value,strlen(value));
done:
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_tf(TTM* ttm, Frame* frame, VString* result) /* Turn Trace Off */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    size_t i;

    TTMFCN_BEGIN(ttm,frame,result);
    /* |argc| > 1 => trace off on specific names; |argc|==1 => turn off global trace */
    if(frame->argc > 1) {
	for(i=1;i<frame->argc;i++) {
	    Function* fcn = dictionaryLookup(ttm,frame->argv[i]);
	    if(fcn != NULL) fcn->fcn.trace = 0;
	}
    } else { /* turn off global tracing */
	ttm->debug.trace = 0;
    }
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_tn(TTM* ttm, Frame* frame, VString* result) /* Turn Trace On */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    size_t i;

    TTMFCN_BEGIN(ttm,frame,result);
    /* |argc| > 1 => trace on specific names; |argc|==1 => turn on global trace */
    if(frame->argc > 1) {
	for(i=1;i<frame->argc;i++) {
	    Function* fcn = dictionaryLookup(ttm,frame->argv[i]);
	    if(fcn != NULL) fcn->fcn.trace = 1;
	}
    } else { /* turn on global tracing but possibly delayed*/
	ttm->debug.trace = 1;
    }
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

/**************************************************/
/* Functions new to this implementation */

/* Get ith command line argument; zero is command */
static TTMERR
ttm_argv(TTM* ttm, Frame* frame, VString* result)
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    long long index = 0;
    int arglen;
    char* arg;

    TTMFCN_BEGIN(ttm,frame,result);
    if((1 != sscanf((const char*)frame->argv[1],"%lld",&index))) {err = TTM_EDECIMAL; goto done;}
    if(index < 0 || ((size_t)index) >= vllength(argoptions)){err = TTM_ERANGE; goto done;}
    arg = vlget(argoptions,(size_t)index);
    arglen = strlen(arg);
    vsappendn(result,arg,arglen);
done:
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

/* Get the length of argoptions */
static TTMERR
ttm_argc(TTM* ttm, Frame* frame, VString* result)
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    char value[MAXINTCHARS+1];
    int argc;

    TTMFCN_BEGIN(ttm,frame,result);
    argc = vllength(argoptions);
    snprintf(value,sizeof(value),"%d",argc);
    vsappendn(result,value,strlen(value));
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_classes(TTM* ttm, Frame* frame, VString* result) /* Obtain all character class names */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    int i,nclasses,index;
    utf8 const** classes;
    /* Note, we ignore any "programclasses" argument */
#if 0
    int allclasses = (frame->argc > 1 ? 1 : 0);
#endif

    TTMFCN_BEGIN(ttm,frame,result);
    /* First, figure out the number of classes and the total size */
    for(nclasses=0,i=0;i<HASHSIZE;i++) {
	struct HashEntry* entry = ttm->tables.charclasses.table[i].next;
	while(entry != NULL) {
	    nclasses++;
	    entry = entry->next;
	}
    }
    if(nclasses == 0) goto done;

    /* Now collect all the classes */
    /* Note the reason we collect the classes is because we need to sort them */
    classes = (utf8 const**)calloc(sizeof(utf8*),nclasses);
    if(classes == NULL) {err = TTM_EMEMORY; goto done;}
    index = 0;
    for(i=0;i<HASHSIZE;i++) {
	struct HashEntry* entry = ttm->tables.charclasses.table[i].next;
	while(entry != NULL) {
	    Charclass* class = (Charclass*)entry;
	    classes[index++] = (const utf8*)class->entry.name;
	    entry = entry->next;
	}
    }
    if(index != nclasses) {err = TTM_ETTM; goto done;} /* Something bad happened */
    /* Quick sort */
    qsort((void*)classes, nclasses, sizeof(utf8*),stringveccmp);

    /* print classes comma separated */
    for(i=0;i<nclasses;i++) {
	if(i > 0) vsappend(result,',');
	vsappendn(result,(const char*)classes[i],strlen((const char*)classes[i]));
    }
    if(nclasses > 0) free(classes); /* but do not free the string contents */
done:
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_lf(TTM* ttm, Frame* frame, VString* result) /* Lock a list of function from being deleted; null =>lock all */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    size_t i;
    Function* fcn;

    TTMFCN_BEGIN(ttm,frame,result);
    if(frame->argc > 1) {/*lock specific names; a single null name => lock all functions */
	if(frame->argc == 2 && strlen((const char*)frame->argv[1])==0) {
	    for(i=0;i<HASHSIZE;i++) {
		struct HashEntry* entry = ttm->tables.dictionary.table[i].next;
		while(entry != NULL) {
		    fcn = (Function*)entry;
		    if(!fcn->fcn.builtin) fcn->fcn.locked = 1;
		    entry = entry->next;
		}
	    }
	} else for(i=1;i<frame->argc;i++) {
	    fcn = dictionaryLookup(ttm,frame->argv[i]);
	    if(fcn != NULL && !fcn->fcn.builtin) fcn->fcn.trace = 1;
	}
    }
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_uf(TTM* ttm, Frame* frame, VString* result) /* Un-Lock a function from being deleted */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    size_t i;
    Function* fcn;

    TTMFCN_BEGIN(ttm,frame,result);
    if(frame->argc > 1) {/*lock specific names; a single null name => lock all functions */
	if(frame->argc == 2 && strlen((const char*)frame->argv[1])==0) {
	    for(i=0;i<HASHSIZE;i++) {
		struct HashEntry* entry = ttm->tables.dictionary.table[i].next;
		while(entry != NULL) {
		    fcn = (Function*)entry;
		    if(!fcn->fcn.builtin) fcn->fcn.locked = 0;
		    entry = entry->next;
		}
	    }
	} else for(i=1;i<frame->argc;i++) {
	    fcn = dictionaryLookup(ttm,frame->argv[i]);
	    if(fcn != NULL && !fcn->fcn.builtin) fcn->fcn.trace = 0;
	}
    }
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_include(TTM* ttm, Frame* frame, VString* result)  /* Include text of a file */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    const char* path;

    TTMFCN_BEGIN(ttm,frame,result);
    path = (const char*)frame->argv[1];
    if(path == NULL || strlen(path) == 0) {err = TTM_EINCLUDE; goto done;}
    readfile(ttm,path,ttm->vs.tmp);
    vsappendn(result,vscontents(ttm->vs.tmp),vslength(ttm->vs.tmp));
    vsclear(ttm->vs.tmp);
done:
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_void(TTM* ttm, Frame* frame, VString* result) /* Throw away all arguments */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);

    TTMFCN_BEGIN(ttm,frame,result);
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

/** Properties functions */
static TTMERR
ttm_setprop(TTM* ttm, Frame* frame, VString* result) /* Set specified property */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    utf8* key = NULL;
    utf8* value = NULL;

    TTMFCN_BEGIN(ttm,frame,result);
    if(frame->argc < 3) {err = TTM_EFEWPARMS; goto done;}
    key = frame->argv[1];
    value = frame->argv[2];
    setproperty((const char*)key,(const char*)value,&ttm->properties);
done:
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_resetprop(TTM* ttm, Frame* frame, VString* result) /* Set specified property back to default */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    utf8* key = NULL;

    TTMFCN_BEGIN(ttm,frame,result);
    if(frame->argc < 2) {err = TTM_EFEWPARMS; goto done;}
    key = frame->argv[1];
    resetproperty(&ttm->properties,(const char*)key,&dfalt_properties);
done:
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

/**
Helper functions for all the ttm commands
and subcommands
*/

/**
#<ttm;meta;which;char>
where which is one of: "sharp","open","close","semi","escape","meta",
and char is the replacement character for that TTM-significant value.
Note that "meta" is for completeness since the cm command does
the same thing.
*/
static TTMERR
ttm_ttm_meta(TTM* ttm, Frame* frame, VString* result)
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    utf8* which = NULL;
    utf8* replacement = NULL;

    TTMFCN_BEGIN(ttm,frame,result);
    if(frame->argc < 3) {err = TTM_EFEWPARMS; goto done;}
    which = frame->argv[1];
    replacement = frame->argv[2];

    if(u8size(replacement) > 1) {err = TTM_EASCII; goto done;}
    switch (metaenumdetect(which)) {
    case ME_SHARP: memcpycp(ttm->meta.sharpc,replacement); break;
    case ME_SEMI: memcpycp(ttm->meta.semic,replacement); break;
    case ME_ESCAPE: memcpycp(ttm->meta.escapec,replacement); break;
    case ME_META: memcpycp(ttm->meta.metac,replacement); break;
    case ME_OPEN: memcpycp(ttm->meta.openc,replacement); break;
    case ME_CLOSE: memcpycp(ttm->meta.closec,replacement); break;
    case ME_LBR: memcpycp(ttm->meta.lbrc,replacement); break;
    case ME_RBR: memcpycp(ttm->meta.rbrc,replacement); break;
    default: err = TTM_ETTM; goto done;
    }
done:
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

/**
#<ttm;info;name;{name}>
Return info about name from the dictionary.
*/
static TTMERR
ttm_ttm_info_name(TTM* ttm, Frame* frame, VString* result)
{
    TTMERR err = TTM_NOERR;
    Function* str;
    char info[8192];
    utf8* arg = NULL;
    size_t arglen = 0;
    utf8* cleaned = NULL;
    size_t ncleaned = 0;

    if(frame->argc < 4) {err = TTM_EFEWPARMS; goto done;}
    arg = frame->argv[3];
    arglen = strlen((const char*)arg);
    str = dictionaryLookup(ttm,arg);
    if(str == NULL) { /* not defined*/
	vsappendn(result,(const char*)arg,arglen);
	vsappendn(result,"|,-,-,-|",0);
	goto done;
    }
    /* assert(str != NULL) */
    vsappendn(result,(const char*)str->entry.name,strlen((const char*)str->entry.name));
    if(str->fcn.builtin) {
	char value[64];
	snprintf(value,sizeof(value),"%zu",str->fcn.maxargs);
	snprintf(info,sizeof(info),",%zu,%s,%s",
		 str->fcn.minargs,(str->fcn.maxargs == ARB?"*":value),sv(str));
	vsappendn(result,info,strlen(info));
    } else {/*!str->fcn.builtin*/
	int nargs = str->fcn.nextsegindex-1;
	if(nargs < 0) nargs = 0;
	snprintf(info,sizeof(info),",0,%d,V",nargs);
	vsappendn(result,info,strlen(info));
    }
    if(!str->fcn.builtin && ttm->debug.verbose) {
	snprintf(info,sizeof(info)," residual=%zu body=|",vsindex(str->fcn.body));
	vsappendn(result,info,strlen(info));
	cleaned = printclean((utf8*)vscontents(str->fcn.body),NULL,&ncleaned);
	vsappendn(result,(const char*)cleaned,ncleaned);
	vsappend(result,'|');
    }
done:
    nullfree(cleaned);
    return THROW(err);
}

/**
#<ttm;info;class;{classname}...>
*/
static TTMERR
ttm_ttm_info_class(TTM* ttm, Frame* frame, VString* result) /* Misc. combined actions */
{
    TTMERR err = TTM_NOERR;
    Charclass* cl;
    utf8* p;
    size_t i,len;

    for(i=3;i<frame->argc;i++) {
	cl = charclassLookup(ttm,frame->argv[i]);
	if(cl == NULL) FAILNOCLASS(i);
	len = strlen((const char*)cl->entry.name);
	vsappendn(result,(const char*)cl->entry.name,len);
	vsappend(result,' ');
	vsappend(result,LBRACKET);
	if(cl->negative) vsappend(result,'^');
	p=cl->characters;
	for(;;) {
	    int ncp = u8size(p);
	    if(ncp == 1 && (*p == LBRACKET || *p == RBRACKET))
		vsappend(result,'\\');
	    p += ncp;
	}
	vsappend(result,'\n');
    }
#if DEBUG > 0
    xprintf(ttm,"info.class: |%s|\n",vscontents(result));
#endif
    return THROW(err);
}

/**
#<ttm;info;string;{name}>
if name is builtin then use value of "" else return the string
value (including any seg/create marks). The result is
bracketed (<...>).
*/
static TTMERR
ttm_ttm_info_string(TTM* ttm, Frame* frame, VString* result) /* Misc. combined actions */
{
    TTMERR err = TTM_NOERR;
    Function* str;
    utf8* arg = NULL;

    if(frame->argc < 4) {err = TTM_EFEWPARMS; goto done;}
    arg = frame->argv[3];
    str = dictionaryLookup(ttm,arg);
    /* Surround the body with <...> */
    vsappendn(result,(const char*)ttm->meta.lbrc,u8size(ttm->meta.lbrc));
    if(str != NULL ||! str->fcn.builtin) {
	vsappendn(result,(const char*)str->fcn.body,strlen((const char*)str->fcn.body));
    } else { /* Use "" */
	/* append nothing */
    }
done:
    vsappendn(result,(const char*)ttm->meta.rbrc,u8size(ttm->meta.rbrc));
    return THROW(err);
}

/**
#<ttm;list;{case}>
#<ttm;list;class;{clname}>
Where case is one of:
"all" => list all names
"builtin" => list only builtin names
"string" => list only user-defined names
Each result is ';' separated and surrounded by <...>
Names are sorted.
*/

static int stringcmp(const void* a, const void* b); /*forward*/

static TTMERR
ttm_ttm_list(TTM* ttm, Frame* frame, VString* result) /* Misc. combined actions */
{
    TTMERR err = TTM_NOERR;
    size_t i;
    int first;
    VList* vl = vlnew();
    void* walker = NULL;
    struct HashEntry* entry = NULL;
    enum TTMEnum tte;
    struct HashTable* table = NULL;

    if(frame->argc < 3) {err = TTM_EFEWPARMS; goto done;}

    /* Figure out what we are collecting */
    switch (tte = ttmenumdetect(frame->argv[2])) {
    case TE_CLASS: table = &ttm->tables.charclasses; break;
    case TE_ALL: case TE_BUILTIN: case TE_STRING: table = &ttm->tables.dictionary; break;
    default: {err = TTM_EINVAL; goto done;}
    }

    /* Pass one: collect all the names */
    walker = hashwalk(table);
    while(hashnext(walker,&entry)) {
	Function* fcn = (Function*)entry;
	switch (tte) {
	case TE_ALL:
	    vlpush(vl,strdup((const char*)fcn->entry.name));
	    break;
	case TE_BUILTIN:
	    if(fcn->fcn.builtin) vlpush(vl,strdup((const char*)fcn->entry.name));
	    break;
	case TE_STRING:
	    if(!fcn->fcn.builtin) vlpush(vl,strdup((const char*)fcn->entry.name));
	    break;
	default: {err = TTM_EINVAL; goto done;}
	}
    }
    hashwalkstop(walker);
    {
    void* contents = vlcontents(vl); /* suppress gcc warning */
    /* Sort the names */
    qsort(contents,(size_t)vllength(vl),sizeof(utf8*),stringcmp);
    }
    /* construct the final result */
    for(first=2,i=0;i<vllength(vl);i++,first=0) {
	if(!first)
	    vsappendn(result,(const char*)ttm->meta.semic,u8size(ttm->meta.semic));
	/* Surround each name with <...> */
	vsappendn(result,(const char*)ttm->meta.lbrc,u8size(ttm->meta.lbrc));
	vsappendn(result,(const char*)vlget(vl,i),strlen((const char*)vlget(vl,i)));
	vsappendn(result,(const char*)ttm->meta.rbrc,u8size(ttm->meta.rbrc));
done:
    }
    return THROW(err);
}

/* Sort helper function */
static int
stringcmp(const void* a, const void* b)
{
    const char** sa = (const char**)a;
    const char** sb = (const char**)b;
    return strcmp(*sa,*sb);
}

/**
#<ttm;meta;which;char>		# Set the value for various meta characters
#<ttm;info;name;{name}*>	# return info about each {name}
#<ttm;info;class;{class}*>	# return info about each {class}
#<ttm;list;{case};{name}*>	# return sorted list of names defined by case
*/
static TTMERR
ttm_ttm(TTM* ttm, Frame* frame, VString* result) /* Misc. combined actions */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    utf8* which = NULL;
    utf8* klass = NULL;

    TTMFCN_BEGIN(ttm,frame,result);
    if(frame->argc < 2) {err = TTM_EFEWPARMS; goto done;}
    which = frame->argv[1];
    switch (ttmenumdetect(which)) {
    case TE_META:
	ttm_ttm_meta(ttm,frame,result);
	break;
    case TE_INFO:
	if(frame->argc < 3) {err = TTM_EFEWPARMS; goto done;}
	klass = frame->argv[2];
	switch (ttmenumdetect(klass)) {
	case TE_NAME:
	    ttm_ttm_info_name(ttm,frame,result);
	    break;
	case TE_CLASS:
	    ttm_ttm_info_class(ttm,frame,result);
	    break;
	case TE_STRING:
	    ttm_ttm_info_string(ttm,frame,result);
	    break;
	default: {err = TTM_ETTMCMD; goto done;}
	}
	break;
    case TE_LIST:
	ttm_ttm_list(ttm,frame,result);
	break;
    default:
	{err = TTM_ETTMCMD; goto done;}
	break;
    }
done:
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

/**************************************************/

/**
 Builtin function table
*/

struct Builtin {
    char* name;
    size_t minargs;
    size_t maxargs;
    enum FcnSV sv;
    TTMFCN fcn;
};

/* TODO: fix the minargs values */

/* Define a subset of the original TTM functions */

/* Define some temporary macros */
#define ARB MAXARGS

static struct Builtin builtin_orig[] = {
    /* Dictionary Operations */
    {"ap",2,2,SV_S,ttm_ap}, /* Append to a string */
    {"cf",2,2,SV_S,ttm_cf}, /* Copy a function */
    {"cr",2,2,SV_S,ttm_cr}, /* Mark for creation */
    {"ds",2,2,SV_S,ttm_ds}, /* Define string */
    {"es",0,ARB,SV_S,ttm_es}, /* Erase string */
    {"sc",2,63,SV_SV,ttm_sc}, /* Segment and count */
    {"ss",2,2,SV_S,ttm_ss}, /* Segment a string */
    /* Stateful String Selection */
    {"cc",1,1,SV_SV,ttm_cc}, /* Call one character */
    {"cn",2,2,SV_SV,ttm_cn}, /* Call n characters */
    {"sn",2,2,SV_S,ttm_sn}, /* Skip n characters */ /*Batch*/
    {"cp",1,1,SV_SV,ttm_cp}, /* Call parameter */
    {"cs",1,1,SV_SV,ttm_cs}, /* Call segment */
    {"isc",4,4,SV_SV,ttm_isc}, /* Initial character scan */
    {"rrp",1,1,SV_S,ttm_rrp}, /* Reset residual pointer */
    {"scn",3,3,SV_SV,ttm_scn}, /* Character scan */
    /* Stateless String Scanning Operations */
    {"gn",2,2,SV_V,ttm_gn}, /* Give n characters */
    {"zlc",1,1,SV_V,ttm_zlc}, /* Zero-level commas */
    {"zlcp",1,1,SV_V,ttm_zlcp}, /* Zero-level commas and parentheses */
    {"flip",1,1,SV_V,ttm_flip}, /* Flip a string */ /*Batch*/
    {"trl",1,1,SV_V,ttm_trl}, /* Translate to lowercase */ /*Batch*/
    {"thd",1,1,SV_V,ttm_thd}, /* Convert hexidecimal to decimal */ /*Batch*/
    /* Character Class Operations */
    {"ccl",2,2,SV_SV,ttm_ccl}, /* Call class */
    {"dcl",2,2,SV_S,ttm_dcl}, /* Define a class */
    {"dncl",2,2,SV_S,ttm_dncl}, /* Define a negative class */
    {"ecl",1,ARB,SV_S,ttm_ecl}, /* Erase a class */
    {"scl",2,2,SV_S,ttm_scl}, /* Skip class */
    {"tcl",4,4,SV_V,ttm_tcl}, /* Test class */
    /* Arithmetic Operations */
    {"abs",1,1,SV_V,ttm_abs}, /* Obtain absolute value */
    {"ad",2,ARB,SV_V,ttm_ad}, /* Add */
    {"dv",2,2,SV_V,ttm_dv}, /* Divide and give quotient */
    {"dvr",2,2,SV_V,ttm_dvr}, /* Divide and give remainder */
    {"mu",2,ARB,SV_V,ttm_mu}, /* Multiply */
    {"su",2,2,SV_V,ttm_su}, /* Substract */
    /* Numeric Comparisons */
    {"eq",4,4,SV_V,ttm_eq}, /* Compare numeric equal */
    {"gt",4,4,SV_V,ttm_gt}, /* Compare numeric greater-than */
    {"lt",4,4,SV_V,ttm_lt}, /* Compare numeric less-than */
    /* Logical Comparisons */
    {"eq?",4,4,SV_V,ttm_eql}, /* ? Compare logical equal */
    {"gt?",4,4,SV_V,ttm_gtl}, /* ? Compare logical greater-than */
    {"lt?",4,4,SV_V,ttm_ltl}, /* ? Compare logical less-than */
    /* Peripheral Input/Output Operations */
    {"cm",1,1,SV_S,ttm_cm}, /*Change Meta Character*/
    {"ps",1,ARB,SV_S,ttm_ps}, /* Print a sequence of strings */
    {"psr",1,1,SV_SV,ttm_psr}, /* Print string and then read */
    {"rs",0,1,SV_V,ttm_rs}, /* Read a string */
    /* Library Operations*/
    {"names",0,0,SV_V,ttm_names}, /* Obtain name strings */
    /* Utility Operations */
    {"exit",0,0,SV_S,ttm_exit}, /* Return from TTM */
    {"ndf",3,3,SV_V,ttm_ndf}, /* Determine if a name is defined */
    {"norm",1,1,SV_V,ttm_norm}, /* Obtain the norm (length) of a string */
    {"time",0,0,SV_V,ttm_time}, /* Obtain time of day (modified) */
    {"xtime",0,0,SV_V,ttm_xtime}, /* Obtain execution time */ /*Batch*/
    {"tf",0,0,SV_S,ttm_tf}, /* Turn Trace Off */
    {"tn",0,0,SV_S,ttm_tn}, /* Turn Trace On */
    {"eos",3,3,SV_V,ttm_eos}, /* Test for end of string */ /*Batch*/
    {NULL,0,0,SV_SV,NULL} /* end of builtins list */
};

/* Functions new to this implementation */
static struct Builtin builtin_new[] = {
    {"argv",1,1,SV_V,ttm_argv}, /* Get ith command line argument; 0<=i<argc */
    {"argc",0,0,SV_V,ttm_argc}, /* no. of command line arguments */
    {"classes",0,0,SV_V,ttm_classes}, /* Obtain character class names */
    {"ctime",1,1,SV_V,ttm_ctime}, /* Convert time to printable string */
    {"include",1,1,SV_S,ttm_include}, /* Include text of a file */
    {"lf",0,ARB,SV_S,ttm_lf}, /* Lock functions */
    {"uf",0,ARB,SV_S,ttm_uf}, /* Unlock functions */
    {"ttm",1,ARB,SV_SV,ttm_ttm}, /* Misc. combined actions */
    {"ge",4,4,SV_V,ttm_ge}, /* Compare numeric greater-than */
    {"le",4,4,SV_V,ttm_le}, /* Compare numeric less-than */
    {"void",0,0,SV_S,ttm_void}, /* throw away all arguments and return an empty string */
    {"comment",0,0,SV_S,ttm_void}, /* alias for ttm_void */
    {"setprop",1,2,SV_SV,ttm_setprop}, /* Set property */
    {"resetprop",1,2,SV_SV,ttm_resetprop}, /* set property to default */
    {"pse",1,ARB,SV_S,ttm_ps}, /* Print a sequence of strings, but charified control chars */
    {"printf",1,ARB,SV_S,ttm_printf}, /* Emulate printf() */
    {"fprintf",2,ARB,SV_S,ttm_fprintf}, /* Emulate fprintf() */
    {"pf",0,1,SV_S,ttm_pf}, /* flush stderr and/or stdout */
    {"tru",1,1,SV_V,ttm_tru}, /* Translate to uppercase */
    {"tdh",1,1,SV_V,ttm_tdh}, /* Convert a decimal value to hexidecimal */
    {"rp",1,1,SV_V,ttm_rp}, /* return the value of the residual pointer */
    {NULL,0,0,SV_SV,NULL} /* end of builtins list */
};

#if 0
/* Functions not implemented; they are irrelevant to this implementation*/
static struct Builtin builtin_irrelevant[] = {
    {"rcd",2,2,SV_S,ttm_rcd}, /* Set to Read from cards */
    {"fm",1,ARB,SV_S,ttm_fm}, /* Format a Line or Card */
    {"tabs",1,8,SV_S,ttm_tabs}, /* Declare Tab Positions */
    {"scc",2,2,SV_S,ttm_scc}, /* Set Continuation Convention */
    {"icc",1,1,SV_S,ttm_icc}, /* Insert a Control Character */
    {"outb",0,3,SV_S,ttm_outb}, /* Output the Buffer */
    /* Library Operations */
    {"store",2,2,SV_S,ttm_store}, /* Store a Program */
    {"delete",1,1,SV_S,ttm_delete}, /* Delete a Program */
    {"copy",1,1,SV_S,ttm_copy}, /* Copy a program */
    {"show",0,1,SV_S,ttm_show}, /* Show program strings */
    {"libs",2,2,SV_S,ttm_libs}, /* Declare standard qualifiers */ /*Batch*/
    {"break",0,1,SV_S,ttm_break}, /* Program Break */
    /* Batch Functions */
    {"insw",2,2,SV_S,ttm_insw}, /* Control output of input monitor */ /*Batch*/
    {"ttmsw",2,2,SV_S,ttm_ttmsw}, /* Control handling of ttm programs */ /*Batch*/
    {"cd",0,0,SV_V,ttm_cd}, /* Input one card */ /*Batch*/
    {"cdsw",2,2,SV_S,ttm_cdsw}, /* Control cd input */ /*Batch*/
    {"for",0,0,SV_V,ttm_for}, /* Input next complete fortran statement */ /*Batch*/
    {"forsw",2,2,SV_S,ttm_forsw}, /* Control for input */ /*Batch*/
    {"pk",0,0,SV_V,ttm_pk}, /* Look ahead one card */ /*Batch*/
    {"pksw",2,2,SV_S,ttm_pksw}, /* Control pk input */ /*Batch*/
    {"ps",1,1,SV_S,ttm_ps}, /* Print a string */ /*Batch*/ /*Modified*/
    {"page",1,1,SV_S,ttm_page}, /* Specify page length */ /*Batch*/
    {"sp",1,1,SV_S,ttm_sp}, /* Space before printing */ /*Batch*/
    {"fmsw",2,2,SV_S,ttm_fmsw}, /* Control fm output */ /*Batch*/
    {"des",1,1,SV_S,ttm_des}, /* Define error string */ /*Batch*/
    {NULL,0,0,NULL} /* terminator */
};
#endif /*0*/

static void
defineBuiltinFunction1(TTM* ttm, struct Builtin* bin)
{
    Function* fcn;

    /* Make sure we did not define builtin twice */
    fcn = dictionaryLookup(ttm,(utf8*)bin->name);
    if(fcn != NULL) FAILX(ttm,TTM_EDUPNAME,"fcn=%s\n",bin->name);
    /* create a new function object */
    fcn = newFunction(ttm);
    fcn->fcn.builtin = 1;
    fcn->fcn.locked = 1;
    fcn->fcn.minargs = bin->minargs;
    fcn->fcn.maxargs = bin->maxargs;
    fcn->fcn.sv = bin->sv;
    switch (bin->sv) {
    case SV_S:	fcn->fcn.novalue = 1; break;
    case SV_V:	fcn->fcn.novalue = 0; break;
    case SV_SV: fcn->fcn.novalue = 0; break;
    }
    fcn->fcn.fcn = bin->fcn;
    /* Convert name to utf8 */
    fcn->entry.name = (utf8*)strdup(bin->name);
    if(!dictionaryInsert(ttm,fcn)) {
	freeFunction(ttm,fcn);
	FAIL(ttm,TTM_ETTM);
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
