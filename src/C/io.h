static TTMERR
setupio(TTM* ttm, const char* infile, const char* outfile)
{
    TTMERR err = TTM_NOERR;
    TTMFILE* io = NULL;

    assert(ttm != NULL);

    /* Mirror stdin, stdout, and stderr */

    if((io = (TTMFILE*)calloc(sizeof(TTMFILE),1))==NULL) FAIL(ttm,TTM_EMEMORY);
    io->npushed = 0 ;
    if(infile == NULL || strlen(infile) == 0) {
	io->name = strdup("stdin");
	io->file = stdin;
        io->isstd = 1; /* => do not close the FILE* object */
	io->mode = IOM_READ;
    } else {
	io->name = strdup(infile);
	io->file = fopen(infile,"rb");
	if(io->file == NULL) {
	    fprintf(stderr,"File cannot be accessed: %s\n",infile);
	    err = errno;
	    free(io);
	    goto done;
	}
	io->isstd = 0;
	io->mode = IOM_READ;
    }
    ttm->io.stdin = io; io = NULL;

    if((io = (TTMFILE*)calloc(sizeof(TTMFILE),1))==NULL) FAIL(ttm,TTM_EMEMORY);
    io->npushed = 0 ;
    if(outfile == NULL || strlen(infile) == 0) {
	io->name = strdup("stdout");
	io->file = stdout;
        io->isstd = 1; /* => do not close the FILE* object */
	io->mode = IOM_WRITE;
    } else {
	io->name = strdup(outfile);
	io->file = fopen(outfile,"wb+");
	if(io->file == NULL) {
	    fprintf(stderr,"File cannot be accessed: %s\n",outfile);
	    err = errno;
	    free(io);
	    goto done;
	}
	io->isstd = 0;
	io->mode = IOM_WRITE | IOM_APPEND;
    }
    ttm->io.stdout = io; io = NULL;

    if((io = (TTMFILE*)calloc(sizeof(TTMFILE),1))==NULL) FAIL(ttm,TTM_EMEMORY);
    io->npushed = 0 ;
    io->name = strdup("stderr");
    io->file = stderr;
    io->isstd = 1; /* => do not close the FILE* object */
    io->mode = IOM_WRITE | IOM_APPEND;
    ttm->io.stderr = io; io = NULL;

done:
    ttmclose(ttm,io);
    errno = 0; /* reset */
    return err;
}

static void
closeio1(TTM* ttm, TTMFILE* f)
{
    if(ttm != NULL && f != NULL)
	ttmclose(ttm,f);
}

static void
closeio(TTM* ttm)
{
    if(ttm != NULL) {
	closeio1(ttm,ttm->io.stdin); ttm->io.stdin = NULL;
	closeio1(ttm,ttm->io.stdout); ttm->io.stdout = NULL;
	closeio1(ttm,ttm->io.stderr); ttm->io.stderr = NULL;
    }
}

/**************************************************/

static TTMFILE*
ttmopen(TTM* ttm, const char* fname, const char* mode)
{
    int eno = 0;
    TTMFILE* tfile = NULL;
    tfile = (TTMFILE*)calloc(1,sizeof(TTMFILE));
    if(tfile == NULL) FAIL(ttm,TTM_EMEMORY);
    tfile->file = fopen(fname,mode);
    if(tfile->file == NULL) {eno = errno; goto done;}
    tfile->npushed = -1;

done:;
    errno = eno;
    return tfile;
}

static int
ttmclose(TTM* ttm, TTMFILE* tfile)
{
    int ret = 0;
    int eno = 0;
    if(tfile != NULL) {
        if(!tfile->isstd) {
	    ret = fclose(tfile->file); tfile->file = NULL;
	    if(ret == EOF) eno = errno;
	}
        nullfree(tfile->name);
        nullfree(tfile);
    }
    errno = eno;
    return ret;
}

int
ttmflush(TTM* ttm, TTMFILE* tfile)
{
    int ret = 0;
    if(tfile == NULL) FAIL(ttm,TTM_ETTM);
    ret = fflush(tfile->file);
    return ret;
}

static int
ttmerror(TTM* ttm, TTMFILE* tfile)
{
    int ret = 0;
    if(tfile == NULL) FAIL(ttm,TTM_ETTM);
    ret = ferror(tfile->file);
    clearerr(tfile->file);
    return ret;
}

static int
ttmeof(TTM* ttm, TTMFILE* tfile)
{
    int ret = 0;
    if(tfile == NULL) FAIL(ttm,TTM_ETTM);
    ret = feof(tfile->file);
    return ret;
}
