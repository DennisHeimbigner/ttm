#ifndef VUTILS_H
#define VUTILS_H 1

/* Define if the equivalent of the standard Unix memmove() is available */
#define HAS_MEMMOVE

typedef struct VList {
    size_t alloc;
    size_t length;
    void**  content; 
    size_t index; /* 0 <= index < length */
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
#define nullfree(x) do{if((x)!=NULL) free(x);}while(0)
#endif

/**************************************************/
/* Forward */

static VList* vlnew(void);
static VList* vlnewdeep(void (deepfree)(size_t nelems, void** elems), void deepclone(size_t nelems, void** src, void** dst));
static void vlfreeall(VList* va);
static void vlfree(VList* va);
static void vlexpand(VList* va);
static void vlsetalloc(VList* va, size_t minalloc);
static void vlsetlength(VList* va, size_t newlen);
static void vlappend(VList* va, void* elem);
static void vlinsert(VList* va, size_t pos, void* elem);
static void* vlremove(VList* va, size_t pos);
static void* vlget(VList* va, size_t pos);
static void* vlgetp(VList* va, size_t pos);
static void* vlextract(VList* va);
static void vlindexset(VList* va, size_t pos);
static void vlindexskip(VList* va, size_t skip);
static size_t vlindex(VList* va);
static void vlindexremove(VList* va);
static void vlindexinsert(VList* va, void* elem);
static VList* vldeepclone(VList* va);
static VList* vlclone(VList* va);
static void vutilsuppresswarnings(void);
static VString* vsnew(void);
static void vsfree(VString* va);
static void vsexpand(VString* va);
static void vssetalloc(VString* va, size_t minalloc);
static void vssetlength(VString* va, size_t newlen);
static void vsappendn(VString* va, const char* s, size_t slen);
static void vsappend(VString* va, char c);
static void vsinsertn(VString* va, size_t pos, const char* s, size_t slen);
static void vsremoven(VString* va, size_t pos, size_t n);
static char* vsgetp(VString* va, size_t pos);
static char* vsextract(VString* va);
static void vsindexset(VString* va, size_t pos);
static char* vsindexskip(VString* va, size_t skip);
static size_t vsindex(VString* va);
static char* vsindexp(VString* va);
static void vsindexinsertn(VString* va, const char* s, size_t n);
static void vsindexremoven(VString* va, size_t n);
static VString* vsclone(VString* va);
static void vsmemmove(char* dst, char* src, size_t len);

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
    else
        nullfree(va->content);
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
@param va the array to expand
@param minalloc make sure alloc is at least this amount
@return void
*/
static void
vlsetalloc(VList* va, size_t minalloc)
{
    while(va->alloc < minalloc) vlexpand(va);
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
    size_t oldlen;
    assert(va != NULL);
    oldlen = va->length;
    if(newlen > oldlen)
        vlsetalloc(va,newlen);
    if(va->index > newlen) va->index = newlen;
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
Side effect: increase index by one if index is past pos
*/
static void
vlinsert(VList* va, size_t pos, void* elem)
{
  size_t i;
  assert(va != NULL);
  vlsetalloc(va,1);
  if(va->length > 0) {
    for(i=va->length;i>pos;i--) va->content[i] = va->content[i-1];
  }
  va->content[pos] = elem;
  if(va->index > pos) va->index++;
  va->length++;
  va->content[va->length] = NULL; /* ensure null terminated */
}

/**
Remove element at position pos.
@param va
@param pos where to remove
@return removed elem
Side effect: reduce index by one if index is past pos
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
  if(va->index > pos) va->index--;
  va->length--;
  va->content[va->length] = NULL; /* ensure null terminated */
  return elem;
}

static void*
vlget(VList* va, size_t pos)
{
    assert(va->length >= pos);
    vlsetalloc(va,1);
    return va->content[pos*sizeof(void*)];
}

static void*
vlgetp(VList* va, size_t pos)
{
    assert(va->length >= pos);
    vlsetalloc(va,1);
    return va->content + (pos*sizeof(void*));
}

/**
Extract the content and leave content null.
@param va
@return ptr to extracted content
Side effect: leave va->length == 0
*/
static void*
vlextract(VList* va)
{
    void* x = NULL;
    if(va == NULL) return NULL;
    /* guarantee content existence and nul terminated */
    vlsetalloc(va,1);
    x = va->content;
    va->content = NULL;
    va->length = 0;
    va->alloc = 0;
    return x;
}

/** Index Management Functions */

/**
Set the index but index <= va->length.
@param va
@param pos set va->index to pos
@return void
*/
static void
vlindexset(VList* va, size_t pos)
{
    assert(va != NULL);
    assert(va->index >= 0);
    vlsetalloc(va,pos+1); /* force existence */
    if(pos > va->length) pos = va->length; /* do not advance */
    va->index = pos;
}

/**
Move the index up by skip elems
@param va
@param skip incr va->index by skip
@return void* of new index
*/
static void
vlindexskip(VList* va, size_t skip)
{
    assert(va != NULL);
    assert(va->index >= 0);
    vlindexset(va,va->index + skip);
}

/**
Return current index.
@param va
@return current index
*/
static size_t
vlindex(VList* va)
{
    assert(va != NULL);
    vlsetalloc(va,1);
    return va->index;
}

/**
Remove a element at the index.
Index remains unchanged.
@param va
@return void
*/
static void
vlindexremove(VList* va)
{
    if(va->index > va->length) va->index = va->length;
    vlremove(va,va->index);
}

/**
Insert an element at the index.
Move index past insertion
@param va
@param elem to insert
@return void
*/
static void
vlindexinsert(VList* va, void* elem)
{
    if(va->index > va->length) va->index = va->length;
    vlinsert(va,va->index,elem);
    va->index++;
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
#define vscat(vs,s)  vsappendn(vs,s,0)
#define vsclear(vs)  vssetlength(vs,0)
#define vspush(vs,elem) vsappend(vs,elem)

/**************************************************/



/**************************************************/

/* Hack to suppress compiler warnings about selected unused static functions */
static void
vutilsuppresswarnings(void)
{
    void* ignore;
    ignore = (void*)vutilsuppresswarnings;
    (void)ignore;
    ignore = (void*)vlclone;
    ignore = (void*)vldeepclone;
    ignore = (void*)vlindexinsert;
    ignore = (void*)vlindexremove;
    ignore = (void*)vlindex;
    ignore = (void*)vlindexskip; 
    ignore = (void*)vlextract;
    ignore = (void*)vlgetp;
    ignore = (void*)vlappend;
    ignore = (void*)vlsetlength;
    ignore = (void*)vlnewdeep;

    ignore = (void*)vsextract;
}

#endif /*VUTILS_H*/

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
    if(va->content != NULL) free(va->content);
    va->content = newcontent;
    va->alloc = newalloc;
    /* length stays the same */  
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
    while(va->alloc < minalloc) vsexpand(va);
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
    size_t oldlen;
    assert(va != NULL);
    oldlen = va->length;
    if(newlen > oldlen)
        vssetalloc(va,newlen);
    if(va->index > newlen) va->index = newlen;
    va->length = newlen;
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
Side effect: increase index by |s| if index >  pos
*/
static void
vsinsertn(VString* va, size_t pos, const char* s, size_t slen)
{
  assert(va != NULL && (slen == 0 || s != NULL));
  if(slen == 0) return;
  vssetalloc(va,pos+slen);
  if(pos < va->length)
      vsmemmove(&va->content[pos+slen],&va->content[pos],slen);
  memcpy(&va->content[pos],s,slen);
  va->length += slen;
  va->content[va->length] = '\0';
  if(va->index <= pos) va->index += slen;
}

/**
Remove n characters at position pos.
@param va
@param pos where to remove
@param n no. of chars to remove
@return void
Side effect:
(1) reduce index by n if index >  pos+n
(2) set index to pos if index >  pos
*/
static void
vsremoven(VString* va, size_t pos, size_t n)
{
  assert(va != NULL);
  assert((pos+n) < va->length);
  if(n > 0) {
    vsmemmove(&va->content[pos],&va->content[pos+n],n);
    va->length -= n;
  }
  va->content[va->length] = '\0';
  if(va->index > (pos+n)) {va->index -= n;} else {if(va->index > pos) {va->index = pos;}}
}

static char*
vsgetp(VString* va, size_t pos)
{
    assert(va->length >= pos);
    return va->content + (pos*sizeof(char));
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
    if(pos > va->length) pos = va->length; /* do not advance */
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

/**
Remove n chars at the index.
Index remains unchanged.
@param va
@param n chars to remove
@return void
*/
static void
vsindexremoven(VString* va, size_t n)
{
    if(va->index > va->length) va->index = va->length;
    vsremoven(va,va->index,n);
}

/**
Insert n chars of s at the index.
@param va
@param s src string
@param n chars of s to insert
@return void
Side Effect: move index past insertion
*/
static void
vsindexinsertn(VString* va, const char* s, size_t n)
{
    size_t newindex;
    if(va->index > va->length) va->index = va->length;
    newindex = va->index+n;
    vsinsertn(va,va->index,s,n);
    va->index = newindex;
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

