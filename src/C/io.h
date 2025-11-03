static TTMERR
setupio(TTM* ttm, const char* infile, const char* outfile, int merge_err_out)
{
    TTMERR err = TTM_NOERR;
    TTMFILE* io = NULL;

    assert(ttm != NULL);

    /* Mirror stdin, stdout, and stderr */
    if(infile == NULL || strlen(infile) == 0) {
        if((err=buildfile(ttm,"stdin",stdin,IOM_READ,&io))) EXIT(ttm,err);
    } else {
        if((err=buildfile(ttm,infile,NULL,IOM_READ,&io))) EXIT(ttm,err);
    }
    io->fileno = 0;
    ttm->io.allfiles[io->fileno] = io;
    ttm->io._stdin = io; io = NULL;

    if(outfile == NULL || strlen(outfile) == 0) {
        if((err=buildfile(ttm,"stdout",stdout,IOM_WRITE,&io))) EXIT(ttm,err);
    } else {
        if((err=buildfile(ttm,outfile,NULL,IOM_WRITE|IOM_APPEND,&io))) EXIT(ttm,err);
    }
    io->fileno = 1;
    ttm->io.allfiles[io->fileno] = io;
    ttm->io._stdout = io; io = NULL;

    if(merge_err_out) {
        if((err=buildfile(ttm,"stderr",stdout,IOM_WRITE,&io))) EXIT(ttm,err);
    } else {
        if((err=buildfile(ttm,"stderr",stderr,IOM_WRITE,&io))) EXIT(ttm,err);
    }
    io->fileno = 2;
    ttm->io.allfiles[io->fileno] = io;
    ttm->io._stderr = io; io = NULL;

done:
    ttmclose(ttm,io);
    errno = 0; /* reset */
    return err;
}

static TTMERR
buildfile(TTM* ttm, const char* fname, FILE* std, int mode, TTMFILE** iop)
{
    int err = TTM_NOERR;
    TTMFILE* io = NULL;
    int isstd = (std == NULL ? 0 : 1);

    if((io = (TTMFILE*)calloc(sizeof(TTMFILE),1))==NULL) EXIT(ttm,TTM_EMEMORY);
    io->npushed = 0 ;
    io->name = strdup(fname);
    io->isstd = isstd;
    io->mode = mode;
    if(isstd)
	io->file = std;
    else {
	const char* modestr = NULL;
	if(io->mode & IOM_READ) modestr = "rb";
	else if((io->mode & IOM_WRITE) && (io->mode & IOM_APPEND)) modestr = "wb+";
	else if(io->mode & IOM_WRITE) modestr = "wb";
	else EXIT(ttm,TTM_EINVAL);
	io->file = fopen(fname,modestr);
    }
    if(io->file == NULL) {
	fprintf(stderr,"File cannot be accessed: %s\n",fname);;
        err = errno;
	free(io);
	goto done;
    }
    if(iop) {*iop = io; io = NULL;}
done:
    closeio1(ttm,io);
    return UPTHROW(ttm,err);
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
    int err = 0;
    TTMFILE* tfile = NULL;
    tfile = (TTMFILE*)calloc(1,sizeof(TTMFILE));
    if(tfile == NULL) EXIT(ttm,TTM_EMEMORY);
    tfile->name = strdup(fname);
    tfile->file = fopen(fname,mode);
    if(tfile->file == NULL) {err = errno; goto done;}
    tfile->npushed = 0;
    tfile->fileno = ttmgetemptyfileno(ttm);
    ttm->io.allfiles[tfile->fileno] = tfile;

done:;
    errno = err;
    return tfile;
}

static int
ttmclose(TTM* ttm, TTMFILE* tfile)
{
    int err = 0;
    if(tfile != NULL) {
	ttm->io.allfiles[tfile->fileno] = NULL;
        if(!tfile->isstd) {
	    err= fclose(tfile->file); tfile->file = NULL;
	    if(err == EOF) err = errno;
	}
        nullfree(tfile->name);
        nullfree(tfile);
    }
    errno = 0;
    return err;
}

int
ttmflush(TTM* ttm, TTMFILE* tfile)
{
    int err = 0;
    if(tfile == NULL) EXIT(ttm,TTM_ETTM);
    err = fflush(tfile->file);
done:
    return err;
}

static int
ttmerror(TTM* ttm, TTMFILE* tfile)
{
    int err = 0;
    if(tfile == NULL) EXIT(ttm,TTM_ETTM);
    err = ferror(tfile->file);
    clearerr(tfile->file);
done:
    return err;
}

static int
ttmeof(TTM* ttm, TTMFILE* tfile)
{
    int err = 0;
    if(tfile == NULL) EXIT(ttm,TTM_ETTM);
    err = feof(tfile->file);
done:
    return err;
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
