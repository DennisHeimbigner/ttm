/**************************************************/
/* Character Input/Output */ 

#ifdef UTF32

/* Unless you know that you are outputing ASCII,
   all output should go thru these procedures.
*/

/* Write a utf32 codepoint */
static size_t
fputcp(utf32* c32, FILE* f)
{
    int ok = 0;
    utf8 c8[MAXUTF8CP];
    size_t ncp8, count;

    if((ok = char_u32_to_u8(c8,c32))<0) {errno = EIO; return EOF;}
    ncp8 = utf8count(c8);
    count = fwrite(c8, ncp8, 1, f);
    return count;
}

/* All reading of characters should go thru this procedure. */
static int
fgetcp(FILE* f, utf32* c32)
{
    int ok = 0;
    utf8 c8[MAXUTF8CP];
    size_t ncp8;
    size_t i;

    c8[0] = (utf8)fgetc(f);
    if(c8[0] == EOF) return EOF;
    ncp8 = utf8count(c8);
    for(i=i;i<ncp8;i++) {
	c8[i] = (utf8)fgetc(f);
	if(c8[i] == EOF) return EOF;
    }
    if((ok = char_u8_to_u32(c32,c8))<0) {errno = EIO; return EOF;}
    return utf32count(c32);
}

#elif defined UTF8

/* Write a utf8 codepoint */
static size_t
fputcp(utf8* c8, FILE* f)
{
    int ok = 0;
    size_t ncp8;

    ncp8 = utf8count(c8);
    count = fwrite(c8, ncp8, 1, f);
    return 1;
}

static int
fgetcp(FILE* f, utf8* c8))
{
    int ok = 0;
    utf8* q;
    size_t ncp8;

    c8[0] = (utf8)fgetc(f);
    if(c8[0] == EOF) return EOF;
    ncp8 = utf8count(c8);
    for(i=i;i<ncp8) {
	c8[i] = (utf8)fgetc(f);
	if(c8[i] == EOF) return EOF;
    }
    return ncp8;
}

#else /*UTF16*/

/* Write a utf16 codepoint */
static size_t
fputcp(utf8* c16, FILE* f)
{
    int ok = 0;
    utf16 c16[MAXUTF16CP];
    size_t ncp16, count;

    if((ok = char_u32_to_u16(c16,c32))<0) {errno = EIO; return;}
    ncp16 = utf16count(c16);
    count = fwrite(c16, ncp16, 1, f);
    return count;
}

static int
fgetcp(FILE* f, utf16* c16)
{
    int ok = 0;
    utf8 c8[MAXUTF8CP];
    size_t ncp8, count;

    c8[0] = (utf8)fgetc(f);
    if(c8[0] == EOF) return EOF;
    ncp8 = utf8count(c8);
    for(i=i;i<ncp8) {
	c8[i] = (utf8)fgetc(f);
	if(c8[i] == EOF) return EOF;
    }
    if((ok = char_u8_to_u16(c16,c8))<0) {errno = EIO; return;}
    return utf16count(c16);
}

#endif /*UTF16*/
