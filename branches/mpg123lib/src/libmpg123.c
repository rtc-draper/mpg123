#include "mpg123lib_intern.h"
#include "getbits.h"

static int initialized = 0;

int mpg123_init(void)
{
	if((sizeof(short) != 2) || (sizeof(long) < 4)) return MPG123_BAD_TYPES;

	init_layer2(); /* inits also shared tables with layer1 */
	init_layer3();
#ifndef OPT_MMX_ONLY
	prepare_decode_tables();
#endif
	check_decoders();
	initialized = 1;
	return MPG123_OK;
}

void mpg123_exit(void)
{
	/* nothing yet, but something later perhaps */
	if(initialized) return;
}

/* create a new handle with specified decoder, decoder can be "", "auto" or NULL for auto-detection */
mpg123_handle *mpg123_new(const char* decoder, int *error)
{
	return mpg123_parnew(NULL, decoder, error);
}

/* ...the full routine with optional initial parameters to override defaults. */
mpg123_handle *mpg123_parnew(mpg123_pars *mp, const char* decoder, int *error)
{
	mpg123_handle *fr = NULL;
	int err = MPG123_OK;
	if(initialized) fr = (mpg123_handle*) malloc(sizeof(mpg123_handle));
	else err = MPG123_NOT_INITIALIZED;
	if(fr != NULL)
	{
		frame_init_par(fr, mp);
		debug("cpu opt setting");
		if(frame_cpu_opt(fr, decoder) != 1)
		{
			err = MPG123_BAD_DECODER;
			frame_exit(fr);
			free(fr);
			fr = NULL;
		}
	}
	if(fr != NULL)
	{
		if((frame_outbuffer(fr) != 0) || (frame_buffers(fr) != 0))
		{
			err = MPG123_NO_BUFFERS;
			frame_exit(fr);
			free(fr);
			fr = NULL;
		}
		else
		{
			opt_make_decode_tables(fr);
			fr->decoder_change = 1;
			/* happening on frame change instead:
			init_layer3_stuff(fr);
			init_layer2_stuff(fr); */
		}
	}
	else if(err == MPG123_OK) err = MPG123_OUT_OF_MEM;

	if(error != NULL) *error = err;
	return fr;
}

int mpg123_decoder(mpg123_handle *mh, const char* decoder)
{
	enum optdec dt = dectype(decoder);
	if(mh == NULL) return MPG123_ERR;

	if(dt == nodec)
	{
		mh->err = MPG123_BAD_DECODER;
		return MPG123_ERR;
	}
	if(dt == mh->cpu_opts.type) return MPG123_OK;

	/* Now really change. */
	/* frame_exit(mh);
	frame_init(mh); */
	debug("cpu opt setting");
	if(frame_cpu_opt(mh, decoder) != 1)
	{
		mh->err = MPG123_BAD_DECODER;
		frame_exit(mh);
		return MPG123_ERR;
	}
	/* New buffers for decoder are created in frame_buffers() */
	if((frame_outbuffer(mh) != 0) || (frame_buffers(mh) != 0))
	{
		mh->err = MPG123_NO_BUFFERS;
		frame_exit(mh);
		return MPG123_ERR;
	}
	opt_make_decode_tables(mh);
	mh->decoder_change = 1;
	return MPG123_OK;
}

int mpg123_param(mpg123_handle *mh, int key, long val, double fval)
{
	int r;
	if(mh == NULL) return MPG123_ERR;
	r = mpg123_par(&mh->p, key, val, fval);
	if(r != MPG123_OK){ mh->err = r; r = MPG123_ERR; }
	return r;
}

int mpg123_par(mpg123_pars *mp, int key, long val, double fval)
{
	int ret = MPG123_OK;
	switch(key)
	{
		case MPG123_VERBOSE:
			mp->verbose = val;
		break;
		case MPG123_FLAGS:
#ifndef GAPLESS
			if(val & MPG123_GAPLESS) ret = MPG123_NO_GAPLESS;
			else
#endif
			mp->flags = val;
		break;
		case MPG123_ADD_FLAGS:
			mp->flags |= val;
		break;
		case MPG123_FORCE_RATE: /* should this trigger something? */
			if(val > 96000) ret = MPG123_BAD_RATE;
			else mp->force_rate = val < 0 ? 0 : val; /* >0 means enable, 0 disable */
		break;
		case MPG123_DOWN_SAMPLE:
			if(val < 0 || val > 2) ret = MPG123_BAD_RATE;
			else mp->down_sample = (int)val;
		break;
		case MPG123_RVA:
			if(val < 0 || val > MPG123_RVA_MAX) ret = MPG123_BAD_RVA;
			else mp->rva = (int)val;
		break;
		case MPG123_DOWNSPEED:
			mp->halfspeed = val < 0 ? 0 : val;
		break;
		case MPG123_UPSPEED:
			mp->doublespeed = val < 0 ? 0 : val;
		break;
		case MPG123_START_FRAME:
			mp->start_frame = val > 0 ? val : 0;
		break;
		case MPG123_DECODE_FRAMES:
			mp->frame_number = val;
		break;
		case MPG123_ICY_INTERVAL:
			mp->icy_interval = val > 0 ? val : 0;
		break;
		case MPG123_OUTSCALE:
#ifdef FLOATOUT
			mp->outscale = fval;
#else
			mp->outscale = val;
#endif
		break;
		default:
			ret = MPG123_BAD_PARAM;
	}
	return ret;
}

int mpg123_getparam(mpg123_handle *mh, int key, long *val, double *fval)
{
	int r;
	if(mh == NULL) return MPG123_ERR;
	r = mpg123_getpar(&mh->p, key, val, fval);
	if(r != MPG123_OK){ mh->err = r; r = MPG123_ERR; }
	return r;
}

int mpg123_getpar(mpg123_pars *mp, int key, long *val, double *fval)
{
	int ret = 0;
	switch(key)
	{
		case MPG123_VERBOSE:
			if(val) *val = mp->verbose;
		break;
		case MPG123_FLAGS:
		case MPG123_ADD_FLAGS:
			if(val) *val = mp->flags;
		break;
		case MPG123_FORCE_RATE:
			if(val) *val = mp->force_rate;
		break;
		case MPG123_DOWN_SAMPLE:
			if(val) *val = mp->down_sample;
		break;
		case MPG123_RVA:
			if(val) *val = mp->rva;
		break;
		case MPG123_DOWNSPEED:
			if(val) *val = mp->halfspeed;
		break;
		case MPG123_UPSPEED:
			if(val) *val = mp->doublespeed;
		break;
		case MPG123_START_FRAME:
			if(val) *val = mp->start_frame;
		break;
		case MPG123_DECODE_FRAMES:
			if(val) *val = mp->frame_number;
		break;
		case MPG123_ICY_INTERVAL:
			if(val) *val = (long)mp->icy_interval;
		break;
		case MPG123_OUTSCALE:
#ifdef FLOATOUT
			if(fval) *fval = mp->outscale;
#else
			if(val) *val = mp->outscale;
#endif
		break;
		default:
			ret = MPG123_BAD_PARAM;
	}
	return ret;
}

int mpg123_eq(mpg123_handle *mh, int channel, int band, double val)
{
	if(mh == NULL) return MPG123_ERR;
	if(band < 0 || band > 31){ mh->err = MPG123_BAD_BAND; return MPG123_ERR; }
	switch(channel)
	{
		case MPG123_LEFT|MPG123_RIGHT:
			mh->equalizer[0][band] = mh->equalizer[1][band] = DOUBLE_TO_REAL(val);
		break;
		case MPG123_LEFT:  mh->equalizer[0][band] = DOUBLE_TO_REAL(val); break;
		case MPG123_RIGHT: mh->equalizer[1][band] = DOUBLE_TO_REAL(val); break;
		default:
			mh->err=MPG123_BAD_CHANNEL;
			return MPG123_ERR;
	}
	mh->have_eq_settings = TRUE;
	return MPG123_OK;
}


/* plain file access, no http! */
int mpg123_open(mpg123_handle *mh, char *path)
{
	mpg123_close(mh);
	frame_reset(mh);
	return open_stream(mh, path, -1);
}

int mpg123_open_fd(mpg123_handle *mh, int fd)
{
	mpg123_close(mh);
	frame_reset(mh);
	return open_stream(mh, NULL, fd);
}

int mpg123_open_feed(mpg123_handle *mh)
{
	mpg123_close(mh);
	frame_reset(mh);
	return open_feed(mh);
}

int decode_update(mpg123_handle *mh)
{
	long native_rate = frame_freq(mh);
	debug("updating decoder structure");
#ifdef GAPLESS
	if(mh->p.flags & MPG123_GAPLESS && mh->lay == 3) /* The gapless info must be there after we got the first real frame. */
	{
		frame_gapless_bytify(mh);
		frame_gapless_position(mh);
	}
#endif
	if(mh->af.rate == native_rate) mh->down_sample = 0;
	else if(mh->af.rate == native_rate>>1) mh->down_sample = 1;
	else if(mh->af.rate == native_rate>>2) mh->down_sample = 2;
	else mh->down_sample = 3; /* flexible (fixed) rate */
	switch(mh->down_sample)
	{
		case 0:
		case 1:
		case 2:
			mh->down_sample_sblimit = SBLIMIT>>(mh->down_sample);
			/* With downsampling I get less samples per frame */
			mh->outblock = sizeof(sample_t)*mh->af.channels*(spf(mh)>>mh->down_sample);
		break;
		case 3:
		{
			if(synth_ntom_set_step(mh) != 0) return -1;
			if(frame_freq(mh) > mh->af.rate)
			{
				mh->down_sample_sblimit = SBLIMIT * mh->af.rate;
				mh->down_sample_sblimit /= frame_freq(mh);
			}
			else mh->down_sample_sblimit = SBLIMIT;
			mh->outblock = sizeof(sample_t) * mh->af.channels *
			               ( ( NTOM_MUL-1+spf(mh)
			                   * (((size_t)NTOM_MUL*mh->af.rate)/frame_freq(mh))
			                 )/NTOM_MUL );
		}
		break;
	}

	if(!(mh->p.flags & MPG123_FORCE_MONO))
	{
		if(mh->af.channels == 1) mh->single = SINGLE_MIX;
		else mh->single = SINGLE_STEREO;
	}
	else mh->single = (mh->p.flags & MPG123_FORCE_MONO)-1;
	if(set_synth_functions(mh) != 0) return -1;;
	init_layer3_stuff(mh);
	init_layer2_stuff(mh);
	do_rva(mh);

	return 0;
}

size_t mpg123_safe_buffer()
{
	return sizeof(sample_t)*2*1152*NTOM_MAX;
}

size_t mpg123_outblock(mpg123_handle *mh)
{
	if(mh != NULL) return mh->outblock;
	else return mpg123_safe_buffer();
}

static int get_next_frame(mpg123_handle *mh)
{
	int change = mh->decoder_change;
	do
	{
		int b;
		debug("read frame");
		b = read_frame(mh);
		debug1("read frame returned %i", b);
		if(b == MPG123_NEED_MORE) return MPG123_NEED_MORE; /* need another call with data */
		else if(b <= 0)
		{
			/* More sophisticated error control? */
			if(b==0 || mh->rdat.filepos == mh->rdat.filelen) return MPG123_DONE;
			else return MPG123_ERR;
		}
		/* Now, there should be new data to decode ... and also possibly new stream properties */
		if(mh->header_change > 1)
		{
			debug("big header change");
			change = 1;
		}
	} while(mh->num < mh->p.start_frame);
	/* When we start actually using the CRC, this could move into the loop... */
	if (mh->error_protection) mh->crc = getbits(mh, 16); /* skip crc */
#ifdef GAPLESS
	/* For new format, this happens (again) in decode_update(), for skipped frames it's needed here. */
	if(mh->p.flags & MPG123_GAPLESS && mh->lay == 3) frame_gapless_position(mh);
#endif
	if(mh->p.frame_number >= 0 && mh->num >= (mh->p.start_frame+mh->p.frame_number))
	{
		mh->to_decode = 0;
		return MPG123_DONE;
	}
	if(change)
	{
		int b = frame_output_format(mh); /* Select the new output format based on given constraints. */
		if(b < 0) return MPG123_ERR; /* not nice to fail here... perhaps once should add possibility to repeat this step */
		if(decode_update(mh) < 0) return MPG123_ERR; /* dito... */
		mh->decoder_change = 0;
		if(b == 1) return MPG123_NEW_FORMAT; /* this should persist over start_frame interations */
	}
	return MPG123_OK;
}

/*
	Put _one_ decoded frame into the frame structure's buffer, accessible at the location stored in <audio>, with <bytes> bytes available.
	The buffer contents will be lost on next call to mpg123_decode_frame.
	MPG123_OK -- successfully decoded the frame, you get your output data
	MPg123_DONE -- This is it. End.
	MPG123_ERR -- some error occured...
	MPG123_NEW_FORMAT -- new frame was read, it results in changed output format -> will be decoded on next call
	MPG123_NEED_MORE  -- that should not happen as this function is intended for in-library stream reader but if you force it...
	MPG123_NO_SPACE   -- not enough space in buffer for safe decoding, also should not happen

	num will be updated to the last decoded frame number (may possibly _not_ increase, p.ex. when format changed).
*/
int mpg123_decode_frame(mpg123_handle *mh, long *num, unsigned char **audio, size_t *bytes)
{
	if(mh == NULL) return MPG123_ERR;
	if(mh->buffer.size < mh->outblock) return MPG123_NO_SPACE;
	mh->buffer.fill = 0; /* always start fresh */
	*audio = mh->buffer.data;
	*bytes = 0;
	while(TRUE)
	{
		/* decode if possible */
		if(mh->to_decode)
		{
			*num = mh->num;
			mh->clip += (mh->do_layer)(mh);
#ifdef GAPLESS
			/* That skips unwanted samples and advances byte position. */
			if(mh->p.flags & MPG123_GAPLESS && mh->lay == 3) frame_gapless_buffercheck(mh);
#endif
			mh->to_decode = FALSE;
			*bytes = mh->buffer.fill;
			return MPG123_OK;
		}
		else
		{
			int b = get_next_frame(mh);
			if(b < 0) return b;
		}
	}
	return MPG123_ERR;
}

int mpg123_read(mpg123_handle *mh, unsigned char *out, size_t size, size_t *done)
{
	return mpg123_decode(mh, NULL, 0, out, size, done);
}

/*
	The old picture:
	while(1) {
		len = read(0,buf,16384);
		if(len <= 0)
			break;
		ret = decodeMP3(&mp,buf,len,out,8192,&size);
		while(ret == MP3_OK) {
			write(1,out,size);
			ret = decodeMP3(&mp,NULL,0,out,8192,&size);
		}
	}
*/

int mpg123_decode(mpg123_handle *mh,unsigned char *inmemory, size_t inmemsize, unsigned char *outmemory, size_t outmemsize, size_t *done)
{
	int ret = MPG123_OK;
	*done = 0;
	if(mh == NULL) return MPG123_ERR;
	if(inmemsize > 0)
	if(feed_more(mh, inmemory, inmemsize) == -1) return MPG123_ERR;
	while(ret == MPG123_OK)
	{
		debug1("decode loop, fill %i", mh->buffer.fill);
		/* Decode a frame that has been read before.
		   This only happens when buffer is empty! */
		if(mh->to_decode)
		{
			if(mh->buffer.size - mh->buffer.fill < mh->outblock) return MPG123_NO_SPACE;
			mh->clip += (mh->do_layer)(mh);
			mh->to_decode = FALSE;
			debug2("decoded frame %li, got %li samples in buffer", mh->num, mh->buffer.fill / (samples_to_bytes(mh, 1)));
#ifdef GAPLESS
			/* That skips unwanted samples and advances byte position. */
			if(mh->p.flags & MPG123_GAPLESS && mh->lay == 3) frame_gapless_buffercheck(mh);
#endif
		}
		if(mh->buffer.fill) /* Copy (part of) the decoded data to the caller's buffer. */
		{
			/* get what is needed - or just what is there */
			int a = mh->buffer.fill > (outmemsize - *done) ? outmemsize - *done : mh->buffer.fill;
			debug4("buffer fill: %i; copying %i (%i - %i)", mh->buffer.fill, a, outmemsize, *done);
			memcpy(outmemory, mh->buffer.data, a);
			/* less data in frame buffer, less needed, output pointer increase, more data given... */
			mh->buffer.fill -= a;
			outmemory  += a;
			*done += a;
			/* move rest of frame buffer to beginning */
			if(mh->buffer.fill) memmove(mh->buffer.data, mh->buffer.data + a, mh->buffer.fill);
			if(!(outmemsize > *done)) return ret;
		}
		else /* If we didn't have data, get a new frame. */
		{
			int b = get_next_frame(mh);
			if(b < 0) return b;
		}
	}
	return ret;
}

long mpg123_clip(mpg123_handle *mh)
{
	long ret = 0;
	if(mh != NULL)
	{
		ret = mh->clip;
		mh->clip = 0;
	}
	return ret;
}

/* Later, this should seek a bit further back and ignore some decoding output to get an exact sample. */

long mpg123_seek_frame(mpg123_handle *mh, long pos, long offset)
{
	long realoff;
	if(mh == NULL) return MPG123_ERR;
	if(pos < 0) pos = mh->num;
	realoff = offset != 0 ? offset : pos - mh->num;
	mh->to_decode = FALSE;
	if(mh->rd->back_frame(mh, -realoff) == MPG123_OK)
	{
		/* Think hard about decoder delay 'n stuff */
		frame_buffers_reset(mh);
#ifdef GAPLESS
		if(mh->p.flags & MPG123_GAPLESS && mh->lay == 3) frame_gapless_position(mh);
#endif
		return mh->num;
	}
	else
	{
		mh->err = MPG123_ERR_READER;
		return MPG123_ERR;
	}
}

int mpg123_meta_check(mpg123_handle *mh)
{
	if(mh != NULL) return mh->metaflags;
	else return 0;
}

int mpg123_id3(mpg123_handle *mh, mpg123_id3v1 **v1, mpg123_id3v2 **v2)
{
	*v1 = NULL;
	*v2 = NULL;
	if(mh == NULL) return MPG123_ERR;

	if(mh->metaflags & MPG123_ID3)
	{
		if(mh->rdat.flags & READER_ID3TAG) *v1 = (mpg123_id3v1*) mh->id3buf;
		*v2 = &mh->id3v2;
		mh->metaflags |= MPG123_ID3;
		mh->metaflags &= ~MPG123_NEW_ID3;
	}
	return MPG123_OK;
}

int mpg123_icy(mpg123_handle *mh, char **icy_meta)
{
	*icy_meta = NULL;
	if(mh == NULL) return MPG123_ERR;

	if(mh->metaflags & MPG123_ICY)
	{
		*icy_meta = mh->icy.data;
		mh->metaflags |= MPG123_ICY;
		mh->metaflags &= ~MPG123_NEW_ICY;
	}
	return MPG123_OK;
}

int mpg123_close(mpg123_handle *mh)
{
	if(mh == NULL) return MPG123_ERR;
	if(mh->rd != NULL && mh->rd->close != NULL) mh->rd->close(mh);
	mh->rd = NULL;
	return MPG123_OK;
}

void mpg123_delete(mpg123_handle *mh)
{
	if(mh != NULL)
	{
		mpg123_close(mh);
		frame_exit(mh); /* free buffers in frame */
		free(mh); /* free struct; cast? */
	}
}

static const char *mpg123_error[] =
{
	"No error... (code 0)",
	"Unable to set up output format! (code 1)",
	"Invalid channel number specified. (code 2)",
	"Invalid sample rate specified. (code 3)",
	"Unable to allocate memory for 16 to 8 converter table! (code 4)",
	"Bad parameter id! (code 5)",
	"Bad buffer given -- invalid pointer or too small size. (code 6)",
	"Out of memory -- some malloc() failed, (code 7)",
	"You didn't initialize the library! (code 8)",
	"Invalid decoder choice. (code 9)",
	"Invalid mpg123 handle. (code 10)",
	"Unable to initialize frame buffers (out of memory?)! (code 11)",
	"Invalid RVA mode. (code 12)",
	"This build doesn't support gapless decoding. (code 13)"
	"Not enough buffer space. (code 14)",
	"Incompatible numeric data types. (code 15)",
	"Bad equalizer band. (code 16)",
	"Null pointer given where valid storage address needed. (code 17)",
	"Some problem reading the stream. (code 18)"
};

const char* mpg123_plain_strerror(int errcode)
{
	if(errcode >= 0 && errcode < sizeof(mpg123_error)/sizeof(char*))
	return mpg123_error[errcode];
	else return "I have no idea - an unknown error code!";
}

int mpg123_errcode(mpg123_handle *mh)
{
	if(mh != NULL) return mh->err;
	return MPG123_BAD_HANDLE;
}

const char* mpg123_strerror(mpg123_handle *mh)
{
	return mpg123_plain_strerror(mpg123_errcode(mh));
}
