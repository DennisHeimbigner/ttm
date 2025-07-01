/* typedef is in types.h */
struct VArray {
    size_t elemsize;
    size_t alloc;   /* in units of elemsize */
    size_t length;  /* in units of elemsize */
    char* content; /* use char rather than void to support pointer arithmetic */
    size_t index; /* 0 <= index < length */
    void* elemnul; /* no arithmetic needed */
};

/* VArray has a fixed expansion size */
#define VARRAYALLOC 64

/* NUL terminate */
static void
nulterm(VArray* va, size_t pos)
{
    if(va->content != NULL) {
        memcpy(va->content+(pos*va->elemsize),va->elemnul,va->elemsize);
    }
}

/**************************************************/
/* Forward */

static VArray* vanew(size_t elemsize);
static void vadeepfree(VArray* va, void (deepfree)(void* elem, void* va));
static void vafree(VArray* va);
static void vafreeall(VArray* va);
static void vaexpand(VArray* va);
static void vasetalloc(VArray* va, size_t minalloc);
static void vasetlength(VArray* va, size_t newlen);
static void vaappendn(VArray* va, const void* elem, size_t n);
static void vainsertn(VArray* va, size_t pos, const void* elems, size_t elen);
static void varemoven(VArray* va, size_t pos, size_t elide);
static void* vagetp(VArray* va, size_t index);
static void* vaextract(VArray* va);
static void vaindexset(VArray* va, size_t pos);
static void* vaindexskip(VArray* va, size_t skip);
static size_t vaindex(VArray* va);
static void* vaindexp(VArray* va);
static void vaindexremoven(VArray* va, size_t elide);
static void vaindexinsertn(VArray* va, const void* seq, size_t slen);
static size_t vaelemlen(VArray* va, const void* seq);
static VArray* vadeepclone(VArray* va, void (deepclone)(void* dstelem, void* srcelem, void* va));
static VArray* vaclone(VArray* va);
static void vaappend(VArray* va, const void* elem);

/*************************/
/* "Inlined" */
#define vacontents(va)  ((va)==NULL?NULL:(va)->content)
#define valength(va)  ((va)==NULL?0:(va)->length)
#define vaalloc(va)  ((va)==NULL?0:(va)->alloc)
#define vacat(va,s)  vaappendn(va,s,0)
#define vaclear(va)  vasetlength(va,0)

static void shallowclone(void* dstelem, void* srcelem, void* v);
static void shallowreclaimer(void* elem, void* v);

/*************************/
/**
Create a new VArray object.
@param elemsize the size (in bytes) of the elements of the array.
@return ptr to the new object
*/
static VArray*
vanew(size_t elemsize)
{
    VArray* va = NULL;
    va = (VArray*)calloc(1,sizeof(VArray));
    assert(va != NULL);
    va->elemsize = elemsize;
    va->elemnul = (void*)calloc(1,elemsize);
    return va;
}

/**
Deep reclaim a VArray object.
@param va the array to reclaim
@param deepfree deep reclaim function
@return void
*/
static void
vadeepfree(VArray* va, void (deepfree)(void* elem, void* v))
{
    if(va == NULL) return;
    if(va->content != NULL) {
        size_t offset;
        for(offset=0;offset<(va->elemsize*va->length);offset+=va->elemsize)
            deepfree(va->content+offset,(void*)va);
        va->length = 0;
    }
    vafree(va);
}

/**
Shallow reclaim a VArray object.
@param va the array to reclaim
@return void
*/
static void
vafreeall(VArray* va)
{
    vadeepfree(va,shallowreclaimer);
}

/**
Reclaim a VArray object.
@param va the array to reclaim
@return void
*/
static void
vafree(VArray* va)
{
    if(va == NULL) return;
    if(va->content != NULL) free(va->content);
    if(va->elemnul) free(va->elemnul);
    free(va);
}

/**
Expand the varray's capacity by a fixed amount in units of elemsize.
@param va the array to expand
@return void
*/
static void
vaexpand(VArray* va)
{
    void* newcontent = NULL;
    size_t newalloc;

    if(va == NULL) return;
    /* Expand by (alloc == 0 ? 4 : 2 * alloc) */
    newalloc = (va->alloc == 0 ? 4 : (2 * va->alloc));
    /* Calling vaexpand always ensures that va->content is non-null */
    if(va->content != NULL && va->alloc >= newalloc) return; /* space already allocated */
    newcontent = calloc(va->elemsize,(newalloc+1));/* always room for nul term */
    assert(newcontent != NULL);
    if(va->alloc > 0
	&& va->length > 0
	&& va->content != NULL) /* something to copy */
            memcpy((void*)newcontent,(void*)va->content,(va->length*va->elemsize));
    nulterm(va,va->length);
    if(va->content != NULL) free(va->content);
    va->content = newcontent;
    va->alloc = newalloc;
    /* length stays the same */  
}

/**
Set the allocated capacity of the varray's capacity.
@param va the array to expand
@param minalloc make sure alloc is at least this amount
@return void
*/
static void
vasetalloc(VArray* va, size_t minalloc)
{
    while(va->alloc < minalloc) vaexpand(va);
}

/**
Set the length of the current no. of elements in the array.
@param va the array to expand
@param newlen
@return void
*/
static void
vasetlength(VArray* va, size_t newlen)
{
    size_t oldlen;
    assert(va != NULL);
    oldlen = va->length;
    if(newlen > oldlen) {
        vasetalloc(va,newlen);
        nulterm(va,newlen);
    }
    if(va->index > newlen) va->index = newlen;
    va->length = newlen;
    /* Pretty'fy */
    if(va->content != NULL && va->alloc > va->length)
        nulterm(va,va->length);
}

/**
Append n elements to the end of an array
Modify the alloc and length as needed
@param va the array to expand
@param elems ptr to sequence of bytes to append, where |elems| % elemsize == 0
@param n the length of the sequence in elemsize chunks
@return void
*/
static void
vaappendn(VArray* va, const void* elem, size_t n)
{
    size_t need;
    assert(va != NULL && elem != NULL);
    if(n == 0) {n = vaelemlen(va,elem);}
    need = va->length + n;
    vasetalloc(va,need+1);
    assert(va->content != NULL);
    memcpy(va->content+(va->length*va->elemsize),elem,n*va->elemsize);
    va->length += n;
    nulterm(va,va->length); /* guarantee nul term */
}

/**
Append one element to the end of an array
Modify the alloc and length as needed
@param va the array to expand
@param elemp  ptr to element to append
@return void
*/
static void
vaappend(VArray* va, const void* elemp)
{
    vaappendn(va,elemp,1);
}

/**
Insert seq of elems s where |s|==n*elems into va at position pos.
@param va
@param pos where to insert; if pos > |va->content| then expand va.
@param elems to insert
@param elem no. of elems in elems
@return void
*/
static void
vainsertn(VArray* va, size_t pos, const void* s, size_t slen)
{
    size_t totalspace = 0;
    size_t valen = 0;

    assert(va != NULL && s != NULL);
    if(slen == 0) {slen = vaelemlen(va,s);}
    valen = valength(va);
#if 0
initial: |len.........|
case 1:  |pos....||...slen...||...len - pos...|
case 2:  |pos==len....||...slen...|
case 3:  |len.........||...pos-len...||...slen...|
#endif
    if(pos < valen) { /* Case 1 */
    /* make space for slen bytes at pos */
        totalspace = valen+slen;
        vasetalloc(va,(totalspace));
        vasetlength(va,(totalspace));
        memmovex((void*)(va->content+pos+slen),(void*)(va->content+pos),(valen - pos)*va->elemsize);
        memcpy((void*)(va->content+(pos*va->elemsize)),(void*)s,slen*va->elemsize);     
    } else if(pos == valen) { /* Case 2 */
    /* make space for slen bytes at pos (== length) */
        totalspace = valen+slen;
        vasetalloc(va,(totalspace));
        vasetlength(va,(totalspace));
        /* append s at pos */
        memcpy((void*)(va->content+(pos*va->elemsize)),(void*)s,slen*va->elemsize);     
    } else /*pos > valen */ { /* Case 3 */
    /* make space for slen bytes at pos */
        totalspace = pos + slen;
        vasetalloc(va,(totalspace));
        vasetlength(va,(totalspace));
        /* clear space from length..pos */
        memset((void*)(va->content+(valen*va->elemsize)),0,(pos-valen)*va->elemsize);
        /* append s at pos */
        memmovex((void*)(va->content+((pos+slen)*va->elemsize)),(void*)(va->content+(pos*va->elemsize)),(valen - pos)*va->elemsize);
        memcpy((void*)(va->content+(pos*va->elemsize)),(void*)s,slen*va->elemsize);     
    }
    nulterm(va,totalspace); /* guarantee nul term */
}

/**
Remove seq of elems s where |s|==n*elems starting at position pos.
@param va
@param pos where to remove; if pos > |va->content| then do nothing
@param elide no. of elems remove
@return void
Side effect: reduce index by elided amount if index is past pos
*/
static void
varemoven(VArray* va, size_t pos, size_t elide)
{
    size_t newlength = 0;
    size_t valen = 0;
    size_t srcpos = 0;
    size_t srclen = 0; /* amount to memmove */

    assert(va != NULL);
    if(elide == 0) goto done; /* nothing to move */
    valen = valength(va);
    srcpos = pos + elide;
    if(va->index > pos) {va->index = pos;} else {}
    /* edge cases */
    if(srcpos >= valen) { /* remove everything from pos to valen */
        vasetlength(va,pos);
    } else {
    srclen = (valen - srcpos);
    memmove((void*)(vacontents(va)+(pos*va->elemsize)),(void*)(vacontents(va)+(srcpos*va->elemsize)),srclen*va->elemsize);
    vasetlength(va,valen - elide);
    }
done:
    newlength = valength(va);
    nulterm(va,newlength);
}

static void*
vagetp(VArray* va, size_t pos)
{
    assert(va->length >= pos);
    return va->content + (pos*va->elemsize);
}

/**
Extract the content and leave content null.
@param va
@return ptr to extracted content
Side effect: leave va->length == 0
*/
static void*
vaextract(VArray* va)
{
    void* x = NULL;
    if(va == NULL) return NULL;
    if(va->content == NULL) {
        /* guarantee content existence and nul terminated */
        if((va->content = calloc(va->elemsize,va->elemsize))==NULL) return NULL;
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
Set the index but index <= va->length.
@param va
@param pos set va->index to pos
@return void
*/
static void
vaindexset(VArray* va, size_t pos)
{
    assert(va != NULL);
    assert(va->index >= 0);
    vasetalloc(va,pos+1); /* force existence */
    if(pos > va->length) pos = va->length; /* do not advance */
    va->index = pos;
}

/**
Move the index up by skip elems
@param va
@param skip incr va->index by skip
@return void* of new index
*/
static void*
vaindexskip(VArray* va, size_t skip)
{
    assert(va != NULL);
    assert(va->index >= 0);
    vaindexset(va,va->index + skip);
    return vaindexp(va);
}

/**
Return current index.
@param va
@return current index
*/
static size_t
vaindex(VArray* va)
{
    assert(va != NULL);
    vasetalloc(va,0);
    return va->index;
}

/**
Return pointer to the byte at the current index.
@param va
@return pointer to the byte at the current index
*/
static void*
vaindexp(VArray* va)
{
    void* p;
    assert(va != NULL && va->index <= va->length); 
    vasetalloc(va,0);
    p = va->content + (va->index*va->elemsize);
    return p;
}

/**
Remove a string at the index.
Index remains unchanged.
@param va
@param elide number of chars to remove
@return void
*/
static void
vaindexremoven(VArray* va, size_t elide)
{
    if(va->index > va->length) va->index = va->length;
    varemoven(va,va->index,elide);
}

/**
Insert a string at the index.
Move index past insertion
@param va
@param seq seq of elems to insert
@param slen |seq| in elemsize units
@return void
*/
static void
vaindexinsertn(VArray* va, const void* seq, size_t slen)
{
    if(va->index > va->length) va->index = va->length;
    vainsertn(va,va->index,seq,slen);
    va->index += slen;
}

/**
Append n elements to the end of an array
Modify the alloc, length, and index as needed
@param va the array to expand
@param elems ptr to sequence of bytes to append, where |elems| % elemsize == 0
@param nelem the length of the sequence in elemsize chunks
@return void
*/
static void
vaindexappendn(VArray* va, const void* elem, size_t nelem)
{
    size_t need;
    assert(va != NULL && elem != NULL);
    if(nelem == 0) {nelem = vaelemlen(va,elem);}
    need = va->length + nelem;
    vasetalloc(va,need+1);
    assert(va->content != NULL);
    memcpy(va->content+(va->length*va->elemsize),elem,nelem*va->elemsize);
    va->length += nelem;
    nulterm(va,va->length); /* guarantee nul term */
    assert(va->index >= 0);
    vaindexset(va,va->index + nelem);
}




/**
Compute the length of an elem seq as defined by the first occurrence of va->elemnul.
@param seq whose length is to be computed
@return the number of elems (in va->elemsize units) of the seq.
*/
static size_t
vaelemlen(VArray* va, const void* seq)
{
    size_t len = 0;
    const void* p = seq;
    while(memcmp(p,va->elemnul,va->elemsize)!=0) {p += va->elemsize;len++;}    
    return len;
}

/**********************/
/* Function arguments */

/* Shallow Cloner */
static void
shallowclone(void* dstelem, void* srcelem, void* v)
{
    VArray* va = (VArray*)v;
    memcpy(dstelem,srcelem,va->elemsize);
}

static void
shallowreclaimer(void* elem, void* v)
{
    void** s = (void**)elem;
    UNUSED(v);
    nullfree(*s);
    *s = NULL;
}

/**********************/

/**
Deep clone a VArray object.
@param va the variable-length object to clone
@param deep the deep cloner function
@return ptr to clone
*/
static VArray*
vadeepclone(VArray* va, void (deepclone)(void* dstelem, void* srcelem, void* v))
{
    VArray* clone = NULL;
    clone = (VArray*)calloc(1,sizeof(VArray));
    assert(va != NULL);
    *clone = *va; /* copy the fields */
    /* Now fix up alloc'd fields */
    clone->elemnul = (void*)calloc(1,clone->elemsize);
    assert(clone->elemnul != NULL);
    if(clone->content != NULL) {
        size_t i;
        clone->content = (void*)calloc(clone->elemsize,clone->alloc);
        assert(clone->content != NULL);
        for(i=0;i<clone->length;i++) {
            size_t offset = i*clone->elemsize;
            deepclone(clone->content+offset,va->content+offset,(void*)va);
        }
    }
    return clone;
}

/**
Shallow clone a VArray object.
@param va the variable-length object to clone
@return ptr to clone
*/
static VArray*
vaclone(VArray* va)
{
    return vadeepclone(va,shallowclone);
}

/**************************************************/
/* List (Array of void*) implementation */

/* typedef is in types.h */

static VList* vlnew(void) {return (VList*)vanew(sizeof(void*));}
static void vlfree(VList* vl) {vafree((VArray*)vl);}
static void vlfreeall(VList* vl) {vafreeall((VArray*)vl);}
static void* vlget(VList* vl, size_t pos) {return *((void**)vagetp((VArray*)vl,pos));}
static void vlappend(VList* vl, const void* elem) {vaappend((VArray*)vl,(void**)&elem);}

/*************************/
/* "Inlined" */
#define vlcontents(vl)  ((vl)==NULL?(void**)NULL:(void**)((VArray*)(vl))->content)
#define vllength(vl)  ((vl)==NULL?0:((VArray*)(vl))->length)
#define vlalloc(vl)  ((vl)==NULL?0:((VArray*)(vl))->alloc)
#define vlcat(vl,s)  vlappendn(vl,s,0)
#define vlclear(vl)  vlsetlength(vl,0)
#define vlpush(vl,elem) vlappend(vl,elem)

/**************************************************/
/* Sequence of utf8/ascii characters */

/* typedef is in types.h */

static VString* vsnew(void) {return (VString*)vanew(sizeof(char));}
static void vsfree(VString* vs) {vafree((VArray*)vs);}
static void vssetalloc(VString* vs, size_t minalloc) {vasetalloc((VArray*)vs,minalloc);}
static void vssetlength(VString* vs, size_t newlen) {vasetlength((VArray*)vs,newlen);}
static void vsappendn(VString* vs, const char* elem, size_t n) {vaappendn((VArray*)vs,(void*)elem,n);}
static void vsappend(VString* vs, char elem) {vaappend((VArray*)vs,(void*)&elem);}
static void vsinsertn(VString* vs, size_t pos, const char* elems, size_t elen) {vainsertn((VArray*)vs,pos,(void*)elems,elen);}
static void vsremoven(VString* vs, size_t pos, size_t elide) {varemoven((VArray*)vs,pos,elide);}
static char* vsextract(VString* vs) {return (char*)vaextract((VArray*)vs);}
static char* vsgetp(VString* vs, size_t pos) {return (char*)vagetp((VArray*)vs,pos);}
static VString* vsclone(VString* vs) {return (VString*)vaclone((VArray*)vs);}
static void vsindexset(VString* vs, size_t pos) {vaindexset((VArray*)vs,pos);}
static char* vsindexskip(VString* vs, size_t skip) {return (char*)vaindexskip((VArray*)vs,skip);}
static size_t vsindex(VString* vs) {return vaindex((VArray*)vs);}
static char* vsindexp(VString* vs) {return (char*)vaindexp((VArray*)vs);}
static void vsindexinsertn(VString* vs, const void* seq, size_t slen) {vaindexinsertn((VArray*)vs,seq,slen);}
static void vsindexappendn(VString* vs, const char* elem, size_t n) {vaindexappendn((VArray*)vs,(void*)elem,n);}

/*************************/
/* "Inlined" */
#define vscontents(vs)  ((vs)==NULL?(char*)(NULL):(char*)((VArray*)(vs))->content)
#define vslength(vs)  ((vs)==NULL?0:((VArray*)(vs))->length)
#define vsalloc(vs)  ((vs)==NULL?0:((VArray*)(vs))->alloc)
#define vscat(vs,s)  vsappendn(vs,s,0)
#define vsclear(vs)  vssetlength(vs,0)
#define vspush(vs,elem) vsappend(vs,elem)

/**************************************************/
/* Following are specific to VString */


/**************************************************/

/* Hack to suppress compiler warnings about selected unused static functions */
static void
vasuppresswarnings(void)
{
    void* ignore;
    ignore = (void*)vasuppresswarnings;
    (void)ignore;
    ignore = (void*)vssetalloc;
    ignore = (void*)vsindexinsertn;
    ignore = (void*)vsextract;
}
