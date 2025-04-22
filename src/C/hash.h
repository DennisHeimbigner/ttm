	/**************************************************/
/**
HashTable Management:
The table is only pseudo-hash simplified by making it an array
of chains indexed by the low order n bits of the name[0].  The
hashcode is just the simple sum of the characters in the name
shifted by 1 bit each.
*/

/* Define a hash computing macro */
static unsigned
computehash(const utf8* name)
{
    unsigned hash;
    const utf8* p;
    for(hash=0,p=name;*p!=NUL8;p++) hash = hash + (*p <<1);
    if(hash==0) hash=1;
    return hash;
}

/* Locate a named entry in the hashtable;
   return 1 if found; 0 otherwise.
   prev is a pointer to HashEntry "before" the found entry.
*/

static int
hashLocate(struct HashTable* table, const utf8* name, struct HashEntry** prevp)
{
    struct HashEntry* prev;
    struct HashEntry* next;
    unsigned index;
    unsigned hash;

    hash = computehash(name);
    if(!(table != NULL && name != NULL))
    assert(table != NULL && name != NULL);
    index = (((unsigned)(name[0])) & HASHTABLEMASK);
    prev = &table->table[index];
    next = prev->next;
    while(next != NULL) {
	if(next->hash == hash
	   && strcmp((char*)name,(char*)next->name)==0)
	    break;
	prev = next;
	next = next->next;
    }
    if(prevp) *prevp = prev;
    return (next == NULL ? 0 : 1);
}

/* Remove an entry specified by argument 'entry'.
   Assumes that the predecessor of entry is specified
   by 'prev' as returned by hashLocate.
*/

static void
hashRemove(struct HashTable* table, struct HashEntry* prev, struct HashEntry* entry)
{
   assert(table != NULL && prev != NULL && entry != NULL);
   assert(prev->next == entry); /* validate the removal */
   prev->next = entry->next;
}


/* Insert an entry specified by argument 'entry'.
   Assumes that the predecessor of entry is specified
   by 'prev' as returned by hashLocate.
*/

static void
hashInsert(struct HashTable* table, struct HashEntry* prev, struct HashEntry* entry)
{
   assert(table != NULL && prev != NULL && entry != NULL);
   assert(entry->hash != 0);
   entry->next = prev->next;
   prev->next = entry;
}

static void*
hashwalk(struct HashTable* table)
{
    struct HashWalk* walker = NULL;
    if((walker = calloc(sizeof(struct HashWalk),1))==NULL) return NULL;
    assert(walker != NULL);
    walker->table = table;
    walker->chain = 0;
    walker->entry = walker->table->table[walker->chain].next;
    return (void*)walker;
}

static void
hashwalkstop(void* walkstate)
{
    if(walkstate != NULL) {
	free(walkstate);
    }
}

static int
hashnext(void* walkstate, struct HashEntry** ithentryp)
{
    struct HashWalk* walker = (struct HashWalk*)walkstate;
    while(walker->entry == NULL) { /* Find next non-empty chain */
	walker->chain++;
	if(walker->chain >= HASHSIZE) return 0; /* No more chains to search */
	walker->entry = walker->table->table[walker->chain].next;
    }
    if(ithentryp) *ithentryp = walker->entry;
    walker->entry = walker->entry->next;
    return 1;
}

static void
clearHashEntry(struct HashEntry* entry)
{
    if(entry == NULL) return;
    nullfree(entry->name);
    memset(entry,0,sizeof(struct HashEntry));
}

