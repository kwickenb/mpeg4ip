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
 * Copyright (C) Cisco Systems Inc. 2000, 2001.  All Rights Reserved.
 * 
 * Contributor(s): 
 *              Bill May        wmay@cisco.com
 */
/*
 * qtime_bytestream.cpp - convert quicktime file to a bytestream
 */

#include "mpeg4ip.h"
#include "mpeg2t_bytestream.h"
#include "player_util.h"
#include "mp4av.h"
//#define DEBUG_MPEG2T_FRAME 1
//#define DEBUG_MPEG2T_PSTS 1

static inline bool convert_psts (mpeg2t_es_t *es_pid,
				 mpeg2t_stream_t *sptr,
				 mpeg2t_frame_t *fptr)
{
  mpeg2t_client_t *info = sptr->m_parent;
  uint64_t ps_ts;
  // we want to 0 out the timestamps of the incoming transport
  // stream so that the on-demand shows the correct timestamps

  // see if we've set the initial timestamp
  if (info->m_have_start_psts == 0) {
    if (fptr->have_ps_ts == 0) {
#ifdef DEBUG_MPEG2T_PSTS
      player_debug_message("dropping %d - no psts", sptr->m_is_video);
#endif
      return false;
    }
    if (sptr->m_is_video != 0) {
      if (fptr->frame_type != 1) {
#ifdef DEBUG_MPEG2T_PSTS
	player_debug_message("dropping "U64" video ftype %d", 
			     fptr->ps_ts, fptr->frame_type);
#endif
	return false;
      }
      // adjust ps_ts to the dts - subtract off the temporal reference field
      ps_ts = fptr->ps_ts;
      uint16_t temp_ref = MP4AV_Mpeg3PictHdrTempRef(fptr->frame + fptr->pict_header_offset);
      ps_ts -= ((temp_ref + 1) * es_pid->tick_per_frame);
#ifdef DEBUG_MPEG2T_PSTS
      player_debug_message("video - convert temp ref %d "U64" to "U64, 
			   temp_ref, fptr->ps_ts, ps_ts);
#endif
    } else {
      // audio - use the psts
      ps_ts = fptr->ps_ts;
    }
      
    info->m_start_psts = ps_ts;
    info->m_have_start_psts = 1;
  } else {
    // here, we want to check for a gross change in the psts
    if (fptr->have_ps_ts == 0) return true;
    ps_ts = fptr->ps_ts;
    // need to put a limit on how much less before we restart
    if (ps_ts < info->m_start_psts) return false;
  }
  sptr->m_last_psts = ps_ts;
#ifdef DEBUG_MPEG2T_PSTS
  uint64_t initial = fptr->ps_ts;
#endif
  fptr->ps_ts -= info->m_start_psts;
#ifdef DEBUG_MPEG2T_PSTS
  player_debug_message("%s convert psts "U64" to "U64" "U64,
		       sptr->m_is_video ? "video" : "audio", 
		       initial, fptr->ps_ts, info->m_start_psts);
  
#endif
  return true;
}

/**************************************************************************
 * 
 **************************************************************************/
CMpeg2tByteStream::CMpeg2tByteStream (mpeg2t_es_t *es_pid,
				      const char *type,
				      int has_video,
				      int ondemand)
  : COurInByteStream(type)
{
#ifdef OUTPUT_TO_FILE
  char buffer[80];
  strcpy(buffer, type);
  strcat(buffer, ".raw");
  m_output_file = fopen(buffer, "w");
#endif
  m_es_pid = es_pid;
  m_stream_ptr = (mpeg2t_stream_t *)mpeg2t_get_userdata(&m_es_pid->pid); 
  m_has_video = has_video;
  m_timestamp_loaded = 0;
  m_frame = NULL;
  m_ondemand = ondemand;
}

CMpeg2tByteStream::~CMpeg2tByteStream()
{
  mpeg2t_set_frame_status(m_es_pid, MPEG2T_PID_NOTHING); // eliminate any callbacks
#ifdef OUTPUT_TO_FILE
  fclose(m_output_file);
#endif
  mpeg2t_free_frame(m_frame);
}

int CMpeg2tByteStream::eof(void)
{
  return m_stream_ptr->m_have_eof; 
}

int CMpeg2tByteStream::have_no_data (void)
{
  return (m_es_pid->list == NULL);
}

void CMpeg2tByteStream::reset (void) 
{
  mpeg2t_frame_t *p;
  do {
    p = mpeg2t_get_es_list_head(m_es_pid);
    if (p != NULL)
      mpeg2t_free_frame(p);
  } while (p != NULL);
}

void CMpeg2tByteStream::pause (void)
{
  mpeg2t_client_t *info = m_stream_ptr->m_parent;
  info->m_have_start_psts = 0;
  mpeg2t_set_frame_status(m_es_pid, MPEG2T_PID_NOTHING);
  reset();
  m_stream_ptr->m_buffering = 1;
  m_stream_ptr->m_have_eof = 0;
}

void CMpeg2tByteStream::play (uint64_t start_time)
{
  m_stream_ptr->m_have_eof = 0;
  m_play_start_time = start_time;
  mpeg2t_set_frame_status(m_es_pid, MPEG2T_PID_SAVE_FRAME);
}

uint64_t CMpeg2tByteStream::start_next_frame (uint8_t **buffer, 
					      uint32_t *buflen,
					      void **ud)
{
  uint64_t ret;
  mpeg2t_free_frame(m_frame);
  
  // dump frames until we have a valid start psts for both
  // audio and video
  bool done = false;
  while (done == false) {
    m_frame = mpeg2t_get_es_list_head(m_es_pid);
    if (m_frame == NULL) {
      return 0;
    }
    done = convert_psts(m_es_pid, m_stream_ptr, m_frame);
    if (done == false) {
      mpeg2t_free_frame(m_frame);
    }
  }

  if (m_frame != NULL) {
    if (get_timestamp_for_frame(m_frame, ret) >= 0) {
      *buffer = m_frame->frame;
      *buflen = m_frame->frame_len;
      if (m_ondemand) {
	ret += m_play_start_time;
      }
#ifdef DEBUG_MPEG2T_FRAME
      player_debug_message("%s - len %d time "U64" ftype %d %p", 
			   m_name, *buflen, ret, m_frame->frame_type,
	*buffer);
#endif
      return (ret);
    }
  }
  return 0;
}

void CMpeg2tByteStream::used_bytes_for_frame (uint32_t bytes_used)
{
  // nothing here yet...
}

int CMpeg2tByteStream::skip_next_frame (uint64_t *pts, 
					int *pSync,
					uint8_t **buffer, 
					uint32_t *buflen,
					void **userdata)
{
  uint64_t ts;
  ts = start_next_frame(buffer, buflen, NULL);
  *pts = ts;
  *pSync = m_has_video && m_frame && m_frame->frame_type == 1 ? 1 : 0; //m_frame_on_has_sync;
  
  player_debug_message("%s - skip time "U64,
			   m_name, ts);
  if (*buffer == NULL) return 0;
  return (1);
}
// left off here...

double CMpeg2tByteStream::get_max_playtime (void) 
{
  // we shouldn't know - we might need to do something when
  // we're running sdp...
  return (0.0);
};

void CMpeg2tVideoByteStream::reset(void)
{
  m_have_prev_frame_type = 0;
  m_timestamp_loaded = 0;
  CMpeg2tByteStream::reset();
}
int CMpeg2tVideoByteStream::get_timestamp_for_frame (mpeg2t_frame_t *fptr,
						     uint64_t &outts)

{
  uint64_t ts;
  //  m_es_pid->frame_rate = 24;
  double value = 90000.0 / m_es_pid->frame_rate;
  uint64_t frame_time = (uint64_t)value;
#ifdef DEBUG_MPEG2T_PSTS
  player_debug_message("video frame len %d have psts %d ts "U64, 
		       fptr->frame_len, fptr->have_ps_ts, fptr->ps_ts);
#endif
  if (fptr->have_ps_ts == 0) {
    // We don't have a timestamp on this - just increment from
    // the previous timestamp.
    if (m_timestamp_loaded == 0) return -1;
    if (m_es_pid->info_loaded == 0) return -1;

    outts = m_prev_ts + frame_time;
    m_have_prev_frame_type = 1;
    m_prev_frame_type = fptr->frame_type;
    m_prev_ts = outts;
    outts *= TO_U64(1000);
    outts /= TO_U64(90000); // get msec from 90000 timescale
    return 0;
  }
  m_timestamp_loaded = 1;
  ts = fptr->ps_ts;
#if 0
  if (m_ondemand != 0) {
    ts -= m_es_pid->first_ps_ts;
  }
#endif
  if (m_have_prev_frame_type) {
    if (fptr->frame_type == 3) {
      // B frame
      outts = ts + frame_time;
    } else {
      outts = m_prev_ts + frame_time;
    }
  } else {
    if (fptr->frame_type == 1) {
      uint16_t temp_ref = MP4AV_Mpeg3PictHdrTempRef(fptr->frame + fptr->pict_header_offset);
      ts -= ((temp_ref + 1) * m_es_pid->tick_per_frame);
      outts = ts;
    } else {
      player_error_message( "no psts and no prev frame");
      outts = ts;
    }
  }

  m_have_prev_frame_type = 1;
  m_prev_frame_type = fptr->frame_type;
  m_prev_ts = outts;

  outts *= TO_U64(1000);
  outts /= TO_U64(90000); // get msec from 90000 timescale
  
  return 0;
}

void CMpeg2tAudioByteStream::reset(void)
{
  m_timestamp_loaded = 0;
  CMpeg2tByteStream::reset();
}

int CMpeg2tAudioByteStream::get_timestamp_for_frame (mpeg2t_frame_t *fptr,
						     uint64_t &ts)
{
  uint64_t pts_in_msec;
  // all ts for audio are stored in msec, not in timescale
#ifdef DEBUG_MPEG2T_PSTS
  player_debug_message("audio frame len %d have psts %d ts "U64" %d %d", 
		       fptr->frame_len, fptr->have_ps_ts, fptr->ps_ts,
		       m_es_pid->sample_per_frame, 
		       m_es_pid->sample_freq);
#endif
  if (fptr->have_ps_ts != 0) {
    m_timestamp_loaded = 1;
    pts_in_msec = fptr->ps_ts;
    pts_in_msec *= TO_U64(1000);
    pts_in_msec /= TO_U64(90000);
    m_last_timestamp = pts_in_msec;
    m_frames_since_last_timestamp = 0;
    ts = m_last_timestamp;
    return 0;
  }

  if (m_timestamp_loaded == 0) return -1;
  if (m_es_pid->info_loaded == 0) return -1;
#if 0
  // this is commented out because it's not really needed - it looks like
  // some psts are not accurate, so we just use the ones that we've got set
  m_frames_since_last_timestamp++;

  double value = (1000.0 * m_frames_since_last_timestamp * m_es_pid->sample_per_frame) /
    m_es_pid->sample_freq;
  uint64_t val = (uint64_t)value;
  ts = m_last_timestamp + val;
#else
  ts = m_last_timestamp;
#endif
  return 0;
}
  
/* end file mpeg2t_bytestream.cpp */
