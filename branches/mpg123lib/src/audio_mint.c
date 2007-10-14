/*
	audio_mint: audio output for MINT

	copyright ?-2006 by the mpg123 project - free software under the terms of the LGPL 2.1
	see COPYING and AUTHORS files in distribution or http://mpg123.org
	initially written by Petr Stehlik
*/

/* derived from LINUX, VOXWARE and SUN for MiNT Audio Device by Petr Stehlik */
#include <fcntl.h>
#include "mpg123app.h"
#include <ioctl.h>
#include <audios.h>

extern int outburst;

int real_rate_printed = 0;



static int audio_rate_best_match(struct audio_info_struct *ai)
{
  int ret,dsp_rate;

  if(!ai || ai->fn < 0 || ai->rate < 0)
    return -1;
  dsp_rate = ai->rate;
  ret = ioctl(ai->fn,AIOCSSPEED, (void *)dsp_rate);
  ret = ioctl(ai->fn,AIOCGSPEED,&dsp_rate);
  if(ret < 0)
    return ret;
  ai->rate = dsp_rate;
  return 0;
}

static int audio_set_rate(struct audio_info_struct *ai)
{
  int dsp_rate = ai->rate;

  if(ai->rate >= 0) {
    int ret, real_rate;
    ret = ioctl(ai->fn, AIOCSSPEED, (void *)dsp_rate);
    if (ret >= 0 && !real_rate_printed) {
      ioctl(ai->fn,AIOCGSPEED,&real_rate);
      if (real_rate != dsp_rate) {
        fprintf(stderr, "Replay rate: %d Hz\n", real_rate);
        real_rate_printed = 1;
      }
    }
    return ret;
  }

  return 0;
}

static int audio_set_channels(struct audio_info_struct *ai)
{
  int chan = ai->channels;

  if(ai->channels < 1)
    return 0;

  return ioctl(ai->fn, AIOCSCHAN, (void *)chan);
}

static int audio_set_format(struct audio_info_struct *ai)
{
  int fmts;

  if(ai->format == -1)
    return 0;

  switch(ai->format) {
    case MPG123_ENC_SIGNED_16:
    default:
      fmts = AFMT_S16;
      break;
    case MPG123_ENC_UNSIGNED_8:
      fmts = AFMT_U8;
      break;
    case MPG123_ENC_SIGNED_8:
      fmts = AFMT_S8;
      break;
    case MPG123_ENC_ULAW_8:
      fmts = AFMT_ULAW;
      break;
  }
  return ioctl(ai->fn, AIOCSFMT, (void *)fmts);
}

static int audio_reset_parameters(struct audio_info_struct *ai)
{
  int ret;
  ret = ioctl(ai->fn,AIOCRESET,NULL);
  if(ret >= 0)
    ret = audio_set_format(ai);
  if(ret >= 0)
    ret = audio_set_channels(ai);
  if(ret >= 0)
    ret = audio_set_rate(ai);
  return ret;
}



int audio_open(struct audio_info_struct *ai)
{
  if(!ai)
    return -1;

  if(!ai->device)
    ai->device = "/dev/audio";

  ai->fn = open(ai->device,O_WRONLY);  

  if(ai->fn < 0)
  {
    error1("Can't open %s!",ai->device);
    return -1;
  }
  ioctl(ai->fn, AIOCGBLKSIZE, &outburst);
  if(outburst > MAXOUTBURST)
    outburst = MAXOUTBURST;
  if(audio_reset_parameters(ai) < 0) {
    close(ai->fn);
    return -1;
  }
  return ai->fn;
}
int audio_get_formats(struct audio_info_struct *ai)
{
  int ret = 0;
  int fmts;

  if(ioctl(ai->fn,AIOCGFMTS,&fmts) < 0)
    return -1;

  if(fmts & AFMT_ULAW)
    ret |= MPG123_ENC_ULAW_8;
  if(fmts & AFMT_S16)
    ret |= MPG123_ENC_SIGNED_16;
  if(fmts & AFMT_U8)
    ret |= MPG123_ENC_UNSIGNED_8;
  if(fmts & AFMT_S8)
    ret |= MPG123_ENC_SIGNED_8;

  return ret;
}

int audio_play_samples(struct audio_info_struct *ai,unsigned char *buf,int len)
{
  return write(ai->fn,buf,len);
}

int audio_close(struct audio_info_struct *ai)
{
  close (ai->fn);
  return 0;
}

void audio_queueflush(struct audio_info_struct *ai)
{
}

