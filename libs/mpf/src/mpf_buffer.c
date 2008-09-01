/*
 * Copyright 2008 Arsen Chaloyan
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "mpf_buffer.h"
#include "apt_cyclic_queue.h"
#include "apt_log.h"

struct mpf_buffer_t {
	apt_cyclic_queue_t *cyclic_queue;
	mpf_frame_t        *cur_chunk;
	apr_size_t          remaining_chunk_size;
	apr_thread_mutex_t *guard;
	apr_pool_t         *pool;
};


mpf_buffer_t* mpf_buffer_create(apr_pool_t *pool)
{
	mpf_buffer_t *buffer = apr_palloc(pool,sizeof(mpf_buffer_t));
	buffer->pool = pool;
	buffer->cur_chunk = NULL;
	buffer->remaining_chunk_size = 0;
	buffer->cyclic_queue = apt_cyclic_queue_create(1000,pool);
	apr_thread_mutex_create(&buffer->guard,APR_THREAD_MUTEX_UNNESTED,pool);
	return buffer;
}

void mpf_buffer_destroy(mpf_buffer_t *buffer)
{
	if(buffer->guard) {
		apr_thread_mutex_destroy(buffer->guard);
		buffer->guard = NULL;
	}
}

apt_bool_t mpf_buffer_restart(mpf_buffer_t *buffer)
{
	apr_thread_mutex_lock(buffer->guard);
	apt_cyclic_queue_clear(buffer->cyclic_queue);
	apr_thread_mutex_unlock(buffer->guard);
	return TRUE;
}

static APR_INLINE apt_bool_t mpf_buffer_chunk_write(mpf_buffer_t *buffer, mpf_frame_t *chunk)
{
	if(apt_cyclic_queue_push(buffer->cyclic_queue,chunk) != TRUE) {
		apt_log(APT_PRIO_WARNING,"Failed to Write Chunk [queue is full]");
		return FALSE;
	}

	apt_log(APT_PRIO_INFO,"Write Chunk [%d]", chunk->codec_frame.size);
	return TRUE;
}

apt_bool_t mpf_buffer_audio_write(mpf_buffer_t *buffer, void *data, apr_size_t size)
{
	mpf_frame_t *chunk;
	apt_bool_t status = TRUE;
	apr_thread_mutex_lock(buffer->guard);

	chunk = apr_palloc(buffer->pool,sizeof(mpf_frame_t));
	chunk->codec_frame.buffer = apr_palloc(buffer->pool,size);
	memcpy(chunk->codec_frame.buffer,data,size);
	chunk->codec_frame.size = size;
	chunk->type = MEDIA_FRAME_TYPE_AUDIO;
	status = mpf_buffer_chunk_write(buffer,chunk);
	
	apr_thread_mutex_unlock(buffer->guard);
	return status;
}

apt_bool_t mpf_buffer_event_write(mpf_buffer_t *buffer, mpf_frame_type_e event_type)
{
	mpf_frame_t *chunk;
	apt_bool_t status = TRUE;
	apr_thread_mutex_lock(buffer->guard);

	chunk = apr_palloc(buffer->pool,sizeof(mpf_frame_t));
	chunk->codec_frame.buffer = NULL;
	chunk->codec_frame.size = 0;
	chunk->type = event_type;
	status = mpf_buffer_chunk_write(buffer,chunk);
	
	apr_thread_mutex_unlock(buffer->guard);
	return status;
}

apt_bool_t mpf_buffer_frame_read(mpf_buffer_t *buffer, mpf_frame_t *media_frame)
{
	mpf_codec_frame_t *dest;
	mpf_codec_frame_t *src;
	apr_size_t remaining_frame_size = media_frame->codec_frame.size;
	apr_thread_mutex_lock(buffer->guard);
	apt_log(APT_PRIO_INFO,"Read Frame");
	do {
		if(!buffer->cur_chunk) {
			buffer->cur_chunk = apt_cyclic_queue_pop(buffer->cyclic_queue);
			if(buffer->cur_chunk) {
				buffer->remaining_chunk_size = buffer->cur_chunk->codec_frame.size;
			}
			else {
				apt_log(APT_PRIO_INFO,"Buffer is Empty");
				break;
			}
		}

		dest = &media_frame->codec_frame;
		src = &buffer->cur_chunk->codec_frame;
		if(remaining_frame_size < buffer->remaining_chunk_size) {
			/* copy remaining_frame_size */
			media_frame->type |= buffer->cur_chunk->type;
			memcpy(
				(char*)dest->buffer + dest->size - remaining_frame_size,
				(char*)src->buffer + src->size - buffer->remaining_chunk_size,
				remaining_frame_size);
			buffer->remaining_chunk_size -= remaining_frame_size;
			remaining_frame_size = 0;
		}
		else {
			/* copy remaining_chunk_size and proceed to the next chunk */
			media_frame->type |= buffer->cur_chunk->type;
			memcpy(
				(char*)dest->buffer + dest->size - remaining_frame_size,
				(char*)src->buffer + src->size - buffer->remaining_chunk_size,
				buffer->remaining_chunk_size);
			remaining_frame_size -= buffer->remaining_chunk_size;
			buffer->remaining_chunk_size = 0;
			buffer->cur_chunk = NULL;
		}
	}
	while(remaining_frame_size);

	if(remaining_frame_size) {
		apr_size_t offset = media_frame->codec_frame.size - remaining_frame_size;
		memset((char*)media_frame->codec_frame.buffer + offset, 0, remaining_frame_size);
	}
	apr_thread_mutex_unlock(buffer->guard);
	return TRUE;
}
