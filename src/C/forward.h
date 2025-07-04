/* ttmX.c Core execution functions */
static TTMERR scan(TTM* ttm);
static TTMERR collectargs(TTM* ttm, Frame* frame);
static TTMERR exec(TTM* ttm);
static TTMERR call(TTM* ttm, Frame* frame, char* body, VString* result);
static TTMERR printstring(TTM* ttm, const char* s8, TTMFILE* output);
static char* cleanstring(const char* s8, char* ctrls, size_t* pfinallen);
static Function* getdictstr(TTM* ttm,const Frame* frame,size_t argi);
static TTMERR execcmd(TTM* ttm, const char* cmd);
static void lockup(TTM* ttm);
static const char* sv(Function* f);
static const char* peek(VString* vs, size_t n);
static size_t rptocp(TTM* ttm, const char* u8, size_t rp);
static size_t cptorp(TTM* ttm, const char* u8, size_t cp);
static int tfcvt(const char*);
static void initTTM();
static void usage(const char* msg);
static TTMERR readline(TTM* ttm, TTMFILE* f, char** linep);
static TTMERR readfile(TTM* ttm, const char* fname, VString* buf);
static char* unescape(const char* s8);
static void setproperty(TTM*, const char* key, const char* value);
static void syncproperty(TTM* ttm, const char* key, const char* value);
static const char* propdfalt2str(enum PropEnum dfalt, size_t n);
static size_t propdfalt(enum PropEnum key);
static void defaultproperties(TTM* ttm);
static void cmdlineproperties(TTM* ttm);

static int u8sizec(char c);
static int u8size(const char* cp);
static int u8validcp(char* cp);
static void ascii2u8(char c, char* u8);
static int u8equal(const char* c1, const char* c2);
static int memcpycp(char* dst, const char* src);
static const char* u8ithcp(const char* base, size_t n);
static int u8ith(const char* base, size_t n);
static const char* u8backup(const char* p0, const char* base);
static TTMERR u8peek(char* s, size_t n, char* cpa);
static TTMERR strsubcp(const char* sstart, size_t send, size_t* pncp);
static const char* strchr8(const char* s, const char* cp);
static const char* strstr8(const char* s, const char* pattern);
/* ttmX.c Utility functions */
static unsigned computehash(const char* name);
static int hashLocate(struct HashTable* table, const char* name, struct HashEntry** prevp);
static void hashRemove(struct HashTable* table, struct HashEntry* prev, struct HashEntry* entry);
static void hashInsert(struct HashTable* table, struct HashEntry* prev, struct HashEntry* entry);
static void* hashwalk(struct HashTable* table);
static void hashwalkstop(void* walkstate);
static int hashnext(void* walkstate, struct HashEntry** ithentryp);
static Function* dictionaryLookup(TTM* ttm, const char* name);
static Function* dictionaryRemove(TTM* ttm, const char* name);
static int dictionaryInsert(TTM* ttm, Function* fcn);
static Charclass* charclassLookup(TTM* ttm, const char* name);
static Charclass* charclassRemove(TTM* ttm, const char* name);
static int charclassInsert(TTM* ttm, Charclass* cl);
static const char* propertyLookup(TTM* ttm, const char* name);
static Property* propertyRemove(TTM* ttm, const char* name);
static int propertyInsert(TTM* ttm, const char* key, const char* value);
static TTM* newTTM(struct Properties*);
static void freeTTM(TTM* ttm);
static void clearHashEntry(struct HashEntry* entry);
static Frame* pushFrame(TTM* ttm);
static void popFrame(TTM* ttm);
static void clearFrame(TTM* ttm, Frame* frame);
static void clearArgv(char** argv, size_t argc);
static void clearFramestack(TTM* ttm);
static Function* newFunction(TTM* ttm, const char* name);
static void resetFunction(TTM* ttm, Function* f);
static void freeFunction(TTM* ttm, Function* f);
static void clearDictionary(TTM* ttm, struct HashTable* dict);
static Charclass* newCharclass(TTM* ttm, const char* name);
static void freeCharclass(TTM* ttm, Charclass* cl);
static void clearcharclasses(TTM* ttm, struct HashTable* charclasses);
static const char* charclassmatch(const char* cp, const char* charclass, int negative);
static enum PropEnum propenumdetect(const char* s);
static enum TTMEnum ttmenumdetect(const char* s);
static enum MetaEnum metaenumdetect(const char* s);

/* ttmX.c IO utilities */
static int ttmgetc8(TTM* ttm, TTMFILE* f, char* cp8);
static int ttmnonl(TTM* ttm, TTMFILE* f, char* cp8);
static void ttmpushbackc(TTM* ttm, TTMFILE* f, char* cp8);
static int ttmputc8(TTM* ttm, const char* c8, TTMFILE* f);
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
static int hashLocate(struct HashTable* table, const char* name, struct HashEntry** prevp);
static void hashRemove(struct HashTable* table, struct HashEntry* prev, struct HashEntry* entry);
static void hashInsert(struct HashTable* table, struct HashEntry* prev, struct HashEntry* entry);
static void* hashwalk(struct HashTable* table);
static void hashwalkstop(void* walkstate);
static int hashnext(void* walkstate, struct HashEntry** ithentryp);

static TTMERR ttm_ap(TTM* ttm, Frame* frame, VString*);
static TTMERR ttm_ap(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_cf(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_ds(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_es(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_subst(TTM* ttm, VString* text, const char* pattern, size_t segindex, size_t* segcountp);
static TTMERR ttm_sc(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_ss(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_cr(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_cc(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_cn(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_sn(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_isc(TTM* ttm, Frame* frame, VString* vsresult);
static TTMERR ttm_scn(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_cp(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_cs(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_rrp(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_eos(TTM* ttm, Frame* frame, VString* vsresult);
static TTMERR ttm_gn(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_zlc(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_zlcp(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_flip(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_trl(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_thd(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_dcl0(TTM* ttm, Frame* frame, int negative, VString* result);
static TTMERR ttm_dcl(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_dncl(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_ecl(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_ccl(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_scl(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_tcl(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_abs(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_ad(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_dv(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_dvr(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_mu(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_su(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_eq(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_gt(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_lt(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_ge(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_le(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_eql(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_gtl(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_ltl(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_ps0(TTM* ttm, TTMFILE* target, int argc, char** argv, VString* result);
static TTMERR ttm_ps(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_rs0(TTM* ttm, TTMFILE* target, VString* result);
static TTMERR ttm_rs(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_psr(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_cm(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_vprintf(TTM* ttm, TTMFILE* target, const char* fmt8, size_t argc, char** argv, VString* result);
static TTMERR ttm_fprintf(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_printf(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_pf(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_tru(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_tdh(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_rp(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_srp(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_names(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_exit(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_ndf(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_norm(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_time(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_xtime(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_ctime(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_tf(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_tn(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_argv(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_argc(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_classes(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_lf(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_uf(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_istn(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_pn(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_trim(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_catch(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_switch(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_clearpassive(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_include(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_void(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_wd(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_breakpoint(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_setprop(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_resetprop(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_getprop(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_removeprop(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_properties(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_sort(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_ttm_meta(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_ttm_info_name(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_ttm_info_class(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_ttm_info_string(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_ttm_list(TTM* ttm, Frame* frame, VString* result);
static TTMERR ttm_ttm(TTM* ttm, Frame* frame, VString* result);

static TTMFILE* ttmopen(TTM* ttm, const char* fname, const char* mode);
static int ttmclose(TTM* ttm, TTMFILE* tfile);
static int ttmerror(TTM* ttm, TTMFILE* tfile);
static int ttmeof(TTM* ttm, TTMFILE* tfile);
static TTMFILE* ttmfindfile(TTM* ttm, const char* filename);
static size_t ttmgetemptyfileno(TTM* ttm);
static void ttmrmfile(TTM* ttm, const char* filename);

/* Debug.h */
static void ttmbreak(TTMERR err);
static TTMERR ttmthrow(TTM* ttm, TTMERR err, const char* file, const char* fcn, int line);
static void dumpframe(TTM* ttm, Frame* frame);
static void dumpstack(TTM* ttm);
static void traceframe(TTM* ttm, Frame* frame, int traceargs);
static void trace1(TTM* ttm, TTMERR err, int depth, int entering, int tracing);
static void trace(TTM* ttm, TTMERR err, int entering, int tracing);
static void fail(TTM* ttm, TTMERR eno, const char* fcn, int line);
static TTMERR failx(TTM* ttm, TTMERR eno, const char* fcn, int line, const char* fmt, ...);
static TTMERR failxcxt(TTM* ttm, TTMERR eno, const char* fcn, int line);
static const char* ttmerrmsg(TTMERR err);
static const char* ttmerrname(TTMERR err);

#ifdef GDB
static void dumpnames(TTM* ttm);
static void dumpcharclasses(TTM* ttm);
static void dumpprops(TTM* ttm);
static const char* printwithpos(VString* vs);
static const char* printwithpos(VString* vs);
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
    (void)ttmrmfile;
#ifdef GDB
    (void)print2len;
#endif
}
