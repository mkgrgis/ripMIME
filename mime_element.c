
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mime_element.h"
#include "logger.h"

#ifndef FL
#define FL __FILE__, __LINE__
#endif

/* Dynamic array support*/
#define INITIAL_SIZE 8

// function prototypes
//  array container functions
void arrayInit(dynamic_array** arr_ptr);

// Basic Operation functions
void insertItem(dynamic_array* container, MIME_element* item);
void updateItem(dynamic_array* container, int i, MIME_element* item);
MIME_element* getItem(dynamic_array* container, int i);
void deleteItem(dynamic_array* container, int i);

void all_MIME_elements_init (void)
{
	all_MIME_elements.mime_count = 0;
	arrayInit(&(all_MIME_elements.mime_arr));
}

all_MIME_elements_s all_MIME_elements;

static char * dup_ini (char* s)
{
	return (s != NULL) ? strdup(s) : "\0";
}

MIME_element* MIME_element_add (void* parent, RIPMIME_output *unpack_metadata, char* filename, char* content_type_string, char* content_transfer_encoding, char* name, int current_recursion_level, int attachment_count, int filecount)
{
	MIME_element *cur = malloc(sizeof(MIME_element));
	int fullpath_len = 0;

	// LOGGER_log("%s:%d:%s:start\n",FL,__func__);

	fullpath_len = strlen(unpack_metadata->dir) + strlen(filename) + 3 * sizeof(char);
	insertItem(all_MIME_elements.mime_arr, cur);
	cur->parent = parent;
	cur->id = all_MIME_elements.mime_count++;
	cur->filename = dup_ini(filename);
	cur->content_type_string = dup_ini(content_type_string);
	cur->content_transfer_encoding = dup_ini(content_transfer_encoding);
	cur->name = dup_ini(name);

	cur->fullpath = (char*)malloc(fullpath_len);
	snprintf(cur->fullpath,fullpath_len,"%s/%s",unpack_metadata->dir,filename);
	cur->f = fopen(cur->fullpath,"wb");
	if (cur->f == NULL) {
		LOGGER_log("%s:%d:%s:ERROR: cannot open %s for writing",FL,__func__,cur->fullpath);
		return cur;
	}

	// LOGGER_log("%s:%d:%s:DEBUG: Decoding [encoding=%d] to %s\n",FL,__func__, content_transfer_encoding, cur->fullpath);

	if (unpack_metadata != NULL && unpack_metadata->unpack_mode == RIPMIME_UNPACK_MODE_LIST_FILES && cur->f != NULL) {
		fprintf (stdout, "%d|%d|%d|%d|%s|%s\n", all_MIME_elements.mime_count, attachment_count, filecount, current_recursion_level, cur->content_type_string, cur->filename);
	}
	return cur;
}

static void dup_free(char *s)
{
	if ((s != NULL) && s[0])
		free(s);
}

void MIME_element_remove (MIME_element* cur)
{
	// LOGGER_log("%s:%d:%s:start\n",FL,__func__);

	if (cur->f != NULL) {
		fclose(cur->f);
	}
	dup_free(cur->fullpath);
	dup_free(cur->filename);
	dup_free(cur->content_type_string);
	dup_free(cur->content_transfer_encoding);
	dup_free(cur->name);

	free(cur);
}

/*

    FILE *fo;
    struct stat st;

    // Determine a file name we can use.
    do {

    }
    while (stat(glb.doubleCRname, &st) == 0);


    fo = fopen(glb.doubleCRname,"w");
    if (!fo)
    {
        LOGGER_log("%s:%d:MIMEH_save_doubleCR:ERROR: unable to open '%s' to write (%s)", FL,glb.doubleCRname,strerror(errno));
        return -1;
    }
*/


//------Dynamic array function definitions------
// Array initialization
void arrayInit(dynamic_array** arr_ptr)
{
	dynamic_array *container;
	container = (dynamic_array*)malloc(sizeof(dynamic_array));
	if(!container) {
		printf("Memory Allocation Failed\n");
		exit(0);
	}

	container->size = 0;
	container->capacity = INITIAL_SIZE;
	container->array = (MIME_element **)malloc(INITIAL_SIZE * sizeof(MIME_element*));
	if (!container->array){
		printf("Memory Allocation Failed\n");
		exit(0);
	}

	*arr_ptr = container;
}

//  Insertion Operation
void insertItem(dynamic_array* container, MIME_element* item)
{
	if (container->size == container->capacity) {
		MIME_element **temp = container->array;
		container->capacity <<= 1;
		container->array = realloc(container->array, container->capacity * sizeof(MIME_element*));
		if(!container->array) {
			printf("Out of Memory\n");
			container->array = temp;
			return;
		}
	}
	container->array[container->size++] = item;
}

// Retrieve Item at Particular Index
MIME_element* getItem(dynamic_array* container, int index)
{
	if(index >= container->size) {
		printf("Index Out of Bounds\n");
		return NULL;
	}
	return container->array[index];
}

// Update Operation
void updateItem(dynamic_array* container, int index, MIME_element* item)
{
	if (index >= container->size) {
		printf("Index Out of Bounds\n");
		return;
	}
	container->array[index] = item;
}

// Delete Item from Particular Index
void deleteItem(dynamic_array* container, int index)
{
	if(index >= container->size) {
		printf("Index Out of Bounds\n");
		return;
	}

	for (int i = index; i < container->size; i++) {
		container->array[i] = container->array[i + 1];
	}
	container->size--;
}

// Array Traversal
void printArray(dynamic_array* container)
{
	printf("Array elements: ");
	for (int i = 0; i < container->size; i++) {
		printf("%p ", container->array[i]);
	}
	printf("\nSize: ");
	printf("%lu", container->size);
	printf("\nCapacity: ");
	printf("%lu\n", container->capacity);
}

// Freeing the memory allocated to the array
void freeArray(dynamic_array* container)
{
	free(container->array);
	free(container);
}
/*----------END OF MIME.c------------*/
