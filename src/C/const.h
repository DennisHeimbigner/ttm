/**
This code takes over part of the UTF8 space to store segment
marks and create marks This should work because we ever export
segmarks outside of TTM.

Specifically a segment mark (and creation mark) is encoded as a three
byte sequence, which uses a lead ascii character followed by 2
continuation characters.

The three bytes are as follows:
1. 0x7f        (binary 0111 1111)
2. 0x80 - 0xBF (binary 10xx xxxx)
3. 0x80 - 0xBF (binary 10xx xxxx)

The segment index is represented by the x characters,
so the segment index is 6+6 = 12 bits, representing 0 - 4095
The value 0 is taken over to represent the creation mark, so
there is room for 4094 segment marks. As a space saving measure
the set of legal mark indices is limited to 1 - 1023.
*/

/* Segment mark contants (term "mark" comes from gin CalTech TTM). */
#define SEGMARKSIZE 3  /*bytes*/
#define SEGMARK0 (0x7F )	/*Signal presence of a segment mark */
#define CREATEMARK0 SEGMARK0	/*Signal presence of a create mark */
/* Segment index constants */
#define SEGINDEXFIRST   1 /* Index of the lowest segmark */
#define CREATEINDEXONLY 0 /* Index for the singular create mark */
/* Masks for set/clr the continuation bits */
#define SEGMARKINDEXMASK ((size_t)0x80)
#define SEGMARKINDEXUNMASK ((size_t)0x3F)
#define SEGMARKINDEXSHIFT 6
/* Misc */
#define CREATELEN 4 /* # of characters for a create value (not the mark */
#define CREATEFORMAT "%04u"
#define MAXSEGMARKS 1023

#define empty_segmark {SEGMARK0,0x80,0x80}

#define MAXARGS       MAXSEGMARKS

#define ARB           MAXARGS
#define MAXINCLUDES   64
#define MAXINTCHARS   32
#define MAXFRAMEDEPTH 1024

#define NUL8 '\0'
#define COMMA ','
#define LPAREN '('
#define RPAREN ')'
#define LBRACKET '['
#define RBRACKET ']'

/* When encountered during scan and the frame stack is empty, then these non-printable characters are ignored: Must be ASCII */
/* Note: NPI stands for non-printable ignore */
#define NPI "\001\002\003\004\005\006\a\b\f\r\016\017\020\021\022\023\024\025\026\027\030\031\032\e\034\035\036\037\177"

/* Like NPI, but only ignored at depth 0 */
#define NPIDEPTH0 "\n" NPI

/* These non-printable characters are not ignored */
#define NONPRINTKEEP = "\t\c\f"

#define DFALTSTACKSIZE 64
#define DFALTEXECCOUNT (1<<20)
#define DFALTSHOWFINAL 1
#define DFALTTRACE 0
#define DFALTVERBOSE 1
#define CONTEXTLEN 20

/*Mnemonics*/
#define TRACING 1
#define TOEOS ((size_t)0xffffffffffffffff)

#if DEBUG > 0
#define PASSIVEMAX 20
#define ACTIVEMAX 20
#endif

/* Max number of pushback codepoints for ttmgetc8 */
#define MAXPUSHBACK 4 /*codepoints */

#define METACHARS "#<;>" /* Fails if these chars were changed */

/* Experimental on Windows */
#define DEVTTY "/dev/tty"

