/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is MPEG4IP.
 * 
 * The Initial Developer of the Original Code is Cisco Systems Inc.
 * Portions created by Cisco Systems Inc. are
 * Copyright (C) Cisco Systems Inc. 2002.  All Rights Reserved.
 * 
 * Contributor(s): 
 *		Bill May (wmay@cisco.com)
 */
/* mpeg2t_mp3.c - process mpeg audio frames */

#include "mpeg4ip.h"
#include "mpeg2_transport.h"
#include <assert.h>
#include "mp4av.h"
#include "mpeg2t_private.h"
#ifdef HAVE_A52DEC
#include <a52dec/a52.h>

#define AC3_SYNC_WORD 0x0b77
static int MP4AV_Ac3FindFrameStart (const uint8_t *buf, 
				    uint32_t buflen,
				    const uint8_t **ppFrame,
				    uint32_t *bitrate,
				    uint32_t *freq,
				    uint32_t *framesize,
				    uint32_t *chans)
{
  while (buflen >= 6) {
    if (ntohs(*(const uint16_t *)buf) == 0x0b77) {
      // we've most likely identified a frame
      int flags, srate, brate;
      *ppFrame = buf;
      *framesize = a52_syncinfo((uint8_t *)buf, &flags, &srate, &brate);
      if (*framesize != 0) {
	*bitrate = brate;
	*freq = srate;
	if (flags & A52_LFE) {
	  *chans = 6;
	} else {
	  switch (flags & A52_CHANNEL_MASK) {
	  case A52_MONO:
	    *chans = 1;
	    break;
	  case A52_CHANNEL:
	  case A52_STEREO:
	  case A52_DOLBY:
	    *chans = 2;
	    break;
	  case A52_2F2R:
	    *chans = 4;
	    break;
	  default:
	    *chans = 5;
	    break;
	  }
	}

	return 1;
      }
    }
    buf++;
    buflen--;
  }
  return 0;
}
    
/*
 * mpeg2t_find_mp3_frame_start - look through the buffer and find
 * the mpeg audio header
 */
static uint32_t mpeg2t_find_ac3_frame_start (mpeg2t_es_t *es_pid, 
					     const uint8_t *esptr, 
					     uint32_t buflen,
					     int *ret)
{
  const uint8_t *fptr;
  int found = 0;
  int dropped = 0;
  int offset, tocopy;
  uint32_t framesize;
  uint32_t freq;
  uint32_t bitrate;
  uint32_t chans;

  if (es_pid->left != 0) {
    // Indicates that we have up to 3 bytes left from previous frame
    // copy so we have 4 bytes in the buffer, then see if it matches.
    memcpy(es_pid->left_buff + es_pid->left,
	   esptr, 
	   7 - es_pid->left);
    // look at these 7 bytes

    found = MP4AV_Ac3FindFrameStart(es_pid->left_buff, 7, &fptr, 
				    &bitrate, &freq,
				    &framesize,
				    &chans);
    if (found) {
      dropped = fptr - (const uint8_t *)&es_pid->left_buff[0];
#if 0
      mpeg2t_message(LOG_ERR, "ac3 - Found in left - %d dropped %d", es_pid->left, dropped);
#endif
    } else {
      es_pid->left = 0;
    }
  }

  if (found == 0) {
    // Not found with leftover bytes - see if it's in the buffer
    found = MP4AV_Ac3FindFrameStart(esptr, buflen, &fptr, 
				    &bitrate, &freq,
				    &framesize, &chans);
    if (found == 0) {
      memcpy(es_pid->left_buff,
	     esptr + buflen - 5, 
	     5);
      es_pid->left = 5;
      return buflen;
    }
    dropped = fptr - esptr;
#if 0
      mpeg2t_message(LOG_ERR, "ac3 - dropped %d", dropped);
#endif
  }

  if (found) {
    // We've found the header - load up the info if we haven't already
    es_pid->sample_freq = freq;
    es_pid->audio_chans = chans;
    es_pid->sample_per_frame = 256 * 6;
    es_pid->bitrate = bitrate;
    mpeg2t_message(LOG_NOTICE, "ac3 - freq %u spf %u brate %g", 
		   es_pid->sample_freq,
		   es_pid->sample_per_frame,
		   es_pid->bitrate);
    es_pid->info_loaded = 1;
    *ret = 1;
    // We know how big the frame will be, so malloc it
    mpeg2t_malloc_es_work(es_pid, framesize);
    if (es_pid->work == NULL) return buflen;
      
    offset = 0;

    // Copy all the data we can to the buffer
    if (es_pid->left) {
      offset = es_pid->left - dropped;
      memcpy(es_pid->work->frame, es_pid->left_buff + dropped, offset);
      es_pid->work_loaded = offset;
      es_pid->left = 0;
      dropped = 0;
    }

    tocopy = MIN(buflen - dropped, framesize);
    memcpy(es_pid->work->frame + offset, 
	   esptr + dropped,
	   tocopy);
    es_pid->work_loaded += tocopy;
#if 0
    printf("start framesize %d copied %d\n", es_pid->work->frame_len,
	   es_pid->work_loaded);
#endif
    return tocopy + dropped;
  }
  return buflen;
}

int process_mpeg2t_ac3_audio (mpeg2t_es_t *es_pid, 
			      const uint8_t *esptr, 
			      uint32_t buflen)
{
  uint32_t used;
  int ret;
  uint32_t tocopy;

#if 0
  if (es_pid->peshdr_loaded != 0 && (es_pid->stream_id & 0xe0) != 0xc0) {
    mpeg2t_message(LOG_ERR, 
		   "Illegal stream id %x in mpeg audio stream - PID %x",
		   es_pid->stream_id, es_pid->pid.pid);
    return -1;
  }
#endif
  ret = 0;
  while (buflen > 0) {
    if (es_pid->work == NULL) {
      // we haven't found the header - look for it.
      if (buflen < 7) {
	memcpy(es_pid->left_buff, esptr, buflen);
	es_pid->left = buflen;
	return ret;
      }

      used = mpeg2t_find_ac3_frame_start(es_pid, esptr, buflen, &ret);
      esptr += buflen;
      buflen -= used;
    } else {
      // We've got the header - keep going until we've got the frame loaded
      tocopy = MIN(buflen, (es_pid->work->frame_len - es_pid->work_loaded));
      memcpy(es_pid->work->frame + es_pid->work_loaded, esptr, tocopy);
      buflen -= tocopy;
      es_pid->work_loaded += tocopy;
      esptr += tocopy;
#if 0
      printf("added %d bytes to %d\n", tocopy, es_pid->work_loaded);
#endif
    }
    if (es_pid->work != NULL &&
	es_pid->work_loaded == es_pid->work->frame_len) {

      mpeg2t_finished_es_work(es_pid, es_pid->work_loaded);
      ret = 1;
    }
  }
  return ret;
}

int mpeg2t_ac3_audio_info (mpeg2t_es_t *es_pid, char *buffer, size_t len)
{
  int rate;
  if (es_pid->info_loaded == 0) return -1;
  rate = es_pid->bitrate / 1000;
  snprintf(buffer, len, "AC3, %d kbps, %d %d channels", 
	   rate, es_pid->sample_freq, 
	   es_pid->audio_chans);
  return 0;
}
#else
int process_mpeg2t_ac3_audio (mpeg2t_es_t *es_pid, 
			      const uint8_t *esptr, 
			      uint32_t buflen)
{
  return -1;
}

int mpeg2t_ac3_audio_info (mpeg2t_es_t *es_pid, char *buffer, size_t len)
{
  return -1;
}
#endif
	   
/* mpeg2t_ac3.c */
