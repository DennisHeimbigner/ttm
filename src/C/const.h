/**
This code takes over part of the UTF8 space to store segment
marks and create marks.	 Specifically a segment mark (and
creation mark) are encoded as two bytes: 0x7f,0xB0..0xBF.
The first byte, the ascii DEL character, is assumed to not be used
in real text and is suppressed by the code for reading external data.
It indicates the start of a segment mark.
The second byte is a utf8 continuation code, and so is technically illegal.
The second byte encodes the segment mark index as the continuation code in
the range: 0xB0 .. 0xBF.
This second byte is divided as follows:
1. The (singleton) create mark index: 0xBF, so complete create mark is (0x7F,0xB0)
2. The 62 segment marks: 0xB1 .. 0xBF), so complete create mark is (0x7F,0xB1..0xBF)
*/

/* Segment mark contants (term "mark" comes from gin CalTech TTM). */
#define SEGMARKSIZE 2 /*bytes*/
#define SEGMARK0 (0x7f )	/*Signal presence of a segment mark */
#define CREATEMARK0 SEGMARK0	/*Signal presence of a create mark */
/* Segment index constants */
#define SEGINDEXFIRST   1 /* Index of the lowest segmark */
#define CREATEINDEXONLY 0 /* Index for the singular create mark */
/* Masks for set/clr the continuation bits */
#define SEGMARKINDEXMASK ((utf8)0x80)
#define SEGMARKINDEXUNMASK ((utf8)0x3F)
/* Misc */
#define CREATELEN 4 /* # of characters for a create mark */
#define CREATEFORMAT "%04u"
#define MAXSEGMARKS 62

#define empty_segmark {SEGMARK0,segmarkindexbyte(SEGINDEXFIRST)}

#define MAXARGS       63
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

/* When encountered during scan, these non-printable characters are ignored: Must be ASCII */
//#define NONPRINTIGNORE "\001\002\003\004\005\006\a\b\n\r\016\017\020\021\022\023\024\025\026\027\030\031\032\e\034\035\036\037\177"
#define NONPRINTIGNORE "\001\002\003\004\005\006\a\b\r\016\017\020\021\022\023\024\025\026\027\030\031\032\e\034\035\036\037\177"

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

