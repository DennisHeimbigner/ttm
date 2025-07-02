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
    io->fileno = 0;
    ttm->io.allfiles[io->fileno] = io;
    ttm->io._stdin = io; io = NULL;

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
    io->fileno = 1;
    ttm->io.allfiles[io->fileno] = io;
    ttm->io._stdout = io; io = NULL;

    if((io = (TTMFILE*)calloc(sizeof(TTMFILE),1))==NULL) FAIL(ttm,TTM_EMEMORY);
    io->npushed = 0 ;
    io->name = strdup("stderr");
    io->file = stderr;
    io->isstd = 1; /* => do not close the FILE* object */
    io->mode = IOM_WRITE | IOM_APPEND;
    io->fileno = 2;
    ttm->io.allfiles[io->fileno] = io;
    ttm->io._stderr = io; io = NULL;

done:
    ttmclose(ttm,io);
    errno = 0; /* reset */
    return err;
}

static void
closeio1(TTM* ttm, TTMFILE* f)
{
    if(ttm != NULL && f != NULL) {
	ttmclose(ttm,f);
	
    }
}

static void
closeio(TTM* ttm)
{
    if(ttm != NULL) {
	closeio1(ttm,ttm->io._stdin); ttm->io._stdin = NULL;
	closeio1(ttm,ttm->io._stdout); ttm->io._stdout = NULL;
	closeio1(ttm,ttm->io._stderr); ttm->io._stderr = NULL;
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
    tfile->npushed = 0;
    tfile->fileno = ttmgetemptyfileno(ttm);
    ttm->io.allfiles[tfile->fileno] = tfile;

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
	ttm->io.allfiles[tfile->fileno] = NULL;
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

/* Find empty ttm->io.allfiles location */
static size_t
ttmgetemptyfileno(TTM* ttm)
{
    size_t i;
    for(i=0;i<MAXOPENFILES;i++) {
	if(ttm->io.allfiles[i] == NULL) break;
    }
    assert(i < MAXOPENFILES);
    return i;
}

static TTMFILE*
ttmfindfile(TTM* ttm, const char* filename)
{
    TTMFILE* file = NULL;
    size_t i;

    if(filename == NULL || strlen(filename) == 0) goto done;
    for(i=0;i<MAXOPENFILES;i++) {
	TTMFILE* f = ttm->io.allfiles[i];
	if(f != NULL) {
	    if(strcmp(f->name,filename)==0) {
		file = f;
		break;
	    }
	}
    }
done:
    return file;
}

/* Remove file by path; close file if open */
static void
ttmrmfile(TTM* ttm, const char* filename)
{
    TTMFILE* file = NULL;

    if(filename == NULL || strlen(filename) == 0) goto done;
    file = ttmfindfile(ttm,filename);
    if(file != NULL) ttmclose(ttm,file);
    remove(filename);
done:
    return;
}
