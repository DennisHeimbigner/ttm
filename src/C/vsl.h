typedef struct VString {
  size_t alloc;
  size_t length;
  char* content;
  size_t index; /* 0 <= index < length */
} VString;

/* VString has a fixed expansion size */
#define VSTRALLOC 64

#if defined(_CPLUSPLUS_) || defined(__CPLUSPLUS__)
#define EXTERNC extern "C"
#else
#define EXTERNC extern
#endif

/**************************************************/
/* Forward */

static VString* vsnew(void);
static void vsfree(VString* vs);
static void vsexpand(VString* vs);
static void vssetalloc(VString* vs, size_t minalloc);
static void vssetlength(VString* vs, size_t newlen);
static void vsappendn(VString* vs, const char* elem, size_t n);
static void vsappend(VString* vs, char elem);
static int vsinsertn(VString* vs, size_t pos, const char* s, size_t slen);
static int vsremoven(VString* vs, size_t dstpos, size_t elide);
static char* vsextract(VString* vs);
static char* vsindexset(VString* vs, size_t pos);
static char* vsindexskip(VString* vs, size_t skip);
static size_t vsindex(VString* vs);
static char* vsindexp(VString* vs);
static int vsindexinsertn(VString* vs, const char* s, size_t slen);

/*************************/
/* "Inlined" */
#define vscontents(vs)  ((vs)==NULL?NULL:(vs)->content)
#define vslength(vs)  ((vs)==NULL?0:(vs)->length)
#define vsalloc(vs)  ((vs)==NULL?0:(vs)->alloc)
#define vscat(vs,s)  vsappendn(vs,s,0)
#define vsclear(vs)  vssetlength(vs,0)

/*************************/
/* "Inlined" */
#define vscontents(vs)  ((vs)==NULL?NULL:(vs)->content)
#define vslength(vs)  ((vs)==NULL?0:(vs)->length)
#define vsalloc(vs)  ((vs)==NULL?0:(vs)->alloc)
#define vscat(vs,s)  vsappendn(vs,s,0)
#define vsclear(vs)  vssetlength(vs,0)

/**************************************************/

static VString*
vsnew(void)
{
  VString* vs = NULL;
  vs = (VString*)calloc(1,sizeof(VString));
  assert(vs != NULL);
  return vs;
}

static void
vsfree(VString* vs)
{
  if(vs == NULL) return;
  if(vs->content != NULL) free(vs->content);
  free(vs);
}

static void
vsexpand(VString* vs)
{
  char* newcontent = NULL;
  size_t newsz;

  if(vs == NULL) return;
  /* Expand by (alloc == 0 ? 4 : 2 * alloc) */
  newsz = (vs->alloc == 0 ? 4 : (2 * vs->alloc));
//  newsz = (vs->alloc + VSTRALLOC); /* increase allocated space */
  /* Calling vsexpand always ensures that vs->content is non-null */
  if(vs->content != NULL && vs->alloc >= newsz) return; /* space already allocated */
  newcontent=(char*)calloc(1,newsz+1);/* always room for nul term */
  assert(newcontent != NULL);
  if(vs->alloc > 0
     && vs->length > 0
     && vs->content != NULL) /* something to copy */
    memcpy((void*)newcontent,(void*)vs->content,vs->length);
  newcontent[vs->length] = '\0'; /* ensure null terminated */
  if(vs->content != NULL) free(vs->content);
  vs->content=newcontent;
  vs->alloc=newsz;
  if(vs->index < 0) vs->index = 0;
  /* length is the same */  
}

static void
vssetalloc(VString* vs, size_t minalloc)
{
    while(vs->alloc < minalloc) vsexpand(vs);
}

static void
vssetlength(VString* vs, size_t newlen)
{
    size_t oldlen;
    assert(vs != NULL);
    oldlen = vs->length;
    if(newlen > oldlen) {
	vssetalloc(vs,newlen);
	vs->content[newlen] = '\0';
    }
    if(vs->index > newlen) vs->index = newlen;
    vs->length = newlen;
    /* Pretty'fy */
    if(vs->content != NULL && vs->alloc > vs->length)
	vs->content[vs->length] = '\0';
}

static void
vsappendn(VString* vs, const char* elem, size_t n)
{
  size_t need;
  assert(vs != NULL && elem != NULL);
  if(n == 0) {n = strlen(elem);}
  need = vs->length + n;
  vssetalloc(vs,need);
  memcpy(&vs->content[vs->length],elem,n);
  vs->length += n;
  vs->content[vs->length] = '\0'; /* guarantee nul term */
}

/**
Append a single byte.
@param vs
@param c the character to append
*/
static void
vsappend(VString* vs, char c)
{
  char s[2];
  s[0] = c;
  s[1] = '\0';
  vsappendn(vs,s,1);
}

/**
Insert s where |s|==n into vs at position pos.
@param vs
@param pos where to insert; if pos > |vs->contents| then expand vs.
@param s string to insert
@param slen no. of chars from s to insert
@return final total space
*/
static int
vsinsertn(VString* vs, size_t pos, const char* s, size_t slen)
{
    size_t totalspace = 0;
    size_t vslen = 0;

    assert(vs != NULL && s != NULL);
    if(slen == 0) {slen = strlen(s);}
    vslen = vslength(vs);
#if 0
initial: |len.........|
case 1:  |pos....||...slen...||...len - pos...|
case 2:  |pos==len....||...slen...|
case 3:  |len.........||...pos-len...||...slen...|
#endif
    if(pos < vslen) { /* Case 1 */
        /* make space for slen bytes at pos */
	totalspace = vslen+slen;
	vssetalloc(vs,(totalspace));
	vssetlength(vs,(totalspace));
	memmovex(vs->content+pos+slen,vs->content+pos,(vslen - pos));
	memcpy(vs->content+pos,s,slen);	
    } else if(pos == vslen) { /* Case 2 */
        /* make space for slen bytes at pos (== length) */
	totalspace = vslen+slen;
	vssetalloc(vs,(totalspace));
	vssetlength(vs,(totalspace));
	/* append s at pos */
	memcpy(vs->content+pos,s,slen);	
    } else /*pos > vslen */ { /* Case 3 */
        /* make space for slen bytes at pos */
	totalspace = pos + slen;
	vssetalloc(vs,(totalspace));
	vssetlength(vs,(totalspace));
	/* clear space from length..pos */
	memset(vs->content+vslen,0,(pos-vslen));
	/* append s at pos */
	memmovex(vs->content+pos+slen,vs->content+pos,(vslen - pos));
	memcpy(vs->content+pos,s,slen);	
    }
    vs->content[totalspace] = '\0'; /* guarantee nul term */
    return totalspace;
}

/**
Remove n characters at position pos and move the remainder down to compress out deleted chars.
@param vs
@param dstpos where to remove; if dstpos > |vs->contents| then do nothing
@param elide no. of chars to remove
@return final new length
Side effect: reduce index by elided amount
*/
static int
vsremoven(VString* vs, size_t dstpos, size_t elide)
{
    size_t newlength = 0;
    size_t vslen = 0;
    size_t srcpos = 0;
    size_t srclen = 0; /* amount to memmove */

    assert(vs != NULL);
    if(elide == 0) goto done; /* nothing to move */
    vslen = vslength(vs);
    srcpos = dstpos + elide;
    /* edge cases */
    if(srcpos >= vslen) { /* remove everything from dstpos to vslen */
	vssetlength(vs,dstpos);
    } else {
        srclen = (vslen - srcpos);
        memmove(vscontents(vs)+dstpos,vscontents(vs)+srcpos,srclen);
        vssetlength(vs,vslen - elide);
    }
    vs->index -= elide;
done:
    newlength = vslength(vs);
    vs->content[newlength] = '\0'; /* guarantee nul term */
    return newlength;
}

/* Extract the content and leave content null */
static char*
vsextract(VString* vs)
{
    char* x = NULL;
    if(vs == NULL) return NULL;
    if(vs->content == NULL) {
	/* guarantee content existence and nul terminated */
	if((vs->content = calloc(1,sizeof(char)))==NULL) return NULL;
	vs->length = 0;
    }
    x = vs->content;
    vs->content = NULL;
    vs->length = 0;
    vs->alloc = 0;
    return x;
}

/** Index Management Functions */

/**
@param vs
@param pos set vs->index to pos
@return vsindexp of new index
*/
static char*
vsindexset(VString* vs, size_t pos)
{
    assert(vs != NULL);
    assert(vs->index >= 0);
    vssetalloc(vs,pos+1); /* force existence */
    if(pos > vs->length) pos = vs->length; /* do not advance */
    vs->index = pos;
    return vsindexp(vs);
}

/**
@param vs
@param skip incr vs->index by skip
@return vsindexp of new index
*/
static char*
vsindexskip(VString* vs, size_t skip)
{
    assert(vs != NULL);
    assert(vs->index >= 0);
    vsindexset(vs,vs->index + skip);
    return vsindexp(vs);
}

/**
@param vs
@return current index
*/
static size_t
vsindex(VString* vs)
{
    assert(vs != NULL);
    vssetalloc(vs,0);
    return vs->index;
}

/**
@param vs
@return pointer to the char at the current index
*/
static char*
vsindexp(VString* vs)
{
    char* p;
    assert(vs != NULL && vs->index <= vs->length); 
    vssetalloc(vs,0);
    p = vs->content + vs->index;
    return p;
}

/**
Insert a string at the index.
Leave index unchanged.
@param vs
@param buffer dst for read
@return final size of vs
*/
static int
vsindexinsertn(VString* vs, const char* s, size_t slen)
{
    int finalsize = 0;
    if(vs->index > vs->length) vs->index = vs->length;
    finalsize = vsinsertn(vs,vs->index,s,slen);
    return finalsize;        
}

/* Hack to suppress compiler warnings about selected unused static functions */
static void
vssuppresswarnings(void)
{
    void* ignore;
    ignore = (void*)vssuppresswarnings;
    (void)ignore;
}

/**************************************************/
/* Simple variable length lists */

typedef struct VList {
  size_t alloc;
  size_t length;
  void** content;
} VList;

/* Forward */
static VList* vlnew(void);
static void vlfree(VList* l);
static void vlfreeall(VList* l);
static void* vlget(VList* l, unsigned index);
static void vlpush(VList* l, void* elem);

static VList*
vlnew(void)
{
  VList* l;
  l = (VList*)calloc(1,sizeof(VList));
  assert(l != NULL);
  return l;
}

static void
vlfree(VList* l)
{
  if(l == NULL) return;
  if(l->content != NULL) {free(l->content); l->content = NULL;}
  free(l);
}

static void
vlfreeall(VList* l) /* call free() on each list element*/
{
  unsigned i;
  if(l == NULL || l->length == 0) return;
  for(i=0;i<l->length;i++) if(l->content[i] != NULL) {free(l->content[i]);}
  vlfree(l);
}

static void
vlexpand(VList* l)
{
  void** newcontent = NULL;
  unsigned newsz;

  if(l == NULL) return;
  newsz = (l->length * 2) + 1; /* basically double allocated space */
  if(l->alloc >= newsz) return; /* space already allocated */
  newcontent=(void**)calloc(newsz,sizeof(void*));
  assert(newcontent != NULL);
  if(l->alloc > 0 && l->length > 0 && l->content != NULL) { /* something to copy */
    memcpy((void*)newcontent,(void*)l->content,sizeof(void*)*l->length);
  }
  if(l->content != NULL) free(l->content);
  l->content=newcontent;
  l->alloc=newsz;
  /* size is the same */  
}

static void*
vlget(VList* l, unsigned index) /* Return the ith element of l */
{
  if(l == NULL || l->length == 0) return NULL;
  assert(index < l->length);
  return l->content[index];
}

static void
vlpush(VList* l, void* elem)
{
  if(l == NULL) return;
  while(l->length >= l->alloc) vlexpand(l);
  l->content[l->length] = elem;
  l->length++;
}

/* Following are always "in-lined"*/
#define vlcontents(l)  ((l)==NULL?NULL:(l)->content)
#define vllength(l)  ((l)==NULL?0:(l)->length)
#define vlclear(l)  vlsetlength(l,0)
#define vlsetlength(l,len)  do{if((l)!=NULL) (l)->length=len;} while(0)
