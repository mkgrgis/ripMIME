
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ffget.h"
#include "mime_element.h"
#include "logger.h"

#ifndef FL
#define FL __FILE__, __LINE__, __func__
#endif

/* Dynamic array support*/
#define INITIAL_SIZE 8

// function prototypes
//  array container functions
void arrayInit(dynamic_array** arr_ptr);
void freeArray(dynamic_array* container);
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

MIME_element* MIME_element_add_with_path (char* fullpath, RIPMIME_output *unpack_metadata, struct MIMEH_header_info *hinfo, int attachment_count, int filecount)
{
    MIME_element *cur = malloc(sizeof(MIME_element));
    char* fn = strrchr(fullpath, '/');
    if (!fn)
       fn = fullpath;

    LOGGER_log("%s:%d:%s:start\n",FL);

    insertItem(all_MIME_elements.mime_arr, cur);
    cur->hinfo = hinfo;
    cur->id = all_MIME_elements.mime_count++;
    cur->fullpath = fullpath;

    cur->f = fopen(cur->fullpath,"wb");
    if (cur->f == NULL) {
        LOGGER_log("%s:%d:%s:ERROR: cannot open %s for writing",FL,cur->fullpath);
        return cur;
    }

    LOGGER_log("%s:%d:%s:DEBUG: Decoding [encoding=%d] to %s\n",FL, hinfo->content_transfer_encoding, cur->fullpath);

    if (unpack_metadata != NULL && unpack_metadata->unpack_mode == RIPMIME_UNPACK_MODE_LIST_FILES && cur->f != NULL) {
        fprintf (stdout, "%d|%d|%d|%d|%s|%s\n", all_MIME_elements.mime_count, attachment_count, filecount, (hinfo != NULL) ? hinfo->current_recursion_level : "?", (hinfo != NULL) ? hinfo->content_type_string : "", (hinfo != NULL) ? hinfo->filename : fn);
    }
    return cur;
}

MIME_element* MIME_element_add (RIPMIME_output *unpack_metadata, struct MIMEH_header_info *hinfo, int attachment_count, int filecount)
{
    int fullpath_len = strlen(unpack_metadata->dir) + strlen(hinfo->filename) + 3 * sizeof(char);
    char *fullpath = (char*)malloc(fullpath_len);

    snprintf(fullpath,fullpath_len,"%s/%s",unpack_metadata->dir,hinfo->filename);
    return MIME_element_add_with_path(fullpath, unpack_metadata, hinfo, attachment_count, filecount);
}

void MIME_element_remove (MIME_element* cur)
{
    LOGGER_log("%s:%d:%s:start\n",FL);

    if (cur->f != NULL) {
        fclose(cur->f);
    }
    free(cur->fullpath);
    free(cur);
}

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
