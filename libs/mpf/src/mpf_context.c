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

#ifdef WIN32
#pragma warning(disable: 4127)
#endif
#include <apr_ring.h> 
#include "mpf_context.h"
#include "mpf_object.h"
#include "mpf_termination.h"
#include "mpf_stream.h"
#include "mpf_encoder.h"
#include "mpf_decoder.h"
#include "mpf_bridge.h"
#include "apt_log.h"

/** Definition of table item used in context */
typedef void* table_item_t;

/** Media processing context */
struct mpf_context_t {
	/** Ring entry */
	APR_RING_ENTRY(mpf_context_t) link;
	/** Back pointer to context factory */
	mpf_context_factory_t        *factory;
	/** Pool to allocate memory from */
	apr_pool_t                   *pool;
	/** External object */
	void                         *obj;

	/** Max number of terminations */
	apr_size_t                    max_termination_count;
	/** Current number of terminations */
	apr_size_t                    termination_count;
	/** Table, which holds terminations and topology */
	table_item_t                **table;
};

/** Factory of media contexts */
struct mpf_context_factory_t {
	/** Ring head */
	APR_RING_HEAD(mpf_context_head_t, mpf_context_t) head;
};


static mpf_object_t* mpf_context_connection_create(mpf_context_t *context, mpf_termination_t *src_termination, mpf_termination_t *sink_termination);

MPF_DECLARE(mpf_context_factory_t*) mpf_context_factory_create(apr_pool_t *pool)
{
	mpf_context_factory_t *factory = apr_palloc(pool, sizeof(mpf_context_factory_t));
	APR_RING_INIT(&factory->head, mpf_context_t, link);
	return factory;
}

MPF_DECLARE(void) mpf_context_factory_destroy(mpf_context_factory_t *factory)
{
	mpf_context_t *context;
	while(!APR_RING_EMPTY(&factory->head, mpf_context_t, link)) {
		context = APR_RING_FIRST(&factory->head);
		mpf_context_destroy(context);
		APR_RING_REMOVE(context, link);
	}
}

MPF_DECLARE(apt_bool_t) mpf_context_factory_process(mpf_context_factory_t *factory)
{
	mpf_context_t *context;
	for(context = APR_RING_FIRST(&factory->head);
			context != APR_RING_SENTINEL(&factory->head, mpf_context_t, link);
				context = APR_RING_NEXT(context, link)) {
		
		mpf_context_process(context);
	}

	return TRUE;
}

 
MPF_DECLARE(mpf_context_t*) mpf_context_create(
								mpf_context_factory_t *factory, 
								void *obj, 
								apr_size_t max_termination_count, 
								apr_pool_t *pool)
{
	apr_size_t i,j;
	mpf_context_t *context = apr_palloc(pool,sizeof(mpf_context_t));
	context->factory = factory;
	context->obj = obj;
	context->pool = pool;
	context->max_termination_count = max_termination_count;
	context->termination_count = 0;
	context->table = apr_palloc(pool,sizeof(table_item_t)*max_termination_count);
	for(i=0; i<max_termination_count; i++) {
		context->table[i] = apr_palloc(pool,sizeof(table_item_t)*max_termination_count);
		for(j=0; j<max_termination_count; j++) {
			context->table[i][j] = NULL;
		}
	}

	return context;
}

MPF_DECLARE(apt_bool_t) mpf_context_destroy(mpf_context_t *context)
{
	apr_size_t i;
	apr_size_t count = context->max_termination_count;
	mpf_termination_t *termination;
	for(i=0; i<count; i++){
		termination = context->table[i][i];
		if(termination) {
			mpf_context_termination_subtract(context,termination);
			if(termination->audio_stream) {
				mpf_audio_stream_destroy(termination->audio_stream);
			}
		}
	}
	return TRUE;
}

MPF_DECLARE(void*) mpf_context_object_get(mpf_context_t *context)
{
	return context->obj;
}

MPF_DECLARE(apt_bool_t) mpf_context_termination_add(mpf_context_t *context, mpf_termination_t *termination)
{
	apr_size_t i;
	apr_size_t count = context->max_termination_count;
	for(i=0; i<count; i++) {
		if(!context->table[i][i]) {
			if(!context->termination_count) {
				apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Add Context");
				APR_RING_INSERT_TAIL(&context->factory->head,context,mpf_context_t,link);
			}

			apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Add Termination");
			context->table[i][i] = termination;
			termination->slot = i;
			context->termination_count++;
			return TRUE;
		}
	}
	return FALSE;
}

MPF_DECLARE(apt_bool_t) mpf_context_termination_subtract(mpf_context_t *context, mpf_termination_t *termination)
{
	apr_size_t i = termination->slot;
	if(i >= context->max_termination_count) {
		return FALSE;
	}
	if(context->table[i][i] != termination) {
		return FALSE;
	}

	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Subtract Termination");
	context->table[i][i] = NULL;
	termination->slot = (apr_size_t)-1;
	context->termination_count--;
	if(!context->termination_count) {
		apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Remove Context");
		APR_RING_REMOVE(context,link);
	}
	return TRUE;
}

MPF_DECLARE(apt_bool_t) mpf_context_process(mpf_context_t *context)
{
	mpf_object_t *object;
	apr_size_t i,j;
	for(i=0; i<context->max_termination_count; i++) {
		for(j=0; j<context->max_termination_count; j++) {
			if(i==j) continue;

			object = context->table[i][j];
			if(object && object->process) {
				object->process(object);
			}
		}
	}
	return TRUE;
}

MPF_DECLARE(apt_bool_t) mpf_context_topology_apply(mpf_context_t *context, mpf_termination_t *termination)
{
	apr_size_t i,j;
	mpf_object_t *object;
	mpf_termination_t *sink_termination;
	mpf_termination_t *source_termination;
	if(context->termination_count <= 1) {
		/* at least 2 terminations are required to apply topology on them */
		return TRUE;
	}

	i = termination->slot;
	for(j=0; j<context->max_termination_count; j++) {
		if(i == j) continue;

		sink_termination = context->table[j][j];
		object = mpf_context_connection_create(context,termination,sink_termination);
		if(object) {
			context->table[i][j] = object;
		}
	}

	j = termination->slot;
	for(i=0; i<context->max_termination_count; i++) {
		if(i == j) continue;

		source_termination = context->table[i][i];
		object = mpf_context_connection_create(context,source_termination,termination);
		if(object) {
			context->table[i][j] = object;
		}
	}

	return TRUE;
}

MPF_DECLARE(apt_bool_t) mpf_context_topology_destroy(mpf_context_t *context, mpf_termination_t *termination)
{
	apr_size_t i,j;
	mpf_object_t *object;
	if(context->termination_count <= 1) {
		/* at least 2 terminations are required to destroy topology */
		return TRUE;
	}

	i = termination->slot;
	for(j=0; j<context->max_termination_count; j++) {
		if(i == j) continue;

		object = context->table[i][j];
		if(object) {
			if(object->destroy) {
				object->destroy(object);
			}
			context->table[i][j] = NULL;
		}
	}

	j = termination->slot;
	for(i=0; i<context->max_termination_count; i++) {
		if(i == j) continue;

		object = context->table[i][j];
		if(object) {
			if(object->destroy) {
				object->destroy(object);
			}
			context->table[i][j] = NULL;
		}
	}
	return TRUE;
}

static mpf_object_t* mpf_context_connection_create(mpf_context_t *context, mpf_termination_t *src_termination, mpf_termination_t *sink_termination)
{
	mpf_object_t *object = NULL;
	mpf_audio_stream_t *source;
	mpf_audio_stream_t *sink;
	if(!src_termination || !sink_termination) {
		return NULL;
	}
	source = src_termination->audio_stream;
	sink = sink_termination->audio_stream;
	if(source && (source->mode & STREAM_MODE_RECEIVE) == STREAM_MODE_RECEIVE &&
		sink && (sink->mode & STREAM_MODE_SEND) == STREAM_MODE_SEND) {
		mpf_codec_t *rx_codec = source->rx_codec;
		mpf_codec_t *tx_codec = sink->tx_codec;
		if(rx_codec && tx_codec) {
			if(mpf_codec_descriptors_match(rx_codec->descriptor,tx_codec->descriptor) == TRUE) {
				object = mpf_null_bridge_create(source,sink,context->pool);
			}
			else {
				if(rx_codec->descriptor->sampling_rate != tx_codec->descriptor->sampling_rate) {
					apt_log(APT_LOG_MARK,APT_PRIO_WARNING,
						"Resampling is not supported now. "
						"Try to configure and use the same sampling rate on both ends");
					return NULL;
				}
				if(rx_codec->vtable && rx_codec->vtable->decode) {
					/* set decoder before bridge */
					mpf_audio_stream_t *decoder = mpf_decoder_create(source,context->pool);
					source = decoder;
				}
				if(tx_codec->vtable && tx_codec->vtable->encode) {
					/* set encoder after bridge */
					mpf_audio_stream_t *encoder = mpf_encoder_create(sink,context->pool);
					sink = encoder;
				}
				object = mpf_bridge_create(source,sink,context->pool);
			}
		}
	}
	return object;
}
