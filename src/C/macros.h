#ifndef MACROS_H
#define MACROS_H

/**************************************************/
/* Misc Utility Macros */

/* Watch out: x is evaluated multiple times */
#define nullfree(x) do{if(x) free(x);}while(0)
#define nulldup(x) ((x)?strdup(x):(x))
#define UNUSED(x) (void)x

/**************************************************/
/* The reason for these macros is to get around the fact that changing ttm->vs.active->index invalidates cp8 pointers */

#define TTMCP8SET(ttm) do{cp8=vsindexp((ttm)->vs.active); ncp=u8size(cp8);}while(0)
#define TTMCP8NXT(ttm) do{cp8=vsindexskip((ttm)->vs.active,u8size(cp8));ncp=u8size(cp8);}while(0)
#define TTMCP8BACK(ttm) do{\
			char* p8 = vscontents((ttm)->vs.active); \
			size_t newindex; \
			p8 = u8backup(cp8,p8); \
			newindex = vsoffset((ttm)->vs.active,p8); \
			vsindexset((ttm)->vs.active,newindex); \
			cp8 = vsindexp((ttm)->vs.active); \
			ncp = u8size(cp8); \
			}while(0);

/**************************************************/
/* Macro Functions */

#define THROW(ttm,eno) ttmthrow(ttm,eno,__FILE__,__FUNCTION__,__LINE__)
#define THROWMSG(ttm,eno,fmt,...) ttmthrowmsg(ttm,eno,__FILE__,__FUNCTION__,__LINE__,fmt  __VA_OPT__(,) __VA_ARGS__)

#define EXIT(ttm,eno) {err = THROW(ttm,eno); goto done;}
#define EXITMSG(ttm,eno,fmt,...) {err = THROWMSG(ttm,eno,fmt  __VA_OPT__(,) __VA_ARGS__); goto done;}

#define FAILNONAME(i)  FAILNONAMES(frame->argv[i])
#define FAILNOCLASS(i) FAILNOCLASSS(frame->argv[i])
#define FAILNONAMES(s)	EXITMSG(ttm,TTM_ENONAME,"Missing dict name=%s\n",(const char*)(s))
#define FAILNOCLASSS(s) EXITMSG(ttm,TTM_ENOCLASS,"Missing class name=%s\n",(const char*)(s))

/* When an err occurs in a function not returning TTMERR */
#define FATAL(ttm,eno,fmt,...) assert(((void)THROWMSG(ttm,eno,fmt  __VA_OPT__(,) __VA_ARGS__),0))

/**************************************************/
/* "inline" functions */

#define UTF8(c) ((utf8)(c))
#define UTF8P(cp) ((utf8*)(cp))

#define isnul(cp)(*(cp) == NUL8?1:0)
#define isnulc(c)((c) == NUL8?1:0)
#define isescape(cp) u8equal(cp,ttm->meta.escapec)
#define isascii8(cp) (*UTF8P(cp) <= 0x7F)

#define issegmark(cp8) ((UTF8P(cp8)[0]) == SEGMARK0)
#define segmarkindex(cp8) (((((size_t)(UTF8P(cp8)[1])) & SEGMARKINDEXUNMASK) << SEGMARKINDEXSHIFT) | (((size_t)(UTF8P(cp8)[2])) & SEGMARKINDEXUNMASK))

#define segmark2utf8(cp8,idx) do{UTF8P(cp8)[0] = SEGMARK0; \
	UTF8P(cp8)[1] = (((((size_t)(idx))>>SEGMARKINDEXSHIFT) & SEGMARKINDEXUNMASK) | SEGMARKINDEXMASK); \
	UTF8P(cp8)[2] = ((((size_t)(idx)) & SEGMARKINDEXUNMASK) | SEGMARKINDEXMASK); \
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


#define getspecial(sp) (testspecials+((size_t)sp))

#define canonpath(path) do{char* p;for(p=(path);*p;p++){if(*p=='\\'){*p='/'; }}}while(0)
#define wincanonpath(path) do{char* p;for(p=(path);*p;p++){if(*p=='/'){*p='\\'; }}}while(0)

#endif /*MACROS_H*/
