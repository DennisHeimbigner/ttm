static TTMERR
scan11(TTM* ttm, VString* src, int* ncpp, utf8** cp8p)
{
    TTMERR err = TTM_NOERR;
    int ncp = -1;
    utf8* cp8 = (utf8*)vsindexp(src);
    if((ncp = u8size(cp8)) < 0) {err = TTM_EUTF8; goto done;}
    if(isnul(cp8)) {err = TTM_EEOS; goto done;}
    vsindexskip(src,ncp);
    *ncpp = ncp;
    *cp8p = cp8;
done:
    return err;
}

static TTMERR
statestep(TTM* ttm, int stopp, int* noadvancep, VString* src, int* ncpp, utf8** cp8p)
{
    *stopp = 0;
    if(vsindexavail(src)==0) {return -1;}
    /* get pointer to next codepoint and skip */
    if(!*noadvancep) {ncp = scan1(ttm,src,cp8p); noadvancep = 0;}
}

static TTMERR
statedriver(TTM* ttm, StateState* params, StateMachine statemachine, void* data)
{
    StringState strstate = empty_stringstate;
    TTMERR err;

    do { /* drive the state machine one step for each iteration */
        params->stop = 0;
        if(vsindexavail(params->src)==0) {return -1;}
        /* get pointer to next codepoint and skip */
        if(!params->noadvance) {scan11(ttm,params->src,&strstate); params->noadvance = 0;}
        err = statemachine(ttm,params,(void*)&strstate,data);
	if(err) break;
    } while(!params->stop);
    return err;
}

static TTMERR
parsecall_machine(TTM* ttm, StateState* params, StringState* ss, void* data)
{
    TTMERR err = TTM_NOERR;
    Frame* frame = (Frame*)data;
    enum ArgState state = (enum ArgState)params->state;
    int ncp = ss->ncp;
    utf8* cp8 = ss->cp8;
    utf8* arg = NULL;

    switch (state) {
    case AG_0:
        /* Look for special chars */
        if(isascii(cp8) && strchr(nonprintignore,*cp8)) {
            /* non-printable ignored chars must be ASCII */
            /* ignore char */
        } else if(u8equal(cp8,ttm->semic)) {
            state = AG_SEMI;
        } else if(u8equal(cp8,ttm->closec)) {
            state = AG_RBR;
        } else if(u8equal(cp8,ttm->openc)) {
            (void)collectescaped(ttm,ttm->passive,params->src);
            state = AG_0;
        } else if(u8equal(cp8,ttm->sharpc)) {
            state = AG_ACTIVE;
        } else if(isescape(cp8)) { /* '\c' */
            state = AG_CHR;
        } else { /* ordinary char */
            params->noadvance = 1;
            state = AG_CHR;
        }
        break;
    case AG_CHR:
        vsappendn(params->dst,(const char*)cp8,ncp);
        state = AG_0;
        break;
    case AG_RBR: params->stop = 1; /* fall thru */
    case AG_SEMI: /* capture argument */
        if(frame->argc >= MAXARGS) fail(ttm,TTM_EMANYPARMS);
	arg = (utf8*)vsextract(params->dst);
#ifdef DEBUG
xprintf("parsecall: argv[%d]=|%s|\n",frame->argc,arg);
#endif
        frame->argv[frame->argc++] = arg;
        state = AG_0; /* start on next arg */
        break;
    case AG_ACTIVE: /* Potential nested fcn call */
        if((err=scan11(ttm,params->src,ss))) goto done;
        if(u8equal(cp8,ttm->sharpc)) {
            state = AG_PASSIVE;
        } else if(u8equal(cp8,ttm->openc)) {
            state = AG_FCNA;
        }
        break;
    case AG_PASSIVE: /* Potential nested fcn call */
        if((err=scan11(ttm,params->src,ss))) goto done;
        if(u8equal(cp8,ttm->sharpc)) { /* ### */
            vsappendn(params->dst,(const char*)ttm->sharpc,u8size(ttm->sharpc));                    
            state = AG_PASSIVE; /* pass a sharp and stay in PASSIVE state */
        } else if(u8equal(cp8,ttm->openc)) {
            state = AG_FCNP;
        }
        break;
    case AG_FCNA: /* Have active nested fcn call */
        /* It is a real call; set active index to
           be at the start of the function name */
        vsindexbackup(params->src,ncp);
        exec(ttm,1);
        state = AG_0;
        break;
    case AG_FCNP: /* Have passive nested fcn call */
        exec(ttm,1);
        state = AG_0;
        break;
    // default: abort();
    }
    params->state = (int)state;
    ss->cp8 = cp8;
    ss->ncp = ncp;
done:
    return err;
}
