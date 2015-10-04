/*
The MIT License (MIT)

Copyright (c) 2015 Terence Parr, Hanzhou Shi, Shuai Yuan, Yuanyuan Zhang

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gc_ms.h"

#define DEBUG 0
#define MAX_ROOTS       100
#define MAX_OBJECTS     200

static Object **_roots[MAX_ROOTS];
static int num_roots;
static int heap_size;
static byte *start_of_heap;
static byte *end_of_heap;
Free_Header *freelist;

static Object *objects[MAX_OBJECTS];
static int num_objects;
static int num_live_objects;

static void gc_mark_live();
static void gc_mark_object(Object *p);
static void gc_sweep();
static bool gc_in_heap(Object *p);
static void *gc_alloc_space(int size);

ClassDescriptor String_metaclass = {
		"String",
		sizeof (struct String),
		0, /* fields */
		NULL
};

void gc_init(int size) {
	heap_size = size;
	start_of_heap = malloc(size);
	end_of_heap = start_of_heap + size -1;
	num_live_objects = 0;
	num_roots = 0;
	num_objects =0;
	freelist = (Free_Header *)start_of_heap;
	freelist->size = size;
	freelist->next = NULL;
}

void gc_done() {
	free(start_of_heap);
}

void gc_add_addr_of_root(Object **p)
{
	_roots[num_roots++] = p;
}

void gc_add_objects(Object *p) {
	objects[num_objects++] = p;
}
void gc_ms() {
	if(DEBUG) printf("gc_mark_sweep\n");
	gc_mark_live();

	gc_sweep();
}

static void gc_mark_live() {
	int i;
    num_live_objects = 0;
	for (i = 0; i < num_roots; i++) {
		if (DEBUG) printf("root[%d]=%p\n", i, _roots[i]);
		Object *p = *_roots[i];
		if (p != NULL) {
			if (gc_in_heap(p)) {
				gc_mark_object(p);
			}
		}
	}
}

static void gc_mark_object(Object *p) {
	if (!p->header.marked) {
		if (DEBUG) printf("mark %s@%p\n", p->header.metaclass->name, p);
		p->header.marked = 1;
        num_live_objects++;
		int i;
		for (i = 0; i < p->header.metaclass->num_fields; i++) {
			int offset_of_ptr_field = p->header.metaclass->field_offsets[i];
			byte *ptr_to_ptr_filed = ((byte *)p) + offset_of_ptr_field;
			Object **ptr_to_obj_ptr_field = (Object **) ptr_to_ptr_filed;
			Object *target_obj = *ptr_to_obj_ptr_field;
			if (target_obj != NULL) {
				gc_mark_object(target_obj);
			}
		}
	}
}

static void gc_sweep(){
	Object *p ;
	int i;
	for (i = 0; i < num_objects; i++) {
		p = objects[i];
		if (p->header.marked) {
			p->header.marked = 0;

		}
		else {
			if( p != NULL) {
				Free_Header *q  = (Free_Header*)p;
				q->size = p->header.metaclass->size;
				q->next = freelist;
				freelist = q;
			}
		}
	}
}

Object *gc_alloc(ClassDescriptor *class) {
	Object *p = gc_alloc_space(class->size);
	memset(p, 0, class->size);
	p->header.metaclass = class;
	gc_add_objects(p);
	return p;
}

String *gc_alloc_string(int size) {
	String *s;
	s = (String *) gc_alloc_space(sizeof (String) + size + 1);
	s->header.metaclass = &String_metaclass;
	s->header.marked = 0;
	memset(s->str, 0, size);
	s->length = size;
	gc_add_objects(s);
	return s;
}

int gc_num_roots() {
	return num_roots;
}

int gc_num_live_object() {
	return num_live_objects;
}

int gc_num_object() {
	return num_objects;
}

void gc_set_num_roots(int roots)
{
	num_roots = roots;
}

static void *gc_alloc_space(int size) {
	Free_Header *p = freelist;
	Free_Header *prev = NULL;
	while (p != NULL && size != p->size && p->size < size + sizeof(Free_Header)) {
		prev = p;
		p = p->next;
	}
	if (p == NULL) return p;

	Free_Header *nextchunk;
	if (p->size == size) {
		nextchunk = p->next;
	}
	else {
		Free_Header *q = (Free_Header *) (((char *) p) + size);
		q->size = p->size - size;
		q->next = p->next;
		nextchunk = q;
	}
	p->size = size;
	if (p == freelist) {
		freelist = nextchunk;
	}
	else {
		prev->next = nextchunk;
	}

	return p;
}

static bool gc_in_heap(Object *p) {
	return p >= (Object *) start_of_heap && p <= (Object *) end_of_heap;
}

void *get_next_free_addr(){
	return freelist;
}