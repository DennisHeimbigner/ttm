/* Forward Types */

struct Builtin;

/**************************************************/

/* Define hdr/trailer actions for ttm_XXX builtin function calls */
#define TTMFCN_DECLS(ttm,frame) size_t unused
#define TTMFCN_BEGIN(ttm,frame,vsresult) UNUSED(unused)
#define TTMFCN_END(ttm,frame,vsresult)

/* Forward */
static void defineBuiltinFunction1(TTM* ttm, struct Builtin* bin);
static void defineBuiltinFunctions(TTM* ttm);
static char* trim(const char* s0, const char* ws);

/* Dictionary Operations */
static TTMERR
ttm_ap(TTM* ttm, Frame* frame, VString* result) /* Append to a string */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    VString* body = NULL;
    char* apstring = NULL;
    Function* str = NULL;
    size_t aplen;

    TTMFCN_BEGIN(ttm,frame,result);
    if((str = dictionaryLookup(ttm,frame->argv[1]))==NULL) {/* Define the string */
	err = ttm_ds(ttm,frame,result);
	goto done;
    }
    if(str->fcn.builtin) EXIT(TTM_ENOPRIM);
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
    char* newname = frame->argv[1];
    char* oldname = frame->argv[2];
    Function* newfcn = dictionaryLookup(ttm,newname);
    Function* oldfcn = dictionaryLookup(ttm,oldname);
    struct HashEntry saveentry;

    TTMFCN_BEGIN(ttm,frame,result);
    if(oldfcn == NULL) {err = FAILNONAMES(oldname); goto done;}
    if(newfcn == NULL) {
	/* create a new string object with given name */
	newfcn = newFunction(ttm,newname);
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
ttm_ds(TTM* ttm, Frame* frame, VString* result)
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    Function* str = NULL;
    TTMFCN_BEGIN(ttm,frame,result);
    str = dictionaryLookup(ttm,frame->argv[1]);
    if(str != NULL && str->fcn.locked) EXIT(TTM_ELOCKED);
    if(str != NULL) { /* clean for re-use */
	resetFunction(ttm,str);
    } else {
	/* create a new string object */
	str = newFunction(ttm,frame->argv[1]);
	dictionaryInsert(ttm,str);
    }
    str->fcn.trace = TR_UNDEF; /* default */
    str->fcn.builtin = 0;
    str->fcn.maxargs = ARB;
    str->fcn.sv = SV_SV;
    str->fcn.novalue = 0;
    str->fcn.body = vsnew();
    vsappendn(str->fcn.body,(const char*)frame->argv[2],0);
    vsindexset(str->fcn.body,0);
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
	Function* str = NULL;
	char* strname = frame->argv[i];
	if(strlen(strname)==0) continue; /* ignore empty names */
	str = dictionaryLookup(ttm,strname);
	if(str != NULL) {
	    if(str->fcn.locked) err = TTM_ELOCKED; /* remember but keep going */
	    else {
	        dictionaryRemove(ttm,strname);
		freeFunction(ttm,str); /* reclaim the string */
	    }
	}
    }
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

/**
Helper function for #<sc> and #<ss> and #<cr>.
For each copy of pattern, substitute the segmark or creation marks.
@param ttm
@param text to search
@param pattern to replace
@param segmark for substituting 
@param segcountp # of segment counters inserted.
@return TTMERR
*/
static TTMERR
ttm_subst(TTM* ttm, VString* text, const char* pattern, size_t segindex, size_t* segcountp)
{
    TTMERR err = TTM_NOERR;
    size_t patlen;
    size_t segcount = 0;
    utf8cpa segmark = empty_u8cpa;
    int seglen;
    size_t rp;
    const char* body = NULL;

    segmark2utf8(segmark,segindex);
    seglen = u8size(segmark);	

    patlen = strlen(pattern);
    if(patlen == 0) goto done; /* no subst possible */
    
    body = vscontents(text);
    rp = vsindex(text); /* save for restore */
    for(;;) {
	size_t pos = vsindex(text); /* search offset*/
	const char* search = NULL; /* search ptr */
	const char* q = NULL;
	if(pos >= vslength(text)) goto done; /* EOS */
	search = vsindexp(text);
	q = strstr8(search,pattern);
	if(q == NULL) break; /* search finished */
	/* q points to next matching string */
	pos = (size_t)(q - body); /* compute new pos of the matching string */
	vsindexset(text,pos); /* set the rp to the new pos */
	vaindexremoven(text,patlen); /* remove pattern */
	vaindexinsertn(text,segmark,seglen); /* insert segmark; will update index */
	segcount++;
    }	
done:
    vsindexset(text,rp); /* restore */
    if(segcountp) *segcountp = segcount;
    return THROW(err);
}


static TTMERR
ttm_sc(TTM* ttm, Frame* frame, VString* result) /* Segment and count */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    Function* str = NULL;
    size_t i, segindex;
    size_t nsegs, segcount;
    VString* text = NULL;
    char count[64];

    TTMFCN_BEGIN(ttm,frame,result);
    if((str = dictionaryLookup(ttm,frame->argv[1]))==NULL) EXIT(TTM_ENONAME);
    if(str->fcn.builtin) EXIT(TTM_ENOPRIM);
    if(str->fcn.locked) EXIT(TTM_ELOCKED);
    text = str->fcn.body;
    segindex = SEGINDEXFIRST;
    segcount = 0;
    for(i=2;i<frame->argc;i++,segindex++) {
	nsegs = 0;
	if((err=ttm_subst(ttm,text,frame->argv[i],segindex,&nsegs))) EXIT(err);
	segcount += nsegs;
    }
    snprintf(count,sizeof(count),"%zu",segcount);
    /* Insert into result */
    vsappendn(result,(const char*)count,strlen(count));
done:
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_ss(TTM* ttm, Frame* frame, VString* result) /* Segment and count */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    Function* str = NULL;
    size_t i, segindex;
    VString* text = NULL;

    TTMFCN_BEGIN(ttm,frame,result);
    if((str = dictionaryLookup(ttm,frame->argv[1]))==NULL) EXIT(TTM_ENONAME);
    if(str->fcn.builtin) EXIT(TTM_ENOPRIM);
    if(str->fcn.locked) EXIT(TTM_ELOCKED);
    text = str->fcn.body;
    segindex = SEGINDEXFIRST;
    for(i=2;i<frame->argc;i++,segindex++) {
	if((err=ttm_subst(ttm,text,frame->argv[i],segindex,NULL))) EXIT(err);
    }
done:
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_cr(TTM* ttm, Frame* frame, VString* result) /* Mark for creation */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    Function* str = NULL;
    VString* text = NULL;

    TTMFCN_BEGIN(ttm,frame,result);
    if((str = dictionaryLookup(ttm,frame->argv[1]))==NULL) EXIT(TTM_ENONAME);
    if(str->fcn.builtin) EXIT(TTM_ENOPRIM);
    if(str->fcn.locked) EXIT(TTM_ELOCKED);
    text = str->fcn.body;
    err = ttm_subst(ttm,text,frame->argv[2],CREATEINDEXONLY,NULL);
done:
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

/* String Selection */

static TTMERR
ttm_cc(TTM* ttm, Frame* frame, VString* result) /* Call one character */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    Function* str = NULL;

    TTMFCN_BEGIN(ttm,frame,result);
    if((str = dictionaryLookup(ttm,frame->argv[1]))==NULL) EXIT(TTM_ENONAME);
    if(str->fcn.builtin) EXIT(TTM_ENOPRIM);
    if(vsindex(str->fcn.body) < vslength(str->fcn.body)) {
	int ncp;
	const char* p = vsindexp(str->fcn.body);
	ncp = u8size(p);
	vsappendn(result,p,ncp);
	vsindexskip(str->fcn.body,ncp);
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
    const char* p = NULL;

    TTMFCN_BEGIN(ttm,frame,result);
    if((fcn = getdictstr(ttm,frame,2))==NULL) {err = FAILNONAME(2); goto done;}

    /* Get number of codepoints to extract */
    if(1 != sscanf((const char*)frame->argv[1],"%lld",&ln)) EXIT(TTM_EDECIMAL);
    if(ln < 0) EXIT(TTM_ENOTNEGATIVE);

    vsclear(result);
    n = (int)ln;
    p = vsindexp(fcn->fcn.body);
    nbytes = (int)strlen((const char*)p);
    if(n == 0 || nbytes == 0) goto done;
    /* copy up to n codepoints or EOS encountered */
    while(n-- > 0) {
	p = vsindexp(fcn->fcn.body);
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
    const char* p;

    TTMFCN_BEGIN(ttm,frame,result);
    if((str = getdictstr(ttm,frame,2))==NULL) {err = FAILNONAME(2); goto done;}
    if(1 != sscanf((const char*)frame->argv[1],"%lld",&num)) EXIT(TTM_EDECIMAL);
    if(num < 0) EXIT(TTM_ENOTNEGATIVE);

    for(p=vsindexp(str->fcn.body);num-- > 0;) {
	int ncp = u8size(p);
	if(ncp < 0) EXIT(TTM_EUTF8);
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
    char* t;
    char* f;
    char* value;
    char* arg;
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
    char* f;
    char* arg;
    char* p;
    char* p0;

    TTMFCN_BEGIN(ttm,frame,result);
    str = getdictstr(ttm,frame,2);

    arg = frame->argv[1];
    arglen = strlen((const char*)arg);
    f = frame->argv[3];

    /* check for sub string match */
    p0 = vsindexp(str->fcn.body);
    p = strstr(p0,arg);
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
    char* rp;
    char* rp0;
    int depth;

    TTMFCN_BEGIN(ttm,frame,result);
    fcn = getdictstr(ttm,frame,1);

    rp0 = vsindexp(fcn->fcn.body);
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
    char* p;
    char* p0;
    size_t delta;

    TTMFCN_BEGIN(ttm,frame,result);

    fcn = getdictstr(ttm,frame,1);
    if(fcn == NULL) EXIT(TTM_ENONAME);
    /* Locate the next segment mark */
    /* Unclear if create marks also qualify; assume yes */
    p0 = vsindexp(fcn->fcn.body);
    p = p0;
    for(;;) {
	int ncp;
	if((ncp = u8size(p))<=0) EXIT(TTM_EUTF8);
	if(isnul(p))
	    break;
	if(issegmark(p)) break; /* Includes create mark */
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
    char* t;
    char* f;
    char* result;
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
    char* snum = frame->argv[1];
    char* s = frame->argv[2];
    long long num;
    int nbytes = 0;

    TTMFCN_BEGIN(ttm,frame,result);
    if(1 != sscanf((const char*)snum,"%lld",&num)) EXIT(TTM_EDECIMAL);
    if(num > 0) {
	nbytes = u8ith(s,(int)num);
	if(nbytes <= 0) EXIT(TTM_EUTF8);
	vsappendn(result,(const char*)s,(size_t)nbytes);
    } else if(num < 0) {
	num = -num;
	nbytes = u8ith(s,(int)num);
	if(nbytes <= 0) EXIT(TTM_EUTF8);
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
    char* s;
    char* p;
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
    char* s;
    char* p;
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
    const char* s;
    const char* p;
    int ncp = 0;

    TTMFCN_BEGIN(ttm,frame,result);
    s = frame->argv[1];
    for(p=s;*p;p+=ncp) {
	ncp = u8size(p);
	if(ncp <= 0) EXIT(TTM_EUTF8);
	vsinsertn(result,0,p,ncp);
    }
done:
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_trl(TTM* ttm, Frame* frame, VString* result) /* translate to lower case */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    char* s;

    TTMFCN_BEGIN(ttm,frame,result);
    vsclear(ttm->vs.tmp);
    s = frame->argv[1];
    {
	size_t i,mbslen,s8len;
	wchar_t* sw = NULL;
	char* s8;

	/* Calculate the length required to hold wchar_t converted s */
	mbslen = mbstowcs(NULL, s, 0);
        if(mbslen == (size_t) -1) EXIT(TTM_EUTF8);
	/* Allocate wide character string of the desired size.  Add 1
	  to allow for terminating null wide character (L'\0'). */
        vssetlength(ttm->vs.tmp,(mbslen + 1)*sizeof(wchar_t));
        sw = (wchar_t*)vscontents(ttm->vs.tmp);
        /* Convert the multibyte character string s to a wide character string. */
        mbslen = mbstowcs(NULL, s, 0);
        if(mbslen == (size_t) -1) EXIT(TTM_EUTF8);
        mbslen = mbstowcs(sw, s, mbslen + 1);
        if(mbslen == (size_t) -1) EXIT(TTM_EUTF8);
	/* Convert to lower case */
	for(i=0;i<mbslen;i++)
	    sw[i] = (wchar_t)towlower(sw[i]);
	/* Convert back to utf8 */
	/* Get space required */
	s8len = wcstombs(NULL, sw, 0);
	if(s8len == ((size_t) -1)) EXIT(TTM_EUTF8);
	/* Allocate memory for the utf8 character string */
	vssetlength(result,s8len);
	s8 = vscontents(result);
	/* Insert into the result buffer */
	s8len = wcstombs(s8,sw,s8len+1);
        if(s8len == (size_t) -1) EXIT(TTM_EUTF8);
    }
done:
    vsclear(ttm->vs.tmp);
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
	if(!ishex(*p)) EXIT(TTM_EINVAL);        
	*q++ = *p++;
    }
    *q = '\0';
    if(1 != sscanf(hex,"%llx",&un)) EXIT(TTM_EINVAL);
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
    if(strlen(frame->argv[1])==0) EXIT(TTM_ENOCLASS);
    cl = charclassLookup(ttm,frame->argv[1]);
    if(cl == NULL) {
	/* create a new charclass object */
	cl = newCharclass(ttm,frame->argv[1]);
	charclassInsert(ttm,cl);
    }
    nullfree(cl->characters);
    cl->characters = strdup((const char*)frame->argv[2]);
    cl->negative = negative;
done:
    return THROW(err);
}

static TTMERR
ttm_dcl(TTM* ttm, Frame* frame, VString* result) /* Define a negative class */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    TTMFCN_BEGIN(ttm,frame,result);
    err = ttm_dcl0(ttm,frame,0,result);
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_dncl(TTM* ttm, Frame* frame, VString* result) /* Define a negative class */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    TTMFCN_BEGIN(ttm,frame,result);
    err = ttm_dcl0(ttm,frame,1,result);
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
	Charclass* cl = NULL;
	char* clname = frame->argv[i];
	if(strlen(clname)==0) continue; /* ignore empty names */
	cl = charclassRemove(ttm,clname);
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
    const char* clseq = NULL;
    const char* cp8 = NULL;
    const char* match = NULL;
    size_t rr0,delta;

    TTMFCN_BEGIN(ttm,frame,result);
    if((str = getdictstr(ttm,frame,2))==NULL) {err = FAILNONAME(2); goto done;}
    if((cl = charclassLookup(ttm,frame->argv[1]))==NULL) FAILNOCLASS(1);

    clseq = cl->characters;
    cp8 = vsindexp(str->fcn.body);

    /* Starting at vsindex(str->fcn.body), locate first char not in class (or negative) */
    vsclear(result);
    rr0 = vsindex(str->fcn.body);
    match = charclassmatch(cp8,clseq,cl->negative);
    delta = (size_t)(match - cp8);
    vsindexset(str->fcn.body,rr0); /* temporary */
    vsappendn(result,vsindexp(str->fcn.body),delta);
    vsindexset(str->fcn.body,rr0+delta); /* final residual index */
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
    const char* clseq = NULL;
    const char* cp8 = NULL;
    const char* match = NULL;
    size_t rr0, delta;

    TTMFCN_BEGIN(ttm,frame,result);
    if((str = getdictstr(ttm,frame,2))==NULL) {err = FAILNONAME(2); goto done;}
    if((cl = charclassLookup(ttm,frame->argv[1]))==NULL) FAILNOCLASS(1);

    vsclear(result);
    clseq = cl->characters;
    cp8 = vsindexp(str->fcn.body);
    rr0 = vsindex(str->fcn.body);
    match = charclassmatch(cp8,clseq,cl->negative);
    delta = (size_t)(match - cp8);
    vsindexset(str->fcn.body,rr0+delta); /* final residual index */

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
    const char* retval;
    const char* t = NULL;
    const char* f = NULL;
    const char* clseq = NULL;
    const char* cp8 = NULL;
    const char* match = NULL;

    TTMFCN_BEGIN(ttm,frame,result);
    if((cl = charclassLookup(ttm,frame->argv[1]))==NULL) FAILNOCLASS(1);

    if(strlen(frame->argv[2]) > 0) {
	if((str = getdictstr(ttm,frame,2))==NULL) {err = FAILNONAME(2); goto done;}
    } else
        str = NULL;

    vsclear(result);
    t = frame->argv[3];
    f = frame->argv[4];
    if(str == NULL)
	retval = f;
    else {
        clseq = cl->characters;
        cp8 = vsindexp(str->fcn.body);
        /* Starting at vsindex(str->fcn.body), locate first char not in class (or negative) */
        match = charclassmatch(cp8,clseq,cl->negative);
	if(match == cp8) retval = f; else retval = t;
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
    char* slhs;
    long long lhs;
    char value[128];

    TTMFCN_BEGIN(ttm,frame,result);
    slhs = frame->argv[1];
    if(1 != sscanf((const char*)slhs,"%lld",&lhs)) EXIT(TTM_EDECIMAL);
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
    char* srhs;
    long long lhs;
    long long rhs;
    char value[128];
    size_t i;

    TTMFCN_BEGIN(ttm,frame,result);
    for(lhs=0,i=1;i<frame->argc;i++) {
	srhs = frame->argv[i];
	if(1 != sscanf((const char*)srhs,"%lld",&rhs)) EXIT(TTM_EDECIMAL);
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
    char* slhs;
    long long lhs;
    char* srhs;
    long long rhs;
    char value[128];

    TTMFCN_BEGIN(ttm,frame,result);
    slhs = frame->argv[1];
    srhs = frame->argv[2];
    if(1 != sscanf((const char*)slhs,"%lld",&lhs)) EXIT(TTM_EDECIMAL);
    if(1 != sscanf((const char*)srhs,"%lld",&rhs)) EXIT(TTM_EDECIMAL);
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
    char* slhs;
    long long lhs;
    char* srhs;
    long long rhs;
    char value[128];

    TTMFCN_BEGIN(ttm,frame,result);
    slhs = frame->argv[1];
    srhs = frame->argv[2];
    if(1 != sscanf((const char*)slhs,"%lld",&lhs)) EXIT(TTM_EDECIMAL);
    if(1 != sscanf((const char*)srhs,"%lld",&rhs)) EXIT(TTM_EDECIMAL);
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
    char* srhs;
    long long lhs;
    long long rhs;
    char value[128];
    size_t i;

    TTMFCN_BEGIN(ttm,frame,result);
    for(lhs=1,i=1;i<frame->argc;i++) {
	srhs = frame->argv[i];
	if(1 != sscanf((const char*)srhs,"%lld",&rhs)) EXIT(TTM_EDECIMAL);
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
    char* slhs;
    long long lhs;
    char* srhs;
    long long rhs;
    char value[128];

    TTMFCN_BEGIN(ttm,frame,result);
    slhs = frame->argv[1];
    srhs = frame->argv[2];
    if(1 != sscanf((const char*)slhs,"%lld",&lhs)) EXIT(TTM_EDECIMAL);
    if(1 != sscanf((const char*)srhs,"%lld",&rhs)) EXIT(TTM_EDECIMAL);
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
    char* slhs;
    char* srhs;
    long long lhs,rhs;
    char* t;
    char* f;
    char* value;

    TTMFCN_BEGIN(ttm,frame,result);
    slhs = frame->argv[1];
    srhs = frame->argv[2];
    t = frame->argv[3];
    f = frame->argv[4];

    if(1 != sscanf((const char*)slhs,"%lld",&lhs)) EXIT(TTM_EDECIMAL);
    if(1 != sscanf((const char*)srhs,"%lld",&rhs)) EXIT(TTM_EDECIMAL);
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
    char* slhs;
    char* srhs;
    long long lhs,rhs;
    char* t;
    char* f;
    char* value;

    TTMFCN_BEGIN(ttm,frame,result);
    slhs = frame->argv[1];
    srhs = frame->argv[2];
    t = frame->argv[3];
    f = frame->argv[4];

    if(1 != sscanf((const char*)slhs,"%lld",&lhs)) EXIT(TTM_EDECIMAL);
    if(1 != sscanf((const char*)srhs,"%lld",&rhs)) EXIT(TTM_EDECIMAL);
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
    char* slhs;
    char* srhs;
    long long lhs,rhs;
    char* t;
    char* f;
    char* value;

    TTMFCN_BEGIN(ttm,frame,result);
    slhs = frame->argv[1];
    srhs = frame->argv[2];
    t = frame->argv[3];
    f = frame->argv[4];

    if(1 != sscanf((const char*)slhs,"%lld",&lhs)) EXIT(TTM_EDECIMAL);
    if(1 != sscanf((const char*)srhs,"%lld",&rhs)) EXIT(TTM_EDECIMAL);
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
    char* slhs;
    char* srhs;
    long long lhs,rhs;
    char* t;
    char* f;
    char* value;

    TTMFCN_BEGIN(ttm,frame,result);
    slhs = frame->argv[1];
    srhs = frame->argv[2];
    t = frame->argv[3];
    f = frame->argv[4];

    if(1 != sscanf((const char*)slhs,"%lld",&lhs)) EXIT(TTM_EDECIMAL);
    if(1 != sscanf((const char*)srhs,"%lld",&rhs)) EXIT(TTM_EDECIMAL);
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
    char* slhs;
    char* srhs;
    long long lhs,rhs;
    char* t;
    char* f;
    char* value;

    TTMFCN_BEGIN(ttm,frame,result);
    slhs = frame->argv[1];
    srhs = frame->argv[2];
    t = frame->argv[3];
    f = frame->argv[4];

    if(1 != sscanf((const char*)slhs,"%lld",&lhs)) EXIT(TTM_EDECIMAL);
    if(1 != sscanf((const char*)srhs,"%lld",&rhs)) EXIT(TTM_EDECIMAL);
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
    char* slhs;
    char* srhs;
    char* t;
    char* f;
    char* value;

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
    char* slhs;
    char* srhs;
    char* t;
    char* f;
    char* value;

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
    char* slhs;
    char* srhs;
    char* t;
    char* f;
    char* value;

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
selectfile(TTM* ttm, const char* fname, IOMODE required_modes, TTMFILE** targetp)
{
    TTMERR err = TTM_NOERR;
    TTMFILE* target = NULL;
    if(strcmp("stdin",(const char*)fname)==0) {
	target = ttm->io._stdin;
    } else if(strcmp("stdout",(const char*)fname)==0) {
	target = ttm->io._stdout;
    } else if(strcmp("stderr",(const char*)fname)==0) {
	target = ttm->io._stderr;
    } else if(strcmp("-",(const char*)fname)==0) {
	target = ttm->io._stdin; 
    } else
    	EXIT(TTM_EACCESS);
    /* Check the modes */
    if((target->mode & required_modes) != required_modes)
	EXIT(TTM_EACCESS);
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
ttm_ps0(TTM* ttm, TTMFILE* target, int argc, char** argv, VString* result) /* Print a Function/String */
{
    TTMERR err = TTM_NOERR;
    int i;
    
    for(i=0;i<argc;i++) {
	char* cleaned = cleanstring(argv[i],"\n\r\t",NULL);
	printstring(ttm,cleaned,target);
	nullfree(cleaned);
    }
    ttmflush(ttm,target);
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
    target = ttm->io._stdout; /* choose stdout as default target */
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
	    target = ttm->io._stdin; /* choose stdin as default target */
	}
    } else
        target = ttm->io._stdin; /* default */
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
    reader = ttm->io._stdin;
    writer = ttm->io._stdout;
    ttm_ps0(ttm,writer,(int)frame->argc-1,frame->argv+1,result);
    ttmflush(ttm,ttm->io._stdout);
    ttm_rs0(ttm,reader,result);
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_cm(TTM* ttm, Frame* frame, VString* result) /* Change meta character and return previous value*/
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    char* replacement = NULL;
    utf8cpa prev = empty_u8cpa;

    TTMFCN_BEGIN(ttm,frame,result);
    if(frame->argc < 1) EXIT(TTM_EFEWPARMS);
    replacement = frame->argv[1];
    if(u8size(replacement) <= 0) EXIT(TTM_EUTF8);
    memcpycp(prev,ttm->meta.metac);
    if(strlen(replacement) > 0) memcpycp(ttm->meta.metac,replacement);
    vsappendn(result,prev,u8size(prev));
done:
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

/* printf helper functions */

static TTMERR
parsespec(const char* fmt, int* lcountp, char* typp, int* ispcentp, size_t* speclenp)
{
    TTMERR err = TTM_NOERR;
    const char* p = fmt;
    int lcount = 0;
    char typ = 'd';
    int ispcent = 0;

    p++; /* skip leading '%' */
    if(isnul(p)) EXITX(TTM_EIO);
    if(*p == '%') {p++; ispcent = 1; goto havevalue;}
    if(isnul(p)) EXITX(TTM_EIO);	    
    if(*p == 'l') {lcount++; p++;}
    if(isnul(p)) EXITX(TTM_EIO);
    if(*p == 'l') {lcount++; p++;}
    if(isnul(p)) EXITX(TTM_EIO);
    switch (*p) {
    case 'd': case 'u': case 'o': case 'x': case 's': typ = *p; p++; break;
    default: break;
    }
havevalue:
    if(ispcentp) *ispcentp = ispcent;
    if(lcountp) *lcountp = lcount;
    if(typp) *typp = typ;
    if(speclenp) *speclenp = (size_t)(p - fmt);
done:
    return THROWX(err);
}

static void
buildspec(int lcount, char typ, char* spec)
{
    char* q = spec;
    *q++ = '%';
    if(lcount > 0) *q++ = 'l';
    if(lcount > 1) *q++ = 'l';
    *q++ = typ;
    *q = '\0';
}

static TTMERR
intconvert(char* arg, long long* lldp)
{
    TTMERR err = TTM_NOERR;
    int count;
    long long lld;
    count = sscanf(arg,"%lld",&lld);
    if(count != 1) EXITX(TTM_EDECIMAL);
    if(lldp) *lldp = lld;
done:
    return THROWX(err);
}

static void
strcvt(int lcount, char typ, long long lld, char* svalue, size_t svallen)
{
    char spec[64]; /* big enough to hold any legal spec */
    buildspec(lcount,typ,spec);
    /* Print according to spec */
    switch (typ) {
    case 'd':   switch (lcount) {
		case 0: snprintf(svalue,svallen,spec,(int)((long long)lld)); break;
		case 1: snprintf(svalue,svallen,spec,(long)((long long)lld)); break;
		case 2: snprintf(svalue,svallen,spec,((long long)lld)); break;
		}
    case 'u': case 'o': case 'x':
	switch (lcount) {
		case 0: snprintf(svalue,svallen,spec,(unsigned)(lld)); break;
	case 1: snprintf(svalue,svallen,spec,(unsigned long)(lld)); break;
	case 2: snprintf(svalue,svallen,spec,(unsigned long long)(lld)); break;
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
ttm_vprintf(TTM* ttm, TTMFILE* target, const char* fmt8, size_t argc, char** argv, VString* result) /* Print a Function/String */
{
    TTMERR err = TTM_NOERR;
    const char* fmt = (const char*)fmt8;
    size_t argi;
    char* p = NULL;
    int lcount,ispcent;
    char typ;
    int stop;
    size_t speclen;
	    
    argi=0;
    p = (char*)fmt;
    /* Walk the fmt and parse each spec in turn */
    do {
        int ncp = u8size(p);
	stop = 0;
	if(isnul(p)) {
	    stop = 1; /* EOS => stop */
	} else if(issegmark(p)) {
	    char info[16];
	    size_t segindex = (size_t)segmarkindex(p);
	    if(iscreateindex(segindex))
		snprintf(info,sizeof(info),"^{CR}");
	    else
		snprintf(info,sizeof(info),"^{%x}",(unsigned)segindex);
	    vsappendn(result,info,strlen(info));
	    p += SEGMARKSIZE;
	} else if(*p == '%') { /* collect the %... spec */
	    lcount = 0; typ = '\0'; ispcent = 0; /* re-initialize */
	    if((err = parsespec(p,&lcount,&typ,&ispcent,&speclen))) goto done;
	    if(ispcent) {  /* special case for "%%" */
	        vsappend(result,'%');
		p += 2;
		continue;
	    }
	    if(argi >= argc) EXIT(TTM_EFEWPARMS);
	    /* check the type */
	    if(typ == 's') {/* Handle string separately */
		vsappendn(result,(char*)argv[argi++],0);
	    } else { /* Integer type */
		long long lld = 0;
		char svalue[256]; /* big enough to hold single integer value string */
		if((err = intconvert((char*)argv[argi],&lld))) goto done;
		strcvt(lcount,typ,lld,svalue,sizeof(svalue));
		vsappendn(result,svalue,0);
	    }
	    p += speclen; /* skip fmt in input string */
	} else {/* pass the char */
	    vsappendn(result,p,ncp);
	    p += ncp;
	}
    } while(!stop);
    /* print the result string (see ttm_ps) */
    printstring(ttm,vscontents(result),target);
#ifdef GDB
    ttmflush(ttm,target);
#endif
done:
    return THROW(err);
}

/**
Emulate restricted **nix fprintf function with:
- target file as argv[1]: legal values are "stdout"|"stderr"|("-" | "") [to signal ttm->io.output]
- format as argv[2].
    -- format tags are "%[l|ll]?[duoxs]"
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
    char* fmt = NULL;
    const char* fname = NULL;
    
    TTMFCN_BEGIN(ttm,frame,result);
    if(frame->argc < 3) EXIT(TTM_EFEWPARMS);
    /* Figure out the target file */
    fname = frame->argv[1];
    if((err = selectfile(ttm,fname,IOM_WRITE,&target))!=TTM_NOERR) {
	target = ttm->io._stdout; /* default */
    }

    /* Get the fmt */
    fmt = frame->argv[2];

    err = ttm_vprintf(ttm,target,fmt,frame->argc-3,frame->argv+3,result);

done:
    return THROW(err);
}

/**
Emulate restricted **nix printf function with:
- format is argv[1].
    -- format tags are "%[l|ll]?[duoxs]"
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
    char* fmt = NULL;
    
    TTMFCN_BEGIN(ttm,frame,result);
    if(frame->argc < 2) EXIT(TTM_EFEWPARMS);
    /* Printf always writes to stdout */
    target = ttm->io._stdout; /* default */

    /* Get the fmt */
    fmt = frame->argv[1];

    err = ttm_vprintf(ttm,target,fmt,frame->argc-2,frame->argv+2,result);

done:
    return THROW(err);
}

static TTMERR
ttm_pf(TTM* ttm, Frame* frame, VString* result) /* Flush stdout and/or stderr */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    char* filename = NULL;
    TTMFILE* file = NULL;

    TTMFCN_BEGIN(ttm,frame,result);
    filename = (frame->argc == 1 ? NULL : frame->argv[1]);
    if(filename == NULL || strlen(filename)==0)
	filename = "stdout";
    else
        filename = frame->argv[1];
    /* Convert file name to file */
    file = ttmfindfile(ttm,filename);
    if(file == NULL) EXIT(TTM_EACCESS);
    ttmflush(ttm,file);
done:
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_tru(TTM* ttm, Frame* frame, VString* result) /* translate to upper case */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    char* s;

    TTMFCN_BEGIN(ttm,frame,result);
    vsclear(ttm->vs.tmp);
    s = frame->argv[1];
    {
	size_t i,mbslen,s8len;
	wchar_t* sw = NULL;
	char* s8;

	/* Calculate the length required to hold wchar_t converted s */
	mbslen = mbstowcs(NULL, s, 0);
        if(mbslen == (size_t) -1) EXIT(TTM_EUTF8);
	/* Allocate wide character string of the desired size.  Add 1
	  to allow for terminating null wide character (L'\0'). */
        vssetlength(ttm->vs.tmp,(mbslen + 1)*sizeof(wchar_t));
        sw = (wchar_t*)vscontents(ttm->vs.tmp);
        /* Convert the multibyte character string s to a wide character string. */
        mbslen = mbstowcs(NULL, s, 0);
        if(mbslen == (size_t) -1) EXIT(TTM_EUTF8);
        mbslen = mbstowcs(sw, s, mbslen + 1);
        if(mbslen == (size_t) -1) EXIT(TTM_EUTF8);
	/* Convert to lower case */
	for(i=0;i<mbslen;i++)
	    sw[i] = (wchar_t)towupper(sw[i]);
	/* Convert back to utf8 */
	/* Get space required */
	s8len = wcstombs(NULL, sw, 0);
	if(s8len == ((size_t) -1)) EXIT(TTM_EUTF8);
	/* Allocate memory for the utf8 character string */
	vssetlength(result,s8len);
	s8 = vscontents(result);
	/* Insert into the result buffer */
	s8len = wcstombs(s8,sw,s8len+1);
        if(s8len == (size_t) -1) EXIT(TTM_EUTF8);
    }
done:
    vsclear(ttm->vs.tmp);
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
	if(!isdec(*p)) EXIT(TTM_EINVAL);        
	*q++ = *p++;
    }
    *q = '\0';
    if(1 != sscanf(dec,"%lld",&ln)) EXIT(TTM_EINVAL);
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
    char* s;
    size_t rp = 0;
    char srp[128];
    
    TTMFCN_BEGIN(ttm,frame,result);
    if((str = getdictstr(ttm,frame,1))==NULL) {err = FAILNONAME(1); goto done;}
    s = vscontents(str->fcn.body);
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

static TTMERR
ttm_srp(TTM* ttm, Frame* frame, VString* result) /* set residual pointer*/
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    Function* str;
    size_t srp = 0;
    
    TTMFCN_BEGIN(ttm,frame,result);
    switch (frame->argc) {
    case 0: case 1: EXIT(TTM_EFEWPARMS);
    case 2:
	srp = 0; /* default if rp not defined */
	break;
    default:
        if(1!=sscanf(frame->argv[2],"%zu",&srp)) EXIT(TTM_EDECIMAL);
	break;
    }
    if((str = getdictstr(ttm,frame,1))==NULL) {err = FAILNONAME(1); goto done;}
    /* convert from codepoint offset to byte offset */
    srp = cptorp(ttm,vscontents(str->fcn.body)+srp,srp);
    vsindexset(str->fcn.body,srp);

done:
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static int
stringveccmp(const void* a, const void* b)
{
    const char** sa = (const char**)a;
    const char** sb = (const char**)b;
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
#	define TTM_NAMES_ALL	  (1<<0)
#	define TTM_NAMES_STRINGS  (1<<1)
#	define TTM_NAMES_BUILTIN  (1<<2)
#	define TTM_NAMES_SPECIFIC (1<<3)
#	    define TTM_NAMES_BODY (1<<16) /* => SPECIFIC|BODY*/

    TTMFCN_BEGIN(ttm,frame,result);
    /* Collect the class of names to be returned:
       * #<names> -- all names
       * #<names;strings[,body]>	-- all ##<ds;string> names with optional body
       * #<names;builtin>		-- all builtin names
       * #<names;;...>			-- specific names; note the ';;' empty arg
       * #<names;body;...>		-- specific + body
    */
    i = frame->argc;
    switch (i) {
    case 0: err = TTM_ETTM; goto done; /* should never happen */
    case 1: klass = TTM_NAMES_ALL; break;
    case 2:
	arg = (const char*)frame->argv[i];
	if(strcmp(arg,"strings")==0)
	    klass = TTM_NAMES_STRINGS;
	else if(strcmp(arg,"strings,body")==0)
	    klass = TTM_NAMES_STRINGS | TTM_NAMES_BODY;
	else if(strcmp(arg,"builtin")==0)
	    klass = TTM_NAMES_BUILTIN;
	else if(arg == NULL || strcmp(arg,"")==0)
	    klass = TTM_NAMES_SPECIFIC;
	else if(arg == NULL || strcmp(arg,"body")==0)
	    klass = TTM_NAMES_SPECIFIC | TTM_NAMES_BODY;
	break;
    default: klass = TTM_NAMES_SPECIFIC; break;
    }

    if(klass == 0) EXIT(TTM_EFEWPARMS);

    /* Collect all the relevant Functions. */
    for(i=0;i<HASHSIZE;i++) {
	struct Function* fcn = (struct Function*)ttm->tables.dictionary.table[i].next;
	while(fcn != NULL) {
	    if((klass & TTM_NAMES_ALL)
	       || ((klass & TTM_NAMES_STRINGS) && !fcn->fcn.builtin)
	       || ((klass & TTM_NAMES_BUILTIN) && fcn->fcn.builtin)) {
		vlpush(nameset,fcn);
	    } else if((klass & TTM_NAMES_SPECIFIC)) {
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
    /* print names comma separated and optional body */
    for(i=0;i<vllength(nameset);i++) {
	Function* f =  (Function*)vlget(nameset,i);
	const char* nm = f->entry.name;
	if(i > 0) vsappend(result,',');
	vsappendn(result,(const char*)nm,0);
	if(!f->fcn.builtin && (klass & TTM_NAMES_BODY)==TTM_NAMES_BODY) {
	    vsappendn(result,"=|",2);
	    vsappendn(result,vscontents(f->fcn.body),vslength(f->fcn.body));
	    vsappendn(result,"|",1);	
	}
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
	if(1!=sscanf((const char*)frame->argv[1],"%lld",&exitcode)) EXIT(TTM_EDECIMAL);
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
    char* t;
    char* f;
    char* value;

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
    char* s;
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

static int
timeofday(struct timeval *tv)
{
#define DELTA_EPOCH_IN_MICROSECS  11644473600000000ULL
static int tzflag = 0;

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

static long long
getRunTime(void)
{
static long frequency = 0;
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

static TTMERR
ttm_time(TTM* ttm, Frame* frame, VString* result) /* Obtain time of day */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    char value[MAXINTCHARS+1];
    struct timeval tv;
    long long time;

    TTMFCN_BEGIN(ttm,frame,result);
    if(ttm->opts.testing) {
	strncpy(value,fixedtestvalues.time,sizeof(value));
    } else {
	if(timeofday(&tv) < 0)
	    EXIT(TTM_ETIME);
	time = (long long)tv.tv_sec;
	time *= 1000000; /* convert to microseconds */
	time += tv.tv_usec;
	time = time / 10000; /* Need time in 100th second */
	snprintf(value,sizeof(value),"%lld",time);
    }
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
    char value[MAXINTCHARS+1];

    TTMFCN_BEGIN(ttm,frame,result);
    if(ttm->opts.testing) {
	strncpy(value,fixedtestvalues.xtime,sizeof(value));
    } else {
	long long time = getRunTime();
	snprintf(value,sizeof(value),"%lld",time);
    }
    vsappendn(result,value,strlen(value));
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_ctime(TTM* ttm, Frame* frame, VString* result) /* Convert ##<time> to printable string */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    char* stod;
    long long tod;
    char value[1024];
    time_t ttod;
    int i;

    TTMFCN_BEGIN(ttm,frame,result);
    stod = frame->argv[1];
    if(1 != sscanf((const char*)stod,"%lld",&tod)) EXIT(TTM_EDECIMAL);
    tod = tod/100; /* need seconds */
    ttod = (time_t)tod;
    snprintf(value,sizeof(value),"%s",ctime(&ttod));
    /* ctime adds a trailing new line; remove it */
    i = (int)strlen(value);
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
	    if(fcn != NULL) fcn->fcn.trace = TR_OFF;
	}
    } else { /* turn off global tracing */
	ttm->debug.trace = TR_OFF;
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
	    if(fcn != NULL) fcn->fcn.trace = TR_ON;
	}
    } else { /* turn on global tracing but possibly delayed*/
	ttm->debug.trace = TR_ON;
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
    const char* arg;

    TTMFCN_BEGIN(ttm,frame,result);
    if((1 != sscanf((const char*)frame->argv[1],"%lld",&index))) EXIT(TTM_EDECIMAL);
    if(index < 0) EXIT(TTM_ERANGE);
    if(((size_t)index) < vllength(argoptions)) {
	if(ttm->opts.testing && index == 0) {
	    arg = fixedtestvalues.argv0;
	} else {
	    arg = vlget(argoptions,(size_t)index);
	}
        arglen = (int)strlen(arg);
        vsappendn(result,arg,arglen);
    }
    
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
    argc = (int)vllength(argoptions);
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
    size_t i;
    VList* classes = NULL;
    char** contents = NULL;

    TTMFCN_BEGIN(ttm,frame,result);

    /* Now collect all the classes */
    /* Note the reason we collect the classes is because we need to sort them */
    classes = vlnew();
    if(classes == NULL) EXIT(TTM_EMEMORY);
    for(i=0;i<HASHSIZE;i++) {
	struct HashEntry* entry = ttm->tables.charclasses.table[i].next;
	while(entry != NULL) {
	    Charclass* class = (Charclass*)entry;
	    vlpush(classes,class->entry.name);
	    entry = entry->next;
	}
    }
    /* Quick sort */
    contents = (char**)vlcontents(classes);
    qsort((void*)contents, vllength(classes), sizeof(char*), stringveccmp);

    /* print classes comma separated */
    for(i=0;i<vllength(classes);i++) {
	const char* nm = (const char*)vlget(classes,i);
	if(i > 0) vsappend(result,',');
	vsappendn(result,nm,strlen(nm));
    }

done:
    vlfree(classes);
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
	    if(fcn != NULL && !fcn->fcn.builtin) fcn->fcn.locked = 1;
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
	    if(fcn != NULL && !fcn->fcn.builtin) fcn->fcn.locked = 0;
	}
    }
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_istn(TTM* ttm, Frame* frame, VString* result) /* return current global trace state */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    const char* value = NULL;

    TTMFCN_BEGIN(ttm,frame,result);
    switch (ttm->debug.trace) {
    case TR_UNDEF: value = "0";  break;
    case TR_OFF:   value = "0"; break;
    case TR_ON:    value = "1"; break;
    }
    vsappendn(result,(const char*)value,strlen((const char*)value));
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_pn(TTM* ttm, Frame* frame, VString* result)  /* pass n chars of a string */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    const char* str = NULL;
    size_t n,len;

    TTMFCN_BEGIN(ttm,frame,result);
    if(1!=sscanf(frame->argv[1],"%zu",&n)) EXIT(TTM_EDECIMAL);
    str = frame->argv[2];    
    /* convert from code points */
    n = cptorp(ttm,str,n);
    len = strlen(str);
    if(len <= n) goto done; /* result is empty string */
    vsappendn(result,str+n,len - n);
done:
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static char*
trim(const char* s0, const char* ws)
{
    char* s = strdup(s0);
    char* p;
    char* q;
    size_t len;

    /* trim left first */    
    len = strlen(s);
    if(len == 0) goto done;
    for(p=s;*p;p++) {if(strchr(ws,*p) == NULL) break;}
    for(q=s+len;q>=s;q--) {if(strchr(ws,*q) == NULL) {*++q = '\0'; break;}}
    memmovex(s,p,strlen(p)+1);
done:
    return s;
}

static TTMERR
ttm_trim(TTM* ttm, Frame* frame, VString* result) /* right and left trim argv[1]; optionally use argv[2] as whitespace */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    const char* ws = NULL;
    const char* arg = NULL;
    char* trimmed = NULL;

    TTMFCN_BEGIN(ttm,frame,result);
    switch (frame->argc) {
    case 0: case 1:  EXIT(TTM_EFEWPARMS);
    case 2: arg = frame->argv[2]; ws = WHITESPACE;
    default: 
	arg = frame->argv[2];
	ws = frame->argv[3];
	break;
    }
    trimmed = trim(arg,ws);    
    if(strlen(trimmed) > 0)
	vsappendn(result,trimmed,strlen(trimmed));
done:
    nullfree(trimmed);
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

/**
Form: #<catch;<expr>>
*/
static TTMERR
ttm_catch(TTM* ttm, Frame* frame, VString* result) /* evaluate ttm expression and return any error code */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    VString* active = NULL;
    VString* passive = NULL;
	
    TTMFCN_BEGIN(ttm,frame,result);
    /* save relevant TTM fields  */
    active = ttm->vs.active;
    passive = ttm->vs.passive;
    ttm->vs.active = vsnew();
    ttm->vs.passive = vsnew();
    vsappendn(ttm->vs.active,frame->argv[1],strlen(frame->argv[1]));
    vsindexset(ttm->vs.active,0);
    ttm->flags.catchdepth++;
    err = scan(ttm);
    ttm->flags.catchdepth--;
    if(err == TTM_NOERR) {
	/* pass the value of passive up */
	vsappendn(result,vscontents(ttm->vs.passive),vslength(ttm->vs.passive));
    } else {
	char msg[1024];
	snprintf(msg,sizeof(msg),"<%s;%d>",ttmerrname(err),err);
	/* clear passive and pass the error name and value up. */
	vsclear(result);
	vsappendn(result,msg,strlen(msg));
	err = TTM_NOERR;
    }

/*done:*/
    /* Reset */
    vsfree(ttm->vs.active);
    ttm->vs.active = active;
    vsfree(ttm->vs.passive);
    ttm->vs.passive = passive;
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

/**
Form: #<switch;testvalue;default;key1;value1;key2;value2;...>
If last value is missing, then treat like "".
The test value and all keys are trimmed before comparison
*/
static TTMERR
ttm_switch(TTM* ttm, Frame* frame, VString* result) /* return current global trace state */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    char* test = NULL;
    const char* value = NULL;
    const char* _default = NULL;
    size_t pair;
    int odd;

    TTMFCN_BEGIN(ttm,frame,result);
    if(frame->argc < 3) EXIT(TTM_EFEWPARMS);
    test = trim(frame->argv[1],WHITESPACE);
    _default = frame->argv[2];
    odd = ((frame->argc - 3) % 2) == 1;
    if(odd) {frame->argv[frame->argc] = strdup(""); frame->argc++;}
    for(pair=3;pair<frame->argc;pair+=2) {
	char* trkey = trim(frame->argv[pair],WHITESPACE);
	if(strcmp(trkey,test)==0) {
	    value = frame->argv[pair+1];
	    break;
	}
    }
    if(value == NULL)
        value = _default;
    vsappendn(result,(const char*)value,strlen((const char*)value));
done:
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_clearpassive(TTM* ttm, Frame* frame, VString* result) /* clear ttm->vs.passive */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);

    TTMFCN_BEGIN(ttm,frame,result);
    vsclear(ttm->vs.passive);
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_include(TTM* ttm, Frame* frame, VString* result)  /* Include text of a file */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    char* path;
    char realpath[4096]; /* if testing */
    char* baseseg = NULL;

    TTMFCN_BEGIN(ttm,frame,result);
    if(strlen(frame->argv[1])==0) EXIT(TTM_EINCLUDE);
    path = strdup(frame->argv[1]);

    if(ttm->opts.testing) {
	char* p;
    /* Convert '\\' to '/' */
    for (p = path; *p; p++) {if (*p == '\\') *p = '/';}
	/* Get the basefile of path */
	p = strrchr(path,'/');
	if(p == NULL) { /* point to base segment */
	    baseseg = strdup(path);
	    *p = '\0'; /* remove base file from path */
	} else {
	    baseseg = strdup(p); /* include leading '/' */
	    *p = '\0';  /* remove base file from path */
	}
	/* Get the current directory */
	if(getcwd(realpath, sizeof(realpath))==NULL) EXIT(TTM_EMEMORY);
    /* Convert '\\' to '/' */
    for (p = realpath; *p; p++) { if (*p == '\\') *p = '/'; }
	/* If last directory is '/Windows' then remove it from path */
	p = strrchr(realpath,'/');
	if(p == NULL) p = realpath;
	if(strcmp(p,LOCALWINSEG)==0) *p = '\0';
	/* append the base file segment */
	strcat(realpath,baseseg);
#ifdef MSWINDOWS
    for (p = realpath; *p; p++) { if (*p == '/') *p = '\\'; }
#endif
    } else {
        strcpy(realpath,frame->argv[1]);
    }
    if(strlen(realpath) == 0) EXIT(TTM_EINCLUDE);
    readfile(ttm,realpath,ttm->vs.tmp);
    vsappendn(result,vscontents(ttm->vs.tmp),vslength(ttm->vs.tmp));
    vsclear(ttm->vs.tmp);
done:
    nullfree(baseseg);
    nullfree(path);
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

static TTMERR
ttm_wd(TTM* ttm, Frame* frame, VString* result) /* return current working directory */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    char wd[2048];
    
    TTMFCN_BEGIN(ttm,frame,result);
    if(ttm->opts.testing) {
	strncpy(wd,fixedtestvalues.wd,sizeof(wd));
    } else {
	wd[0] = '\0';
	if(getcwd(wd, sizeof(wd))==NULL) EXIT(TTM_EMEMORY);
    }
    vsappendn(result,wd,strlen(wd));
done:
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_fps(TTM* ttm, Frame* frame, VString* result) /* return current working directory */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    
    TTMFCN_BEGIN(ttm,frame,result);
    if(ttm->opts.testing)
        vsappendn(result,"/",1);
    else
#ifdef MSWINDOWS
	vsappendn(result,"\\",1);
#else
	vsappendn(result,"/",1);
#endif

    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_breakpoint(TTM* ttm, Frame* frame, VString* result) /* Throw away all arguments */
{
    TTMERR err = TTM_NOERR;
    return THROW(err);
}

/** Properties functions */
static TTMERR
ttm_setprop(TTM* ttm, Frame* frame, VString* result) /* Set specified property */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    const char* key = NULL;
    const char* value = NULL;

    TTMFCN_BEGIN(ttm,frame,result);
    switch(frame->argc) {
    case 0: case 1: EXIT(TTM_EFEWPARMS);
    case 2:
        key = frame->argv[1];
	value = "";
	break;
    default:
        key = frame->argv[1];
	value = frame->argv[2];
	break;
    }
    setproperty(ttm,key,value);
done:
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_resetprop(TTM* ttm, Frame* frame, VString* result) /* Set specified property back to default; return old value */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    char* key = NULL;
    const char* value = NULL;
    enum PropEnum propkey;
    size_t propdefault;

    TTMFCN_BEGIN(ttm,frame,result);
    if(frame->argc < 2) EXIT(TTM_EFEWPARMS);
    key = frame->argv[1];

    /* get old value */
    value = propertyLookup(ttm,key);
    if(value != NULL)
        vsappendn(result,value,strlen(value));
    
    propkey = propenumdetect(key);
    if(propkey != PE_UNDEF) {
        propdefault = propdfalt(propkey);
        value = propdfalt2str(propkey,propdefault);
        if(value != NULL)
	    setproperty(ttm,key,value);
    }
done:
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_getprop(TTM* ttm, Frame* frame, VString* result) /* Get specified property  */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    char* key = NULL;
    const char* value = NULL;
    
    TTMFCN_BEGIN(ttm,frame,result);
    if(frame->argc < 1) EXIT(TTM_EFEWPARMS);
    key = frame->argv[1];
    value = propertyLookup(ttm,key);
    if(value != NULL)
        vsappendn(result,value,strlen(value));

done:
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_removeprop(TTM* ttm, Frame* frame, VString* result) /* remove specified property */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    char* key = NULL;
    
    TTMFCN_BEGIN(ttm,frame,result);
    if(frame->argc < 1) EXIT(TTM_EFEWPARMS);
    key = frame->argv[1];
    (void)propertyRemove(ttm,key);
done:
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

static TTMERR
ttm_properties(TTM* ttm, Frame* frame, VString* result) /* Obtain all property names */
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    size_t i;
    VList* props = NULL;
    char** contents = NULL;

    TTMFCN_BEGIN(ttm,frame,result);

    /* Now collect all the properties */
    /* Note the reason we collect them is because we need to sort them */
    props = vlnew();
    if(props == NULL) EXIT(TTM_EMEMORY);
    for(i=0;i<HASHSIZE;i++) {
	struct HashEntry* entry = ttm->tables.properties.table[i].next;
	while(entry != NULL) {
	    Property* prop = (Property*)entry;
	    vlpush(props,prop->entry.name);
	    entry = entry->next;
	}
    }
    /* Quick sort */
    contents = (char**)vlcontents(props);
    qsort((void*)contents, vllength(props), sizeof(char*), stringveccmp);

    /* print props comma separated */
    for(i=0;i<vllength(props);i++) {
	const char* nm = (const char*)vlget(props,i);
	if(i > 0) vsappend(result,',');
	vsappendn(result,nm,strlen(nm));
    }

done:
    vlfree(props);
    TTMFCN_END(ttm,frame,result);
    return THROW(err);
}

/**************************************************/
/**
Sort helpers
*/

/**
Sort a the content of a named string where the elements to sort are separated with sep and trimmed of blanks
*/
static TTMERR
ttm_sort(TTM* ttm, Frame* frame, VString* result)
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    const char* p8 = NULL;
    const char* q8 = NULL;
    utf8cpa sep8;
    VList* elems = vlnew();
    Function* name;
    int seplen;
    size_t i;

    TTMFCN_BEGIN(ttm,frame,result);
    if(frame->argc < 2) EXIT(TTM_EFEWPARMS);
    if(frame->argc > 2) {
	char* a2 = (char*)frame->argv[2];
        memmovex((char*)sep8,a2,u8size(a2));
    } else
        sep8[0] = ',';
    seplen = u8size(sep8);
    assert(seplen > 0);
    name = dictionaryLookup(ttm,frame->argv[1]);
    if(name == NULL) EXIT(TTM_ENONAME);
    p8 = vscontents(name->fcn.body);
    if(p8 == NULL || strlen((char*)p8)==0) goto done;
    /* parse the input string */
    for(;;) {
	size_t len;
	char* elem = NULL;
	q8 = strchr8(p8,sep8);
	if(q8 == NULL) q8 = (p8 + strlen(p8)); /* last element */
	len = (q8 - p8);
	elem = malloc(len+1);
	memcpy(elem,p8,len);
	elem[len] = NUL8;
	vlpush(elems,elem); elem = NULL;
	if(isnul(q8)) break; /* last element */
	p8 = (q8 + seplen);
    }
    /* sort the elements */
    {
	void* elist= (void*)vlcontents(elems);
	qsort(elist, vllength(elems), sizeof(char*), stringveccmp);
    }
    /* convert to result */
    vsclear(ttm->vs.tmp);
    for(i=0;i<vllength(elems);i++) {
	const char* el = (const char*)vlget(elems,i);
	if(i > 0) vsappendn(ttm->vs.tmp,sep8,u8size(sep8));
	vsappendn(ttm->vs.tmp,el,strlen(el));
    }
    /* replace old body content */
    vsclear(name->fcn.body);
    vsindexset(name->fcn.body,0);
    vsappendn(name->fcn.body,vscontents(ttm->vs.tmp),vslength(ttm->vs.tmp));
    /* cleanup */
    vsclear(ttm->vs.tmp);

done:
    vlfreeall(elems);
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
If char is empty, then no change is made.
Return previous value of which.
*/
static TTMERR
ttm_ttm_meta(TTM* ttm, Frame* frame, VString* result)
{
    TTMERR err = TTM_NOERR;
    TTMFCN_DECLS(ttm,frame);
    char* which = NULL;
    char* replacement = NULL;
    utf8cpa prev = empty_u8cpa;

    TTMFCN_BEGIN(ttm,frame,result);
    if(frame->argc < 4) EXIT(TTM_EFEWPARMS);
    which = frame->argv[2];
    replacement = frame->argv[3];

    if(u8size(replacement) <= 0) EXIT(TTM_EUTF8);
    switch (metaenumdetect(which)) {
    case ME_SHARP:
	memcpycp(prev,ttm->meta.sharpc);
	if(strlen(replacement) > 0) memcpycp(ttm->meta.sharpc,replacement);
	break;
    case ME_SEMI:
	memcpycp(prev,ttm->meta.semic);
	if(strlen(replacement) > 0) memcpycp(ttm->meta.semic,replacement);
	break;
    case ME_ESCAPE:
	memcpycp(prev,ttm->meta.escapec);
	if(strlen(replacement) > 0) memcpycp(ttm->meta.escapec,replacement);
	break;
    case ME_META:
	memcpycp(prev,ttm->meta.metac);
	if(strlen(replacement) > 0) memcpycp(ttm->meta.metac,replacement);
	break;
    case ME_OPEN:
	memcpycp(prev,ttm->meta.openc);
	if(strlen(replacement) > 0) memcpycp(ttm->meta.openc,replacement);
	break;
    case ME_CLOSE:
	memcpycp(prev,ttm->meta.closec);
	if(strlen(replacement) > 0) memcpycp(ttm->meta.closec,replacement);
	break;
    case ME_LBR:
	memcpycp(prev,ttm->meta.lbrc);
	if(strlen(replacement) > 0) memcpycp(ttm->meta.lbrc,replacement);
	break;
    case ME_RBR:
	memcpycp(prev,ttm->meta.rbrc);
	if(strlen(replacement) > 0) memcpycp(ttm->meta.rbrc,replacement);
	break;
    default: err = TTM_ETTM; goto done;
    }
    vsappendn(result,prev,u8size(prev));
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
    char* arg = NULL;
    size_t arglen = 0;
    char* cleaned = NULL;
    size_t ncleaned = 0;
    int nargs;

    if(frame->argc < 4) EXIT(TTM_EFEWPARMS);
    arg = frame->argv[3];
    arglen = strlen(arg);
    str = dictionaryLookup(ttm,arg);
    vsappendn(result,ttm->meta.lbrc,u8size(ttm->meta.lbrc));
    if(str == NULL) { /* not defined*/
	vsappendn(result,arg,arglen);
	vsappendn(result,ttm->meta.semic,u8size(ttm->meta.semic));
	vsappend(result,'-');
	vsappendn(result,ttm->meta.semic,u8size(ttm->meta.semic));
	vsappend(result,'-');
	vsappendn(result,ttm->meta.semic,u8size(ttm->meta.semic));
	vsappend(result,'-');
	vsappendn(result,ttm->meta.semic,u8size(ttm->meta.semic));
	vsappend(result,'0');
	goto done;
    }
    /* assert(str != NULL) */
    vsappendn(result,(const char*)str->entry.name,strlen((const char*)str->entry.name));
    if(str->fcn.builtin) {
	nargs = (int)str->fcn.maxargs;
    } else {/*!str->fcn.builtin*/
	nargs = (int)str->fcn.nextsegindex-1;
    }
    snprintf(info,sizeof(info),",%zu",str->fcn.minargs);
    vsappendn(result,info,strlen(info));
    vsappendn(result,ttm->meta.semic,u8size(ttm->meta.semic));
    if(str->fcn.maxargs==ARB)
	strcpy(info,"*");
    else
	snprintf(info,sizeof(info),"%d",nargs);
    vsappendn(result,info,strlen(info));
    vsappendn(result,ttm->meta.semic,u8size(ttm->meta.semic));
    vsappendn(result,sv(str),0);
    vsappendn(result,ttm->meta.semic,u8size(ttm->meta.semic));
    snprintf(info,sizeof(info),"locked=%d",str->fcn.locked);
    vsappendn(result,info,strlen(info));
    if(!str->fcn.builtin) {
	size_t rp = vsindex(str->fcn.body);
	snprintf(info,sizeof(info),"segindex=%zu",str->fcn.nextsegindex);
	vsappendn(result,ttm->meta.semic,u8size(ttm->meta.semic));
	vsappendn(result,info,strlen(info));
	vsappendn(result,ttm->meta.semic,u8size(ttm->meta.semic));
	rp = rptocp(ttm,vscontents(str->fcn.body),vsindex(str->fcn.body));
	snprintf(info,sizeof(info),"residual=%zu",rp);
	vsappendn(result,info,strlen(info));
	vsappendn(result,ttm->meta.semic,u8size(ttm->meta.semic));
	vsappendn(result,"body=",strlen("body="));
	vsappendn(result,ttm->meta.lbrc,u8size(ttm->meta.lbrc));
	cleaned = cleanstring(vscontents(str->fcn.body),NULL,&ncleaned);
	vsappendn(result,(const char*)cleaned,ncleaned);
	vsappendn(result,ttm->meta.rbrc,u8size(ttm->meta.rbrc));
    }
    vsappendn(result,ttm->meta.rbrc,u8size(ttm->meta.rbrc));
done:
    nullfree(cleaned);
    return THROW(err);
}

/**
#<ttm;info;class;{classname}>
*/
static TTMERR
ttm_ttm_info_class(TTM* ttm, Frame* frame, VString* result) /* Misc. combined actions */
{
    TTMERR err = TTM_NOERR;
    Charclass* cl;
    char* p;
    int ncp;
    const char* arg = NULL;
    size_t arglen;

    if(frame->argc < 4) EXIT(TTM_EFEWPARMS);
    arg = frame->argv[3];
    arglen = strlen((const char*)arg);
    cl = charclassLookup(ttm,arg);
    if(cl == NULL) EXIT(TTM_ENOCLASS);
    vsappendn(result,ttm->meta.lbrc,u8size(ttm->meta.lbrc));
    vsappendn(result,arg,arglen);
    vsappendn(result,ttm->meta.semic,u8size(ttm->meta.semic));
    vsappend(result,LBRACKET);
    if(cl->negative) vsappend(result,'^');
    for(p=cl->characters;*p;p+=ncp) {
	ncp = u8size(p);
	if(ncp == 1 && (*p == LBRACKET || *p == RBRACKET)) {
	    vsappendn(result,ttm->meta.escapec,u8size(ttm->meta.escapec));
	    vsappendn(result,p,ncp);
	    p += ncp;
	}
	vsappendn(result,p,ncp);
    }
    vsappend(result,RBRACKET);
    vsappendn(result,ttm->meta.rbrc,u8size(ttm->meta.rbrc));
#if DEBUG > 0
    xprintf(ttm,"info.class: |%s|\n",vscontents(result));
#endif
done:
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
    char* arg = NULL;

    if(frame->argc < 4) EXIT(TTM_EFEWPARMS);
    arg = frame->argv[3];
    str = dictionaryLookup(ttm,arg);
    /* Surround the body with <...> */
    vsappendn(result,(const char*)ttm->meta.lbrc,u8size(ttm->meta.lbrc));
    if(str != NULL ||! str->fcn.builtin) {
	vsappendn(result,vscontents(str->fcn.body),vslength(str->fcn.body));
    } else { /* Use "" */
	/* append nothing */
    }
done:
    vsappendn(result,(const char*)ttm->meta.rbrc,u8size(ttm->meta.rbrc));
    return THROW(err);
}

/**
#<ttm;list;{case}>
#<ttm;list;{case}>
Where case is one of:
"all" => list all names
"builtin" => list only builtin names
"string" => list only user-defined names
"class" => list all classes 
Each result is ',' separated.
*/

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

    if(frame->argc < 3) EXIT(TTM_EFEWPARMS);

    /* Figure out what we are collecting */
    switch (tte = ttmenumdetect(frame->argv[2])) {
    case TE_CLASS: table = &ttm->tables.charclasses; break;
    case TE_ALL: case TE_BUILTIN: case TE_STRING: table = &ttm->tables.dictionary; break;
    default: EXIT(TTM_EINVAL);
    }

    /* Pass one: collect all the names */
    walker = hashwalk(table);
    while(hashnext(walker,&entry)) {
	Function* fcn = NULL;
	Charclass* cl = NULL;
	switch (tte) {
	case TE_ALL:
	    fcn = (Function*)entry;
	    vlpush(vl,strdup((const char*)fcn->entry.name));
	    break;
	case TE_BUILTIN:
	    fcn = (Function*)entry;
	    if(fcn->fcn.builtin) vlpush(vl,strdup((const char*)fcn->entry.name));
	    break;
	case TE_STRING:
	    fcn = (Function*)entry;
	    if(!fcn->fcn.builtin) vlpush(vl,strdup((const char*)fcn->entry.name));
	    break;
	case TE_CLASS:
	    cl = (Charclass*)entry;
	    vlpush(vl,strdup((const char*)cl->entry.name));
	    break;
	default: EXIT(TTM_EINVAL);
	}
    }
    hashwalkstop(walker);
    /* construct the final result */
    vsappendn(result,(const char*)ttm->meta.lbrc,u8size(ttm->meta.lbrc));
    for(first=1,i=0;i<vllength(vl);i++,first=0) {
	const char* name = (const char*)vlget(vl,i);
	if(!first) vsappendn(result,",",1);
	vsappendn(result,name,strlen(name));
    }
    vsappendn(result,(const char*)ttm->meta.rbrc,u8size(ttm->meta.rbrc));
done:
    return THROW(err);
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
    char* which = NULL;
    char* klass = NULL;

    TTMFCN_BEGIN(ttm,frame,result);
    if(frame->argc < 2) EXIT(TTM_EFEWPARMS);
    which = frame->argv[1];
    switch (ttmenumdetect(which)) {
    case TE_META:
	err = ttm_ttm_meta(ttm,frame,result);
	break;
    case TE_INFO:
	if(frame->argc < 3) EXIT(TTM_EFEWPARMS);
	klass = frame->argv[2];
	switch (ttmenumdetect(klass)) {
	case TE_NAME:
	    err = ttm_ttm_info_name(ttm,frame,result);
	    break;
	case TE_CLASS:
	    err = ttm_ttm_info_class(ttm,frame,result);
	    break;
	case TE_STRING:
	    err = ttm_ttm_info_string(ttm,frame,result);
	    break;
	default: EXIT(TTM_ETTMCMD);
	}
	break;
    case TE_LIST:
	err = ttm_ttm_list(ttm,frame,result);
	break;
    default:
	EXIT(TTM_ETTMCMD);
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
    {"ds",2,2,SV_S,ttm_ds}, /* Define string */
    {"es",0,ARB,SV_S,ttm_es}, /* Erase string */
    {"sc",2,63,SV_SV,ttm_sc}, /* Segment and count */
    {"ss",2,2,SV_S,ttm_ss}, /* Segment a string */
    {"cr",2,2,SV_S,ttm_cr}, /* Mark for creation */
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
    {"cm",1,1,SV_SV,ttm_cm}, /*Change Meta Character; return previous value*/
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
    {"ctime",1,1,SV_V,ttm_ctime}, /* Convert arg1 (typically #<time>) to printable string */
    {"include",1,1,SV_V,ttm_include}, /* Include text of a file */
    {"lf",0,ARB,SV_S,ttm_lf}, /* Lock functions */
    {"uf",0,ARB,SV_S,ttm_uf}, /* Unlock functions */
    {"ttm",1,ARB,SV_SV,ttm_ttm}, /* Misc. combined actions */
    {"ge",4,4,SV_V,ttm_ge}, /* Compare numeric greater-than */
    {"le",4,4,SV_V,ttm_le}, /* Compare numeric less-than */
    {"void",0,ARB,SV_S,ttm_void}, /* throw away all arguments and return an empty string */
    {"comment",0,ARB,SV_S,ttm_void}, /* alias for ttm_void */
    {"setprop",1,2,SV_SV,ttm_setprop}, /* Set property */
    {"resetprop",1,1,SV_SV,ttm_resetprop}, /* set property to default */
    {"getprop",1,1,SV_SV,ttm_getprop}, /* get property value */
    {"removeprop",1,1,SV_SV,ttm_removeprop}, /* remove property value */
    {"properties",0,0,SV_V,ttm_properties }, /* list all property keys in form <key,...>*/
    {"printf",1,ARB,SV_S,ttm_printf}, /* Emulate printf() */
    {"fprintf",2,ARB,SV_S,ttm_fprintf}, /* Emulate fprintf() */
    {"pf",0,1,SV_S,ttm_pf}, /* flush stderr and/or stdout */
    {"tru",1,1,SV_V,ttm_tru}, /* Translate to uppercase */
    {"tdh",1,1,SV_V,ttm_tdh}, /* Convert a decimal value to hexidecimal */
    {"rp",1,1,SV_V,ttm_rp}, /* return the value of the residual pointer */
    {"srp",1,2,SV_S,ttm_srp}, /* set the value of the residual pointer */
    {"sort",1,2,SV_S,ttm_sort}, /* sort the contents of a named string */
    {"tn?",0,0,SV_V,ttm_istn}, /* Return 1 if trace is on; 0 if false and "" if undefined */
    {"pn",2,2,SV_V,ttm_pn}, /* pass arg[1] chars and return all but first arg[1] characters of arg[2] */
    {"trim",1,2,SV_V,ttm_trim}, /* trim leading and trailing whitespace */
    {"breakpoint",0,0,SV_S,ttm_breakpoint},
    {"catch",1,1,SV_SV,ttm_catch}, /* evaluate a TTM expression and return any error code */
    {"switch",1,ARB,SV_V,ttm_switch}, /* multiway conditional */
    {"wd",0,0,SV_V,ttm_wd}, /* get current working directory */
    {"fps",0,0,SV_V,ttm_fps}, /* platform specific file path separator */
    {"clearpassive",0,0,SV_S,ttm_clearpassive}, /* clear current passive results */
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
    fcn = dictionaryLookup(ttm,bin->name);
    if(fcn != NULL) FAILX(ttm,TTM_EDUPNAME,"fcn=%s\n",bin->name);
    /* create a new function object */
    fcn = newFunction(ttm,bin->name);
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
