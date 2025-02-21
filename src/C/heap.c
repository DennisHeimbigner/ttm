#define Bensure(bb,n) {if(Bavail(bb) < n) Bextend(bb,n);}
/**
Ensure at least the amount of space required, including a trailing NUL
*/
static void
Bextend(Buffer* bb, unsigned int need)
{
    char* newcontent;
    unsigned int oldalloc;
    
    assert(bb != NULL);
    need++; /* for trailing NUL */
    if(Bavail(bb) >= need) return;
    newalloc = bb->alloc;
    if(newalloc == 0) newalloc = BMINBUFSIZE;
    while((newalloc - bb->length) < need) newalloc *= 2;
    if(bb->content != NULL)
	realloc(bb->content,newalloc);
    else
	bb->content = malloc(newalloc);
    if(bb->content == NULL) outofmemory();	
    bb->alloc = newalloc;
    bb->content[bb->length] = '\0';
}    

