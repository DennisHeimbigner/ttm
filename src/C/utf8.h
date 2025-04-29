/**
Copy a single codepoint from src to dst
@param dst target for the codepoint
@param src src for the codepoint
@return no. of bytes in codepoint
*/
static int
memcpycp(utf8* dst, const utf8* src)
{
    int count = u8size(src);
    memcpy(dst,src,count);
    return count;
}

static int
u8sizec(utf8 c)
{
    if(c == SEGMARK0) return SEGMARKSIZE; /* segment|create mark */
    if((c & 0x80) == 0x00) return 1; /* us-ascii */
    if((c & 0xE0) == 0xC0) return 2;
    if((c & 0xF0) == 0xE0) return 3;
    if((c & 0xF8) == 0xF0) return 4;
    return -1;
}

static int
u8size(const utf8* cp)
{
    return u8sizec(*cp);
}

/* return no. bytes in codepoint or -1 if codepoint invalid */
/* We use a simplified test for validity */
static int
u8validcp(utf8* cp)
{
    int ncp = u8size(cp);
    int i;
    if(ncp == 0) return -1;
    /* Do validity check */
    /* u8size checks validity of first byte */
    for(i=1;i<ncp;i++) {
	if((cp[i] & 0xC0) != 0x80) return -1; /* bad continuation char */
    }
    return ncp;
}

static void
ascii2u8(char c, utf8* u8)
{
    *u8 = (utf8)(0x7F & (unsigned)c);
}

static int
u8equal(const utf8* c1, const utf8* c2)
{
    int l1 = u8size(c1);
    int l2 = u8size(c2);
    if(l1 != l2) return 0;
    if(memcmp((void*)c1,(void*)c2,l1)!=0) return 0;
    return 1;
}

/**
Compute a pointer to base[n], where base is in units
of codepoints.	if length of base in codepoints is less than n
(i.e. we hit EOS), then return ptr to EOS.
@param base start of the codepoint vector
@param n offset from base in units of codepoints
@return pointer to base[n] in units of codepoints or ptr to EOS.
*/
static const utf8*
u8ithcp(const utf8* base, size_t n)
{
    const utf8* p = NULL;
    size_t nbytes = strlen((const char*)base); /* bytes as opposed to codepoints */
    for(p=base;n > 0; n--) {
	int ncp = u8size(p);
	if(isnul(p)) {break;} /* hit EOS */
	if(ncp < 0 || nbytes < (size_t)ncp) return NULL; /* bad codepoint */
	p += ncp; /* skip across this codepoint */
	nbytes -= ncp; /* track no. of remaining bytes in src */
    }
    return p;
}

/**
Compute the position of base[n] in bytes, where base is in units
of codepoints.	if length of base in codepoints is less than n
(i.e. we hit EOS), then return position of the EOS.
@param base start of the codepoint vector
@param n offset from base in units of codepoints
@return offset (in bytes) of base[n] from base in units of codepoints or -1 if malformed
*/
static int
u8ith(const utf8* base, size_t n)
{
    const utf8* p = u8ithcp(base,n);
    return (p==NULL?-1:(int)(p - base));
}


/**
Given a utf8 pointer, backup one codepoint.  This is doable
because we can recognize the start of a codepoint.

Special case is required when backing up over a segment mark since it assumes
that the raw SEGMARK is at the beginning of the mark.

@param p0 pointer to back up
@return pointer to codepoint before p0
*/
static const utf8*
u8backup(const utf8* p, const utf8* base)
{
    while(p-- > base) {
        if((*p & 0xB0) != 0xB0) break; /* backup over all continuation bytes */
    }
    /* we should be at the start of the codepoint or segmark */
    return p;
}

#if 0
/**
Count no. of codepoints in src to src+n or EOS.
This is sort of the inverse of u8ith().
@param dst target for the codepoint
@param src src for the codepoint
@param n check in src to src+n
@return no. of codepoints or -1 if malformed
*/
static int
u8cpcount(const utf8* src, size_t n)
{
    size_t nbytes = strlen((const char*)src); /* bytes as opposed to codepoints */
    int cpcount = 0;
    size_t offset = 0;
    if(nbytes > n) nbytes = n;
    for(offset=0;offset < nbytes || isnul(src);) {
	int ncp = u8size(src);
	if(ncp < 0) return -1;
	if((offset + (unsigned)ncp) > nbytes) return -1; /* bad codepoint */
	src += ncp;
	offset += ncp;
	cpcount++;
    }
    if(offset < nbytes) return -1; /* extraneous extra bytes at end */
    return cpcount;
}
#endif

/**
Count number of codepoints in a sub-string of a string
@param sstart beginning of substring
@param slen length (in bytes) of substring sstart => search sstart[0..send-1]
@param np return no. of codepoints in substring
@return TTMERR; especially TTM_EEOS if eos encountered during codeppoint scan
*/
static TTMERR
strsubcp(const utf8* sstart, size_t slen, size_t* pnc)
{
    TTMERR err = TTM_NOERR;
    size_t ncodepoints = 0;
    const utf8* p = sstart;
    const utf8* q = p + slen;
    for(ncodepoints=0;p < q;ncodepoints++) {
	int ncp = u8size(p);
	if(ncp <= 0) {err = TTM_EUTF8; goto done;}
	if(isnul(p)) {err = TTM_EEOS; goto done;}
	p += ncp;
    }
    if(pnc) *pnc = ncodepoints;
done:
    return err;
}

/**
Peek at the nth char starting at s.
Return that char.
If EOS is encountered, then no advancement occurs
@param s string
@param n number of codepoints to peek ahead
@param cpa store the peek'd codepoint
@return TTMERR
*/
static TTMERR
u8peek(utf8* s, size_t n, utf8* cpa)
{
    TTMERR err = TTM_NOERR;
    size_t i;
    utf8* p = s;
    int ncp = 0;

    for(i=0;i<n;i++) {
	if(isnul(p)) break;
	ncp = u8size(p);	
	p += ncp;
    }
    memcpy(cpa,p,u8size(p));
    return err;
}
