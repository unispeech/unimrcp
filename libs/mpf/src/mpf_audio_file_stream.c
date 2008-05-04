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

#include "mpf_audio_file_stream.h"
#include "mpf_frame.h"
#include "apt_log.h"

struct mpf_audio_file_stream_t {
	mpf_audio_stream_t base;

	FILE *read_file;
	FILE *write_file;
};


static apt_bool_t mpf_audio_file_destroy(mpf_audio_stream_t *stream)
{
	mpf_audio_file_stream_t *file_stream = (mpf_audio_file_stream_t*)stream;
	if(file_stream->read_file) {
		fclose(file_stream->read_file);
		file_stream->read_file = NULL;
	}
	if(file_stream->write_file) {
		fclose(file_stream->write_file);
		file_stream->write_file = NULL;
	}
	return TRUE;
}

static apt_bool_t mpf_audio_file_reader_open(mpf_audio_stream_t *stream)
{
	return TRUE;
}

static apt_bool_t mpf_audio_file_reader_close(mpf_audio_stream_t *stream)
{
	return TRUE;
}

static apt_bool_t mpf_audio_file_frame_read(mpf_audio_stream_t *stream, mpf_frame_t *frame)
{
	mpf_audio_file_stream_t *file_stream = (mpf_audio_file_stream_t*)stream;
	if(file_stream->read_file) {
		if(fread(frame->codec_frame.buffer,1,frame->codec_frame.size,file_stream->read_file) == frame->codec_frame.size) {
			frame->type = MEDIA_FRAME_TYPE_AUDIO;
		}
	}
	return TRUE;
}


static apt_bool_t mpf_audio_file_writer_open(mpf_audio_stream_t *stream)
{
	return TRUE;
}

static apt_bool_t mpf_audio_file_writer_close(mpf_audio_stream_t *stream)
{
	return TRUE;
}

static apt_bool_t mpf_audio_file_frame_write(mpf_audio_stream_t *stream, const mpf_frame_t *frame)
{
	mpf_audio_file_stream_t *file_stream = (mpf_audio_file_stream_t*)stream;
	if(file_stream->write_file) {
		fwrite(frame->codec_frame.buffer,1,frame->codec_frame.size,file_stream->write_file);
	}
	return TRUE;
}

static const mpf_audio_stream_vtable_t vtable = {
	mpf_audio_file_destroy,
	mpf_audio_file_reader_open,
	mpf_audio_file_reader_close,
	mpf_audio_file_frame_read,
	mpf_audio_file_writer_open,
	mpf_audio_file_writer_close,
	mpf_audio_file_frame_write
};

static void mpf_audio_file_capabilities_init(mpf_audio_stream_t *stream, mpf_stream_mode_e mode, apr_pool_t *pool)
{
	mpf_codec_descriptor_t *descriptor;
	mpf_audio_stream_init(stream,&vtable);
	stream->mode = mode;
	mpf_codec_list_init(&stream->codec_list,1,pool);
	descriptor = mpf_codec_list_add(&stream->codec_list);
	if(descriptor) {
		descriptor->payload_type = 11;
		descriptor->name = "L16";
		descriptor->sampling_rate = 8000;
		descriptor->channel_count = 1;
	}
}

MPF_DECLARE(mpf_audio_stream_t*) mpf_audio_file_reader_create(const char *file_name, apr_pool_t *pool)
{
	mpf_audio_file_stream_t *file_stream = apr_palloc(pool,sizeof(mpf_audio_file_stream_t));
	mpf_audio_file_capabilities_init(&file_stream->base,STREAM_MODE_RECEIVE,pool);

	file_stream->write_file = NULL;
	file_stream->read_file = fopen(file_name,"rb");
	return &file_stream->base;
}

MPF_DECLARE(mpf_audio_stream_t*) mpf_audio_file_writer_create(const char *file_name, apr_pool_t *pool)
{
	mpf_audio_file_stream_t *file_stream = apr_palloc(pool,sizeof(mpf_audio_file_stream_t));
	mpf_audio_file_capabilities_init(&file_stream->base,STREAM_MODE_SEND,pool);

	file_stream->read_file = NULL;
	file_stream->write_file = fopen(file_name,"wb");
	return &file_stream->base;
}
