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

#include "apt_cyclic_queue.h"

struct apt_cyclic_queue_t {
	void              **data;
	apr_size_t          max_size;
	apr_size_t          actual_size;
	apr_size_t          head;
	apr_size_t          tail;
	
	apr_pool_t         *pool;
};


APT_DECLARE(apt_cyclic_queue_t*) apt_cyclic_queue_create(apr_size_t size, apr_pool_t *pool)
{
	apt_cyclic_queue_t *queue = apr_palloc(pool, sizeof(apt_cyclic_queue_t));
	queue->pool = pool;
	queue->max_size = size;
	queue->actual_size = 0;
	queue->data = apr_palloc(pool,sizeof(void*) * queue->max_size);
	queue->head = queue->tail = 0;
	queue->pool = pool;
	return queue;
}

APT_DECLARE(void) apt_cyclic_queue_destroy(apt_cyclic_queue_t *queue)
{
	/* nothing to do, the queue is allocated from the pool */
}

APT_DECLARE(apt_bool_t) apt_cyclic_queue_push(apt_cyclic_queue_t *queue, void *obj)
{
	if(queue->actual_size < queue->max_size) {
		queue->data[queue->head] = obj;
		queue->head = (queue->head + 1) % queue->max_size;
		queue->actual_size++;
		return TRUE;
	}
	return FALSE;
}

APT_DECLARE(void*) apt_cyclic_queue_pop(apt_cyclic_queue_t *queue)
{
	void *obj = NULL;
	if(queue->actual_size) {
		obj = queue->data[queue->tail];
		queue->tail = (queue->tail + 1) % queue->max_size;
		queue->actual_size--;
	}
	return obj;
}

APT_DECLARE(void) apt_cyclic_queue_clear(apt_cyclic_queue_t *queue)
{
	queue->actual_size = 0;
	queue->head = queue->tail = 0;
}

APT_DECLARE(apt_bool_t) apt_cyclic_queue_is_empty(apt_cyclic_queue_t *queue)
{
	return queue->actual_size ? TRUE : FALSE;
}
