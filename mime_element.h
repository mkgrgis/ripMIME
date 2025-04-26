
#ifndef MIME_ELEMENT
#define MIME_ELEMENT
/* mime_element.h */

/* unpack modes */
#define RIPMIME_UNPACK_MODE_TO_DIRECTORY	0
#define RIPMIME_UNPACK_MODE_COUNT_FILES		1
#define RIPMIME_UNPACK_MODE_LIST_FILES		2

struct mime_output
{
	char *dir;
	int unpack_mode;
	// int fragment_number; will be used later
};
typedef struct mime_output RIPMIME_output;

typedef struct {
	void* parent;
	int id;
	char* directory;
	char* filename;
	char* fullpath;
	FILE* f;
	char* content_type_string;
	char* content_transfer_encoding;
	char* name;
} MIME_element;

typedef struct {
	size_t size;
	size_t capacity;
	MIME_element** array;
} dynamic_array;

typedef struct {
	int mime_count;
	dynamic_array* mime_arr;
} all_MIME_elements_s;

extern all_MIME_elements_s all_MIME_elements;

void all_MIME_elements_init (void);
MIME_element* MIME_element_add (void* parent, RIPMIME_output *unpack_metadata, char* filename, char* content_type_string, char* content_transfer_encoding, char* name, int current_recursion_level, int attachment_count, int filecount);
void MIME_element_remove (MIME_element* cur);
void printArray(dynamic_array* container);
void freeArray(dynamic_array* container);

#endif
