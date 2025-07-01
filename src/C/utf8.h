/**
Copy a single codepoint from src to dst
@param dst target for the codepoint
@param src src for the codepoint
@return no. of bytes in codepoint
*/
static int
memcpycp(char* dst, const char* src)
{
    int count = u8size(src);
    memcpy(dst,src,count);
    return count;
}

static int
u8sizec(char cc)
{
    utf8 c = (utf8)cc;
    if(c == SEGMARK0) return SEGMARKSIZE; /* segment|create mark */
    if((c & 0x80) == 0x00) return 1; /* us-ascii */
    if((c & 0xE0) == 0xC0) return 2;
    if((c & 0xF0) == 0xE0) return 3;
    if((c & 0xF8) == 0xF0) return 4;
    return -1;
}

static int
u8size(const char* cp)
{
    return u8sizec(*cp);
}

/* return no. bytes in codepoint or -1 if codepoint invalid */
/* We use a simplified test for validity */
static int
u8validcp(char* cp)
{
    int ncp = u8size(cp);
    int i;
    utf8* cp8 = (utf8*)cp;
    if(ncp == 0) return -1;
    /* Do validity check */
    /* u8size checks validity of first byte */
    for(i=1;i<ncp;i++) {
	if((cp8[i] & 0xC0) != 0x80) return -1; /* bad continuation char */
    }
    return ncp;
}

static void
ascii2u8(char c, char* u8)
{
    *u8 = (char)(0x7F & (unsigned)c);
}

static int
u8equal(const char* c1, const char* c2)
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
static const char*
u8ithcp(const char* base, size_t n)
{
    const char* p = NULL;
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
u8ith(const char* base, size_t n)
{
    const char* p = u8ithcp(base,n);
    return (p==NULL?-1:(int)(p - base));
}


/**
Given a char pointer, backup one codepoint.  This is doable
because we can recognize the start of a codepoint.

Special case is required when backing up over a segment mark since it assumes
that the raw SEGMARK is at the beginning of the mark.

@param p pointer to back up
@param base do not backup further than this ptr
@return pointer to codepoint before p
*/
static const char*
u8backup(const char* p, const char* base)
{
    while(p-- > base) {
        if((UTF8(*p) & 0xB0) != 0xB0) break; /* backup over all continuation bytes */
    }
    /* we should be at the start of the codepoint or segmark */
    return p;
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
u8peek(char* s, size_t n, char* cpa)
{
    TTMERR err = TTM_NOERR;
    size_t i;
    char* p = s;
    int ncp = 0;

    for(i=0;i<n;i++) {
	if(isnul(p)) break;
	ncp = u8size(p);	
	p += ncp;
    }
    memcpy(cpa,UTF8P(p),u8size(p));
    return err;
}

/**
Count number of codepoints in a sub-string of a string
@param sstart beginning of substring
@param slen length (in bytes) of substring sstart => search sstart[0..send-1]
@param np return no. of codepoints in substring
@return TTMERR; especially TTM_EEOS if eos encountered during codeppoint scan
*/
static TTMERR
strsubcp(const char* sstart, size_t slen, size_t* pnc)
{
    TTMERR err = TTM_NOERR;
    size_t ncodepoints = 0;
    const char* p = sstart;
    const char* q = p + slen;
    for(ncodepoints=0;p < q;ncodepoints++) {
	int ncp = u8size(p);
	if(ncp <= 0) EXITX(TTM_EUTF8);
	if(isnul(p)) EXITX(TTM_EEOS);
	p += ncp;
    }
    if(pnc) *pnc = ncodepoints;
done:
    return err;
}

/**
Equivalent of strchr except that the char can be a UTF-8 codepoint
and s can contain UTF-8 characters.
@param s string to search
@param pattern pointer to codepoint to search for
@return NULL if not found else ptr to the first found codepoint.
*/
static const char*
strchr8(const char* s, const char* pattern)
{
    const char* p = NULL;
    utf8cpa cp = empty_u8cpa;

    memcpycp(cp,pattern);
    p = strstr(s,cp);
    return p;
}

/**
Equivalent of strstr except that the args can be UTF-8.
@param s string to search
@param pattern pointer to string to search for.
@return NULL if not found else ptr to the first found instance of pattern
*/
static const char*
strstr8(const char* s, const char* pattern)
{
    const char* p = NULL;
    p = strstr(s,pattern);
    return p;
}
