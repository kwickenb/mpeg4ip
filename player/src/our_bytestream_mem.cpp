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

#include "our_bytestream_mem.h"
#include "player_util.h"
#include <SDL.h>

void COurInByteStreamMem::init (const unsigned char *membuf, 
				uint32_t len)
{
  m_frames = 0;
  m_memptr = membuf;
  m_len = len;
  m_offset = 0;
  m_total = 0;
}

COurInByteStreamMem::~COurInByteStreamMem(void)
{
  player_debug_message("bytes %u, frames %llu, fps %llu",
		       m_total, m_frames, m_frame_per_sec);
  if (m_frames > 0)
    player_debug_message("bits per sec %llu", 
			 (m_total * 8 * m_frame_per_sec) / m_frames);
}

int COurInByteStreamMem::eof (void)
{
  return (m_offset >= m_len);
}

uint64_t COurInByteStreamMem::start_next_frame (unsigned char **buffer, 
						uint32_t *buflen) 
{
  uint64_t ret;
  ret = (m_frames * 1000) / m_frame_per_sec;
  m_frames++;
  ret += m_play_start_time;
  *buffer = (unsigned char *)(m_memptr + m_offset);
  *buflen = m_len - m_offset;
  return (ret);
}

void COurInByteStreamMem::used_bytes_for_frame (uint32_t bytes)
{
  m_offset += bytes;
}

void COurInByteStreamMem::reset (void)
{
  m_total = 0;
  m_offset = 0;
  m_frames = 0;
}

const char *COurInByteStreamMem::get_throw_error (int error)
{
  if (error == THROW_MEM_PAST_END) {
    return "Memory past end";
  }
  player_debug_message("Unknown throw error %d", error);
  return "Unknown Error";
}

int COurInByteStreamMem::throw_error_minor (int error)
{
  return 0;
}

COurInByteStreamWav::COurInByteStreamWav (const unsigned char *membuf, 
					  uint32_t len) :
  COurInByteStreamMem(membuf, len) 
{
}
COurInByteStreamWav::~COurInByteStreamWav(void)
{
  SDL_FreeWAV((Uint8 *)m_memptr); 
};

