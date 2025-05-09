/* ttmX.c Core execution functions */
static TTMERR scan(TTM* ttm);
static TTMERR testfcnprefix(TTM* ttm, enum FcnCallCases* pfcncase);
static TTMERR collectargs(TTM* ttm, Frame* frame);
static void propagateresult(TTM* ttm, Frame* frame);
static TTMERR exec(TTM* ttm, Frame* frame);
static TTMERR call(TTM* ttm, Frame* frame, VString*, VString* body);
static TTMERR processfcn(TTM* ttm);
static TTMERR printstring(TTM* ttm, const utf8* s8, TTMFILE* output);
static utf8* cleanstring(const utf8* s8, char* ctrls, size_t* pfinallen);
static Function* getdictstr(TTM* ttm,const Frame* frame,size_t argi);
static TTMERR execcmd(TTM* ttm, const char* cmd);
static void lockup(TTM* ttm);
static TTMERR collectescaped(TTM* ttm, VString* dst);
static const char* sv(Function* f);
static const utf8* peek(VString* vs, size_t n, in* pncp);
static size_t rptocp(TTM* ttm, const utf8* u8, size_t rp);
static size_t cptorp(TTM* ttm, const utf8* u8, size_t cp);
static void initglobals();
static void usage(const char* msg);
static TTMERR readline(TTM* ttm, TTMFILE* f, utf8** linep);
static TTMERR readfile(TTM* ttm, const char* fname, VString* buf);
static utf8* unescape(const utf8* s8);
static struct Properties propertiesclone(struct Properties* props);
static void setproperty(const char* tag, const char* value, struct Properties* props);
static void resetproperty(struct Properties* props, const char* key, const struct Properties* dfalts);
static int u8sizec(utf8 c);
static int u8size(const utf8* cp);
static int u8validcp(utf8* cp);
static void ascii2u8(char c, utf8* u8);
static int u8equal(const utf8* c1, const utf8* c2);
static int memcpycp(utf8* dst, const utf8* src);
static const utf8* u8ithcp(const utf8* base, size_t n);
static int u8ith(const utf8* base, size_t n);
static const utf8* u8backup(const utf8* p0, const utf8* base);
static TTMERR strsubcp(const utf8* sstart, size_t send, size_t* pncp);
static TTMERR u8peek(utf8* s, size_t n, utf8* cpa);
/* ttmX.c Utility functions */
static unsigned computehash(const utf8* name);
static int hashLocate(struct HashTable* table, const utf8* name, struct HashEntry** prevp);
static void hashRemove(struct HashTable* table, struct HashEntry* prev, struct HashEntry* entry);
static void hashInsert(struct HashTable* table, struct HashEntry* prev, struct HashEntry* entry);
static void* hashwalk(struct HashTable* table);
static void hashwalkstop(void* walkstate);
static int hashnext(void* walkstate, struct HashEntry** ithentryp);
static Function* dictionaryLookup(TTM* ttm, const utf8* name);
static Function* dictionaryRemove(TTM* ttm, const utf8* name);
static int dictionaryInsert(TTM* ttm, Function* fcn);
static Charclass* charclassLookup(TTM* ttm, const utf8* name);
static Charclass* charclassRemove(TTM* ttm, const utf8* name);
static int charclassInsert(TTM* ttm, Charclass* cl);
static TTM* newTTM(struct Properties*);
static void freeTTM(TTM* ttm);
static void clearHashEntry(struct HashEntry* entry);
static Frame* pushFrame(TTM* ttm);
static void popFrame(TTM* ttm);
static void clearFrame(TTM* ttm, Frame* frame);
static void clearArgv(utf8** argv, size_t argc);
static void clearFramestack(TTM* ttm);
static Function* newFunction(TTM* ttm);
static void freeFunction(TTM* ttm, Function* f);
static void clearDictionary(TTM* ttm, struct HashTable* dict);
static Charclass* newCharclass(TTM* ttm);
static void freeCharclass(TTM* ttm, Charclass* cl);
static void clearCharclasses(TTM* ttm, struct HashTable* charclasses);
static int charclassMatch(utf8* cp, utf8* charclass);
static enum PropEnum propenumdetect(const utf8* s);
static enum TTMEnum ttmenumdetect(const utf8* s);
#if 0
static enum MetaEnum metaenumdetect(const utf8* s);
#endif

/* ttmX.c IO utilities */
static int ttmgetc8(TTM* ttm, TTMFILE* f, utf8* cp8);
static int ttmnonl(TTM* ttm, TTMFILE* f, utf8* cp8);
static void ttmpushbackc(TTM* ttm, TTMFILE* f, utf8* cp8);
static int ttmputc8(TTM* ttm, const utf8* c8, TTMFILE* f);
static void xprintf(TTM*,const char* fmt,...);
static void vxprintf(TTM*,const char* fmt, va_list ap);
static TTMERR setupio(TTM* ttm, const char* infile, const char* outfile);
static void closeio1(TTM* ttm, TTMFILE* f);
static void closeio(TTM* ttm);

#ifdef GDB
static const char* print2len(VString* vs);
static const char* printsubseq(VString* vs, size_t argstart , size_t argend);
#endif

#ifdef HAVE_MEMMOVE
static void memmovex(char* dst, char* src, size_t len);
#else
#define memmovex(dst,src,len) memmove(dst,src,len)
#endif

/* HashTable operations */
static int hashLocate(struct HashTable* table, const utf8* name, struct HashEntry** prevp);
static void hashRemove(struct HashTable* table, struct HashEntry* prev, struct HashEntry* entry);
static void hashInsert(struct HashTable* table, struct HashEntry* prev, struct HashEntry* entry);
static void* hashwalk(struct HashTable* table);
static void hashwalkstop(void* walkstate);
static int hashnext(void* walkstate, struct HashEntry** ithentryp);

static TTMERR ttm_ap(TTM* ttm, Frame* frame, VString*);
static TTMERR ttm_cf(TTM* ttm, Frame* frame, VString*);
static TTMERR ttm_cr(TTM* ttm, Frame* frame, VString*);
static TTMERR ttm_ds(TTM* ttm, Frame* frame, VString*);
static TTMERR ttm_es(TTM* ttm, Frame* frame, VString*);
static TTMERR ttm_sc(TTM* ttm, Frame* frame, VString*);
static TTMERR ttm_ss(TTM* ttm, Frame* frame, VString*);
static TTMERR ttm_ss0(TTM* ttm, Frame* frame, size_t segmark, size_t* segcountp);
static size_t ttm_mark1(TTM* ttm, VString* target, utf8* pattern, size_t mark);
static TTMERR ttm_cc(TTM* ttm, Frame* frame, VString*);
static TTMERR ttm_cn(TTM* ttm, Frame* frame, VString*);
static TTMERR ttm_sn(TTM* ttm, Frame* frame, VString*);
static TTMERR ttm_isc(TTM* ttm, Frame* frame, VString*);
static TTMERR ttm_scn(TTM* ttm, Frame* frame, VString*);
static TTMERR ttm_cp(TTM* ttm, Frame* frame, VString*);
static TTMERR ttm_cs(TTM* ttm, Frame* frame, VString*);
static TTMERR ttm_rrp(TTM* ttm, Frame* frame, VString*);
static TTMERR ttm_eos(TTM* ttm, Frame* frame, VString*);
static TTMERR ttm_gn(TTM* ttm, Frame* frame, VString*);
static TTMERR ttm_zlc(TTM* ttm, Frame* frame, VString*);
static TTMERR ttm_zlcp(TTM* ttm, Frame* frame, VString*);
static TTMERR ttm_flip(TTM* ttm, Frame* frame, VString*);
static TTMERR ttm_dcl0(TTM* ttm, Frame* frame, int negative, VString*);
static TTMERR ttm_dcl(TTM* ttm, Frame* frame, VString*);
static TTMERR ttm_dncl(TTM* ttm, Frame* frame, VString*);
static TTMERR ttm_ecl(TTM* ttm, Frame* frame, VString*);
static TTMERR ttm_ccl(TTM* ttm, Frame* frame, VString*);
static TTMERR ttm_scl(TTM* ttm, Frame* frame, VString*);
static TTMERR ttm_tcl(TTM* ttm, Frame* frame, VString*);
static TTMERR ttm_abs(TTM* ttm, Frame* frame, VString*);
static TTMERR ttm_ad(TTM* ttm, Frame* frame, VString*);
static TTMERR ttm_dv(TTM* ttm, Frame* frame, VString*);
static TTMERR ttm_dvr(TTM* ttm, Frame* frame, VString*);
static TTMERR ttm_mu(TTM* ttm, Frame* frame, VString*);
static TTMERR ttm_su(TTM* ttm, Frame* frame, VString*);
static TTMERR ttm_eq(TTM* ttm, Frame* frame, VString*);
static TTMERR ttm_gt(TTM* ttm, Frame* frame, VString*);
static TTMERR ttm_lt(TTM* ttm, Frame* frame, VString*);
static TTMERR ttm_eql(TTM* ttm, Frame* frame, VString*);
static TTMERR ttm_gtl(TTM* ttm, Frame* frame, VString*);
static TTMERR ttm_ltl(TTM* ttm, Frame* frame, VString*);
static TTMERR ttm_ps(TTM* ttm, Frame* frame, VString*);
static TTMERR ttm_rs(TTM* ttm, Frame* frame, VString*);
static TTMERR ttm_psr(TTM* ttm, Frame* frame, VString*);
static TTMERR ttm_cm(TTM* ttm, Frame* frame, VString*);
static TTMERR ttm_pf(TTM* ttm, Frame* frame, VString*);
static TTMERR ttm_names(TTM* ttm, Frame* frame, VString*);
static TTMERR ttm_exit(TTM* ttm, Frame* frame, VString*);
static TTMERR ttm_ndf(TTM* ttm, Frame* frame, VString*);
static TTMERR ttm_norm(TTM* ttm, Frame* frame, VString*);
static TTMERR ttm_time(TTM* ttm, Frame* frame, VString*);
static TTMERR ttm_xtime(TTM* ttm, Frame* frame, VString*);
static TTMERR ttm_ctime(TTM* ttm, Frame* frame, VString*);
static TTMERR ttm_tf(TTM* ttm, Frame* frame, VString*);
static TTMERR ttm_tn(TTM* ttm, Frame* frame, VString*);
static TTMERR ttm_argv(TTM* ttm, Frame* frame, VString*);
static TTMERR ttm_argc(TTM* ttm, Frame* frame, VString*);
static TTMERR ttm_classes(TTM* ttm, Frame* frame, VString*);
static TTMERR ttm_lf(TTM* ttm, Frame* frame, VString*);
static TTMERR ttm_uf(TTM* ttm, Frame* frame, VString*);
static TTMERR ttm_include(TTM* ttm, Frame* frame, VString*);
#if 0
static TTMERR ttm_ttm_meta(TTM* ttm, Frame* frame, VString*);
#endif
static TTMERR ttm_ttm_info_name(TTM* ttm, Frame* frame, VString*);
static TTMERR ttm_ttm_info_class(TTM* ttm, Frame* frame, VString*);
static TTMERR ttm_ttm_info_string(TTM* ttm, Frame* frame, VString*);
static TTMERR ttm_ttm(TTM* ttm, Frame* frame, VString*);

static TTMFILE* ttmopen(TTM* ttm, const char* fname, const char* mode);
static int ttmclose(TTM* ttm, TTMFILE* tfile);
static int ttmerror(TTM* ttm, TTMFILE* tfile);
static int ttmeof(TTM* ttm, TTMFILE* tfile);

static void dumpframe(TTM* ttm, Frame* frame);
static void dumpstack(TTM* ttm);
static void traceframe(TTM* ttm, Frame* frame, int traceargs);
static void trace1(TTM* ttm, int depth, int entering, int tracing);
static void trace(TTM* ttm, int entering, int tracing);
static void fail(TTM* ttm, TTMERR eno, const char* fcn, int line);
static TTMERR failx(TTM* ttm, TTMERR eno, const char* fcn, int line, const char* fmt, ...);
static TTMERR failxcxt(TTM* ttm, TTMERR eno, const char* fcn, int line);
static const char* errstring(TTMERR err);

#ifdef GDB
static void dumpdict0(TTM*,struct HashTable* dict, int printvalues);
static void dumpnames(TTM* ttm);
static void dumpcharclasses(TTM* ttm);
static const utf8* printwithpos(VString* vs);
static const utf8* printwithpos(VString* vs);
#endif /*GDB*/

static char* debash(const char* s);

/* Hack to suppress compiler warnings about selected unused static functions */
static void
suppresswarnings(void)
{
    void* ignore;
    ignore = (void*)suppresswarnings;
    (void)ignore;
    (void)cptorp;
#ifdef GDB
    (void)print2len;
#endif
}
