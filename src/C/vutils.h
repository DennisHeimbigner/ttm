#ifndef VUTILS_H
#define VUTILS_H 1

/* Define if the equivalent of the standard Unix memmove() is available */
#define HAS_MEMMOVE

typedef struct VList {
    size_t alloc;
    size_t length;
    void**  content; 
    void   (*deepfree)(size_t nelems, void** elems);
    void   (*deepclone)(size_t nelems, void** src, void** dst);
} VList;

typedef struct VString {
    size_t alloc;
    size_t length;
    char* content; 
    size_t index; /* 0 <= index < length */
} VString;

/* Minimum VList expansion size */
#define VLMINALLOC 4

/* Minimum VString expansion size */
#define VSMINALLOC 16

#ifndef nullfree
#define nullfree(x) do{void* p = (void*)(x); if(p) free(p);}while(0)
#endif

/**************************************************/
/* Forward */

static VList* vlnew(void);
static VList* vlnewdeep(void (deepfree)(size_t nelems, void** elems), void deepclone(size_t nelems, void** src, void** dst));
static void vlfreeall(VList* va);
static void vlfree(VList* va);
static void vlmanifest(VList* va);
static void vlexpand(VList* va);
static void vlsetalloc(VList* va, size_t minalloc);
static void vlsetlength(VList* va, size_t newlen);
static void vlappend(VList* va, void* elem);
static void vlinsert(VList* va, size_t pos, void* elem);
static void* vlremove(VList* va, size_t pos);
static void* vlget(VList* va, size_t pos);
static void** vlgetp(VList* va, size_t pos);
static void** vlextract(VList* va);
static VList* vldeepclone(VList* va);
static VList* vlclone(VList* va);

static VString* vsnew(void);
static void vsfree(VString* va);
static VString* vsclone(VString* va);
static void vsmanifest(VString* va);
static void vsexpand(VString* va);
static void vssetalloc(VString* va, size_t minalloc);
static void vssetlength(VString* va, size_t newlen);
static void vsappendn(VString* va, const char* s, size_t slen);
static void vsappend(VString* va, char c);
static void vsinsertn(VString* va, size_t pos, const char* s, size_t slen);
static void vssetn(VString* va, size_t pos, const char* s, size_t slen);
static void vsremoven(VString* va, size_t pos, size_t n);
static char* vsgetp(VString* va, size_t pos);
static char vsget(VString* va, size_t pos);
static char* vsextract(VString* va);
static void vsnulterm(VString* va);

static void vsindexset(VString* va, size_t pos);
static char* vsindexskip(VString* va, size_t skip);
static size_t vsindex(VString* va);
static char* vsindexp(VString* va);

static void vsmemmove(char* dst, char* src, size_t len);
static void vutilsuppresswarnings(void);

/**************************************************/
/* "Inlined" */
#define vlcontents(vl)  ((vl)==NULL?(void**)(vl):(vl)->content)
#define vllength(vl)  ((vl)==NULL?0:(vl)->length)
#define vlalloc(vl)  ((vl)==NULL?0:(vl)->alloc)
#define vlpush(vl,s)  vlappend(vl,s)
#define vlclear(vl)  vlsetlength(vl,0)

/*************************/
/**
Create a new VList object.
@return ptr to the new object
*/
static VList*
vlnew(void)
{
    VList* va = NULL;
    va = (VList*)calloc(1,sizeof(VList));
    assert(va != NULL);
    return va;
}

/**
Create a new VList object with deep free/clone functions.
@param deep free fcn
@param deep clone fcn
@return ptr to the new object
*/
static VList*
vlnewdeep(void (deepfree)(size_t nelems, void** elems), void deepclone(size_t nelems, void** src, void** dst))
{
    VList* va = NULL;
    va = (VList*)calloc(1,sizeof(VList));
    assert(va != NULL);
    va->deepfree = deepfree;
    va->deepclone = deepclone;
    return va;
}

/**
Reclaim a VList object.
@param va the array to reclaim
@return void
*/
static void
vlfreeall(VList* va)
{
    if(va->deepfree)
	va->deepfree(va->length,va->content);
    else {
	size_t i;
	for(i=0;i<va->length;i++)
	    nullfree(va->content[i]);
	nullfree(va->content);
    }
    va->content = NULL;
    va->length = 0;
    nullfree(va);
}

/**
Reclaim a VList object.
@param va the array to reclaim
@return void
*/
static void
vlfree(VList* va)
{
    if(va == NULL) return;
    nullfree(va->content);
    free(va);
}

/**
Helper function to ensure va->content != NULL.
*/
static void
vlmanifest(VList* va)
{
    if(va == NULL) return;
    assert(va->alloc == 0 || (va->alloc > 0 && va->content != NULL));
    if(va->content != NULL) return;
    va->content = (void**)calloc(1,sizeof(void*));
    va->alloc = 1;
    va->length = 0;
}

/**
Expand the VList's capacity by a fixed amount in units of elemsize.
@param va the array to expand
@return void
*/
static void
vlexpand(VList* va)
{
    void* newcontent = NULL;
    size_t newalloc;

    if(va == NULL) return;
    /* Expand by (alloc == 0 ? VLMINALLOC : 2 * va->alloc) */
    newalloc = (va->alloc == 0 ? VLMINALLOC : (2 * va->alloc));
    /* Calling expand always ensures that va->content is non-null */
    if(va->content != NULL && va->alloc >= newalloc) return; /* space already allocated */
    newcontent = calloc((newalloc+1),sizeof(void*));/* always room for nul term */
    assert(newcontent != NULL);
    if(va->alloc > 0
	&& va->length > 0
	&& va->content != NULL) /* something to copy */
            memcpy((void*)newcontent,(void*)va->content,(va->length*sizeof(void*)));
    if(va->content != NULL) free(va->content);
    va->content = newcontent;
    va->alloc = newalloc;
    /* length stays the same */  
}

/**
Set the allocated capacity of the VList's capacity.
Allocated capacity will never decrease.
@param va the array to expand
@param minalloc make sure alloc is at least this amount
@return void
*/
static void
vlsetalloc(VList* va, size_t minalloc)
{
    vlmanifest(va); /* ensure va->content */
    while(va->alloc <= minalloc) vlexpand(va); /* ensure room for NULL term */
}

/**
Set the length of the current no. of elements in the array.
@param va the array to expand
@param newlen
@return void
*/
static void
vlsetlength(VList* va, size_t newlen)
{
    assert(va != NULL);
    vlmanifest(va); /* ensure va->content */
    vlsetalloc(va,newlen);
    va->length = newlen;
}

/**
Append element to the end of an array
Modify the alloc and length as needed
@param va the array to expand
@param elem to append
@return void
*/
static void
vlappend(VList* va, void* elem)
{
    vlinsert(va,va->length,elem);
}

/**
Insert an element at position pos.
@param va
@param pos where to insert; if pos > |va->content| then expand va.
@param elem to insert
@return void
*/
static void
vlinsert(VList* va, size_t pos, void* elem)
{
  size_t i;
  assert(va != NULL);
  vlsetalloc(va,va->length+1);
  if(va->length > 0) {
    for(i=va->length;i>pos;i--) va->content[i] = va->content[i-1];
  }
  va->content[pos] = elem;
  va->length++;
  va->content[va->length] = NULL; /* ensure null terminated */
}

/**
Remove element at position pos.
@param va
@param pos where to remove
@return removed elem
*/
static void*
vlremove(VList* va, size_t pos)
{
  size_t len,i;
  void* elem;
  assert(va != NULL);
  if((len=va->length) == 0) return NULL;
  if(pos >= len) return NULL;
  elem = va->content[pos];
  for(i=pos+1;i<len;i++) va->content[i-1] = va->content[i];
  va->length--;
  va->content[va->length] = NULL; /* ensure null terminated */
  return elem;
}

static void*
vlget(VList* va, size_t pos)
{
    return *vlgetp(va,pos);
}

static void**
vlgetp(VList* va, size_t pos)
{
    assert(va->length >= pos);
    vlsetalloc(va,pos+1);
    assert(pos < va->length);
    return va->content + pos;
}

/**
Extract the content and leave content null.
@param va
@return ptr to extracted content
Side effect: leave va->length == 0
*/
static void**
vlextract(VList* va)
{
    void** x = NULL;
    if(va == NULL) return NULL;
    /* guarantee content existence and nul terminated */
    vlsetalloc(va,1);
    x = va->content;
    va->content = NULL;
    va->length = 0;
    va->alloc = 0;
    return x;
}

/**
Deep clone a VList object.
@param va the variable-length object to clone
@param deep the deep cloner function
@return ptr to clone
*/
static VList*
vldeepclone(VList* va)
{
    VList* clone = NULL;
    assert(va->deepclone != NULL);
    clone = (VList*)calloc(1,sizeof(VList));
    assert(va != NULL);
    *clone = *va; /* copy the fields */
    /* Now fix up alloc'd fields */
    if(va->content != NULL) {
	clone->content = (void**)calloc(va->alloc,sizeof(void*));
        assert(clone->content != NULL);
        va->deepclone(va->length,va->content,clone->content);
    }
    return clone;
}

/**
Shallow clone a VList object.
@param va the variable-length object to clone
@return ptr to clone
*/
static VList*
vlclone(VList* va)
{
    VList* clone = NULL;
    clone = (VList*)calloc(1,sizeof(VList));
    assert(clone != NULL);
    *clone = *va; /* copy the fields */
    /* Now fix up alloc'd fields */
    if(va->content != NULL) {
	clone->content = (void**)calloc(va->alloc,sizeof(void*));
        assert(clone->content != NULL);
	memcpy(clone->content,va->content,va->length*sizeof(void*));
    }
    return clone;
}

/**************************************************/
/* Sequence of utf8/ascii characters */

/*************************/
/* "Inlined" */
#define vscontents(vs)  ((vs)==NULL?(char*)(NULL):(char*)((VList*)(vs))->content)
#define vslength(vs)  ((vs)==NULL?0:((VList*)(vs))->length)
#define vsalloc(vs)  ((vs)==NULL?0:((VList*)(vs))->alloc)
#define vscat(vs,s)  vsappendn(vs,s,strlen(s))
#define vsclear(vs)  vssetlength(vs,0)

/**************************************************/

/**
Create a new VString object.
@return ptr to the new object
*/
static VString*
vsnew(void)
{
    VString* va = NULL;
    va = (VString*)calloc(1,sizeof(VString));
    assert(va != NULL);
    return va;
}

/**
Reclaim a VString object.
@param va the array to reclaim
@return void
*/
static void
vsfree(VString* va)
{
    if(va == NULL) return;
    nullfree(va->content);
    free(va);
}

/**
Clone a VString object.
@param va the variable-length object to clone
@return ptr to clone
*/
static VString*
vsclone(VString* va)
{
    VString* clone = NULL;
    clone = (VString*)calloc(1,sizeof(VString));
    assert(clone != NULL);
    *clone = *va; /* copy the fields */
    /* Now fix up alloc'd fields */
    if(va->length > 0) {
    	assert(va->content != NULL);
	clone->content = (char*)calloc(va->alloc,sizeof(char));
        assert(clone->content != NULL);
	memcpy(clone->content,va->content,va->length*sizeof(char));
    }
    return clone;
}

/**
Helper function to ensure va->content != NULL.
*/
static void
vsmanifest(VString* va)
{
    if(va == NULL) return;
    assert(va->alloc == 0 || (va->alloc > 0 && va->content != NULL));
    if(va->content != NULL) return;
    va->content = (char*)calloc(1,sizeof(char));
    va->alloc = 1;
    va->length = 0;
}

/**
Expand the VString's capacity by a fixed amount in units of elemsize.
@param va the array to expand
@return void
*/
static void
vsexpand(VString* va)
{
    char* newcontent = NULL;
    size_t newalloc;

    if(va == NULL) return;
    /* Expand by (alloc == 0 ? VSMINALLOC : 2 * va->alloc) */
    newalloc = (va->alloc == 0 ? VSMINALLOC : (2 * va->alloc));
    /* Calling expand always ensures that va->content is non-null */
    if(va->content != NULL && va->alloc >= newalloc) return; /* space already allocated */
    newcontent = calloc((newalloc+1),sizeof(char));/* always room for nul term */
    assert(newcontent != NULL);
    if(va->alloc > 0
	&& va->length > 0
	&& va->content != NULL) { /* something to copy */
            memcpy(newcontent,va->content,(va->length*sizeof(char)));
	    newcontent[va->length*sizeof(char)] = '\0';
    }
    if(va->content != NULL) {free(va->content); va->content = NULL;}
    va->content = newcontent; newcontent = NULL;
    va->alloc = newalloc;
    /* length stays the same */  
    nullfree(newcontent);
}

/**
Set the allocated capacity of the VString's capacity.
@param va the array to expand
@param minalloc make sure alloc is at least this amount
@return void
*/
static void
vssetalloc(VString* va, size_t minalloc)
{
    vsmanifest(va);
    while(va->alloc <= minalloc) vsexpand(va);
}

/**
Set the length of the current no. of elements in the array.
@param va the array to expand
@param newlen
@return void
*/
static void
vssetlength(VString* va, size_t newlen)
{
    assert(va != NULL);
    vsmanifest(va);
    vssetalloc(va,newlen); /* ensure va->content exists */
    va->length = newlen;
    va->content[va->length] = '\0';
}

/**
Append n chars to the end of a string
@param va the array to expand
@param s string to append
@param slen no. of chars to append
@return void
*/
static void
vsappendn(VString* va, const char* s, size_t slen)
{
    vsinsertn(va,va->length,s,slen);
}

/**
Append 1 char to the end of a string
@param va the array to expand
@param c char to append
@return void
*/
static void
vsappend(VString* va, char c)
{
    char s[2] = {0,0};
    s[0] = c;
    vsappendn(va,s,1);
}

/**
Insert a string at position pos.
@param va
@param pos where to insert; if pos > |va->content| then expand va.
@param s string to append
@param slen no. of chars to append
@return void
*/
static void
vsinsertn(VString* va, size_t pos, const char* s, size_t slen)
{
  assert(va != NULL && (slen == 0 || s != NULL));
  assert(pos <= va->length);
  if(slen == 0) return;
  vssetalloc(va,va->length+slen);
  if(pos < va->length) {
    size_t nmove = va->length - pos;
    vsmemmove(&va->content[pos+slen],&va->content[pos],nmove);
  }
  memcpy(&va->content[pos],s,slen);
  va->length += slen;
  va->content[va->length] = '\0';
}

/*
Overwrite a string at position pos.
@param va
@param pos where to write; if pos > |va->content| then expand va.
@param s src string
@param slen |s|	
@return void
*/
static void
vssetn(VString* va, size_t pos, const char* s, size_t slen)
{
  size_t finallen;
  assert(va != NULL && (slen == 0 || s != NULL));
  assert(pos <= va->length);
  if(slen == 0) return;
  /* compute final string length (since pos+slen might be > va->length) */
  finallen = pos+slen;
  if(finallen < va->length) finallen = va->length;
  vssetalloc(va,finallen);
  memcpy(&va->content[pos],s,slen);
  vssetlength(va,finallen);
  vsnulterm(va);
}

/**
Remove n characters at position pos.
@param va
@param pos where to remove
@param n no. of chars to remove
@return void
*/
static void
vsremoven(VString* va, size_t pos, size_t n)
{
  size_t nmove;
  assert(va != NULL);
  assert((pos+n) <= va->length);
  if(n == 0) return;
  nmove = va->length - (pos+n);
  if(nmove > 0)
    vsmemmove(&va->content[pos],&va->content[pos+n],nmove);
  va->length -= n;
  va->content[va->length] = '\0';
}

static char*
vsgetp(VString* va, size_t pos)
{
    assert(va->length >= pos);
    return va->content + (pos*sizeof(char));
}

static char
vsget(VString* va, size_t pos)
{
    assert(va->length >= pos);
    return va->content[(pos*sizeof(char))];
}

/**
Extract the content and leave content null.
@param va
@return ptr to extracted content
Side effect: leave va->length == 0
*/
static char*
vsextract(VString* va)
{
    void* x = NULL;
    if(va == NULL) return NULL;
    if(va->content == NULL) {
	vssetalloc(va,1); /* guarantee content existence and nul terminated */
        va->length = 0;
    }
    x = va->content;
    va->content = NULL;
    va->length = 0;
    va->alloc = 0;
    return x;
}

static void
vsnulterm(VString* va)
{
    if(va->length == va->alloc)
        vssetalloc(va,va->alloc+1);
    va->content[va->length] = '\0';
}

/** Index Management Functions */

/**
Set the index to pos.
@param va
@param pos set va->index to pos
@return void
*/
static void
vsindexset(VString* va, size_t pos)
{
    assert(va != NULL);
    assert(va->index >= 0);
    vssetalloc(va,1); /* force existence */
    if(pos > va->length)
        pos = va->length; /* do not advance */
    else
        va->index = pos;
}

/**
Move the index up by skip elems
@param va
@param skip incr va->index by skip
@return void* of new index
*/
static char*
vsindexskip(VString* va, size_t skip)
{
    assert(va != NULL);
    assert(va->index >= 0);
    vsindexset(va,va->index + skip);
    return vsindexp(va);
}

/**
Return current index.
@param va
@return current index
*/
static size_t
vsindex(VString* va)
{
    assert(va != NULL);
    vssetalloc(va,1);
    return va->index;
}

/**
Return pointer to current index'th char.
@param va
@return current index
*/
static char*
vsindexp(VString* va)
{
    assert(va != NULL);
    vssetalloc(va,1);
    return &va->content[va->index];
}

/**************************************************/
/* Utility functions */

/**
Define an internal form of memmove if not defined by platform.
@param dst where to store bytes
@param src source of bytes
@param len no. of bytes to move
@return void
*/
static void
vsmemmove(char* dst, char* src, size_t len)
{
#ifdef HAS_MEMMOVE
    memmove((void*)dst,(void*)src,len*sizeof(char));
#else
    src += len;
    dst += len;
    while(len--) {*(--dst) = *(--src);}
#endif
}

/* Hack to suppress compiler warnings about selected unused static functions */
static void
vutilsuppresswarnings(void)
{
    void* ignore;
    ignore = (void*)vutilsuppresswarnings;
    (void)ignore;
    ignore = (void*)vlclone;
    ignore = (void*)vldeepclone;
    ignore = (void*)vlnew;
    ignore = (void*)vlnewdeep;
    ignore = (void*)vlfreeall;
    ignore = (void*)vlfree;
    ignore = (void*)vlmanifest;
    ignore = (void*)vlexpand;
    ignore = (void*)vlsetalloc;
    ignore = (void*)vlsetlength;
    ignore = (void*)vlappend;
    ignore = (void*)vlinsert;
    ignore = (void*)vlremove;
    ignore = (void*)vlget;
    ignore = (void*)vlgetp;
    ignore = (void*)vlextract;
    ignore = (void*)vldeepclone;
    ignore = (void*)vlclone;
    ignore = (void*)vsnew;
    ignore = (void*)vsfree;
    ignore = (void*)vsclone;
    ignore = (void*)vsmanifest;
    ignore = (void*)vsexpand;
    ignore = (void*)vssetalloc;
    ignore = (void*)vssetlength;
    ignore = (void*)vsappendn;
    ignore = (void*)vsappend;
    ignore = (void*)vsinsertn;
    ignore = (void*)vssetn;
    ignore = (void*)vsremoven;
    ignore = (void*)vsgetp;
    ignore = (void*)vsget;
    ignore = (void*)vsextract;
    ignore = (void*)vsnulterm;
    ignore = (void*)vsindexset;
    ignore = (void*)vsindexskip;
    ignore = (void*)vsindex;
    ignore = (void*)vsindexp;
    ignore = (void*)vsmemmove;
}

#endif /*VUTILS_H*/
