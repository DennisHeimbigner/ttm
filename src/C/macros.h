/**************************************************/
/* Misc Utility Macros */

/* Watch out: x is evaluated multiple times */
#define nullfree(x) do{if(x) free(x);}while(0)
#define nulldup(x) ((x)?strdup(x):(x))
#define UNUSED(x) (void)x

/**************************************************/
/* The reason for these macros is to get around the fact that changing ttm->vs.active->index invalidates cp8 pointers */
/* The reason for these macros is to get around the fact that changing ttm->vs.active->index invalidates cp8 pointers */
#define TTMINIT(cp8) do{ \
	(cp8)=(utf8*)vsindexp(ttm->vs.active); \
	}while(0)	/* ttm->vs.active->index->cp8 */
#define TTMGETPTR(cp8) do{ \
	(cp8)=(utf8*)vsindexp(ttm->vs.active); \
	}while(0)	/* ttm->vs.active->index->cp8 */
#define TTMSETPTR(pcp8) do{ \
	vsindexskip(ttm->vs.active, \
		((cp8)-(utf8*)vsindexp(ttm->vs.active)));  \
	}while(0)	/* cp8->ttm->vs.active->index */

/* use frame stack  to track depth */
#define frameresult(ttm) (ttm->frames.top >= 0 ? ttm->frames.stack[ttm->frames.top].result : ttm->vs.passive)
#define frameparent(ttm) (ttm->frames.top > 0 ? ttm->frames.stack[ttm->frames.top-1].result : ttm->vs.passive)

/**************************************************/
/* Macro Functions */

#ifdef CATCH
#define THROW(err) ttmthrow(err)
#else
#define THROW(err) (err)
#endif

#define FAILX(ttm,eno,fmt,...) THROW(failx(ttm,eno,__FILE__,__LINE__,fmt  __VA_OPT__(,) __VA_ARGS__))
#define FAIL(ttm,eno) fail(ttm,eno,__FILE__,__LINE__)

/**************************************************/
/* "inline" functions */

#define isnul(cp)(*(cp) == NUL8?1:0)
#define isnulc(c)((c) == NUL8?1:0)
#define isescape(cp) u8equal(cp,ttm->meta.escapec)
#define isascii(cp) (*cp <= 0x7F)

#define issegmark(cp8) (((cp8)[0]) == SEGMARK0)
#define segmarkindex(cp8) (((((size_t)(cp8)[1]) & SEGMARKINDEXUNMASK) << SEGMARKINDEXSHIFT) | (((size_t)((cp8)[2])) & SEGMARKINDEXUNMASK))
#define segmark2utf8(cp8,idx) do{(cp8)[0] = SEGMARK0; \
	(cp8)[1] = (((((size_t)(idx))>>SEGMARKINDEXSHIFT) & SEGMARKINDEXUNMASK) | SEGMARKINDEXMASK); \
	(cp8)[2] = ((((size_t)(idx)) & SEGMARKINDEXUNMASK) | SEGMARKINDEXMASK); \
	} while(0)

#define iscreateindex(idx) (((idx) == CREATEINDEXONLY)?1:0)
#define iscreatemark(cp) (iscreateindex(segmarkindex(cp)))
#define createmarkindex(cp8) ((unsigned)(*(cp8) & CREATEMARKINDEXUNMASK))
#define createmarkindexcp(index) ((utf8)(((index)&CREATEMARKINDEXUNMASK)|CREATEMARKINDEXMASK))
    
#define iscontrol(c) ((c) < ' ' || (c) == 127 ? 1 : 0)
#define isdec(c) (((c) >= '0' && (c) <= '9') ? 1 : 0)
#define ishex(c) ((c >= '0' && c <= '9') \
		  || (c >= 'a' && c <= 'f') \
		  || (c >= 'A' && c <= 'F') ? 1 : 0)

#define fromhex(c) \
    (c >= '0' && c <= '9' \
	? (c - '0') \
	: (c >= 'a' && c <= 'f' \
	    ? ((c - 'a') + 10) \
	    : (c >= 'A' && c <= 'F' \
		? ((c - 'a') + 10) \
		: -1)))

#define FAILNONAME(i)  FAILNONAMES(frame->argv[i])
#define FAILNOCLASS(i) FAILNOCLASSS(frame->argv[i])
#define FAILNONAMES(s)	FAILX(ttm,TTM_ENONAME,"Missing dict name=%s\n",(const char*)(s))
#define FAILNOCLASSS(s) FAILX(ttm,TTM_ENOCLASS,"Missing class name=%s\n",(const char*)(s))

