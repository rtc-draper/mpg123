/* headers will change */
#include "mpg123lib.h"
#include "frame.h" /* mpg123_handle needs to be filled with life */
#include "decode.h"

static int initialized = 0;

void mpg123_init(void)
{
	init_layer2(); /* inits also shared tables with layer1 */
	init_layer3();
#ifndef OPT_MMX_ONLY
	prepare_decode_tables();
#endif
	initialized = 1;
}

void mpg123_exit(void)
{
	/* nothing yet, but something later perhaps */
}

mpg123_handle *mpg123_new()
{
	mpg123_handle *fr;
	if(initialized) fr = (mpg123_handle*) malloc(sizeof(mpg123_handle));
	else error("You didn't initialize the library!");

	if(fr != NULL)
	{
		frame_init(fr);
		if((frame_outbuffer(fr) != 0) || (frame_buffers(fr) != 0))
		{
			error("Unable to initialize frame buffers!");
			frame_exit(fr);
			fr = NULL;
		}
		else
		{
			init_layer3_stuff(fr);
			init_layer2_stuff(fr);
		}
	}
	else error("Unable to create a handle!");

	return fr;
}

/* Intended usage:
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
	
	So this function reads from buf up to len bytes and decodes what it can, up to a limit, stores decoded bytes.
	I need a buffer fake reader for that. Then get read_frame from common.c straight.
	Also, I need to set up fixed otuput mode 16bit Stereo... hm... forced? What did old mpglib do?
	For the full decoding pleasure we need interface to mpg123's param struct and integration of file readers.
	Most often, you'll have a path/URL or a file descriptor to share.
*/

int mpg123_decode(mpg123_handle *mh,char *inmemory,int inmemsize, char *outmemory,int outmemsize,int *done)
{
	
}

void mpg123_delete(mpg123_handle *mh)
{
	if(mh != NULL)
	{
		frame_clear(mh); /* free buffers in frame */
		free(mh); /* free struct; cast? */
	}
}
