#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "logger.h"
#include "pldstr.h"
#include "bt-int.h"
#include "bytedecoders.h"
#include "mime_element.h"
#include "olestream-unwrap.h"
#include "ole.h"

/** Sector ID values (predefined) **/
#define OLE_SECTORID_FREE					-1 /** Unallocated sector **/
#define OLE_SECTORID_ENDOFCHAIN				-2 /** Sector marks the end of the a sector-ID chain **/
#define OLE_SECTORID_SAT					-3 /** Sector used by sector allocation Table  **/
#define OLE_SECTORID_MSAT					-4 /** Sector used by master sector allocation Table **/

// Main header accessors
#define header_id(x)							((x) +0)
#define header_clid(x)							((x) +0x08)
#define header_minor_version(x)					((x) +0x18)
#define header_dll_version(x)					((x) +0x1a)
#define header_byte_order(x)					((x) +0x1c)
#define header_sector_shift(x)					((x) +0x1e)
#define header_mini_sector_shift(x)				((x) +0x20)
#define header_fat_sector_count(x)				((x) +0x2c)
#define header_directory_stream_start_sector(x)	((x) +0x30)
#define header_mini_cutoff_size(x)				((x) +0x38)
#define header_mini_fat_start(x)				((x) +0x3c)
#define header_mini_fat_sector_count(x)			((x) +0x40)
#define header_dif_start_sector(x)				((x) +0x44)
#define header_dif_sector_count(x)				((x) +0x48)
#define header_fat_sectors(x)					((x) +0x4c)

//Property Storage accessor macros
#define pps_rawname(x)				((x) +0)
#define pps_sizeofname(x)			((x) +0x40)
#define pps_type(x)					((x) +0x42)
#define pps_previouspps(x)			((x) +0x44)
#define pps_nextpps(x)				((x) +0x48)
#define pps_directorypps(x)			((x) +0x4c)
#define pps_time1seconds(x)			((x) +0x64)
#define pps_time1days(x)			((x) +0x68)
#define pps_time2seconds(x)			((x) +0x6c)
#define pps_time2days(x)			((x) +0x70)
#define pps_propertystart(x)		((x) +0x74)
#define pps_sizeofproperty(x)		((x) +0x78)

// Type lenghts
#define LEN_BYTE	1
#define LEN_USHORT	2
#define LEN_ULONG	4

// Directory types
#define STGTY_INVALID	0
#define STGTY_STORAGE	1
#define STGTY_STREAM	2
#define STGTY_LOCKBYTES	3
#define STGTY_PROPERTY	4
#define STGTY_ROOT		5

// Directory tag colours
#define DE_RED		0
#define DE_BLACK	1

#define DOLE	if (OLE_DNORMAL(ole->debug))
#define VOLE	if (ole->verbose)

unsigned char OLE_id_v2[]={ 0xd0, 0xcf, 0x11, 0xe0, 0xa1, 0xb1, 0x1a, 0xe1 };
unsigned char OLE_id_v1[]={ 0x0e, 0x11, 0xfc, 0x0d, 0xd0, 0xcf, 0x11, 0xe0 };

/*-----------------------------------------------------------------\
  Function Name	: OLE_version
  Returns Type	: int
  ----Parameter List
  1. void , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int OLE_version( void )
{
	fprintf(stderr,"ripOLE: %s\n", LIBOLE_VERSION);

	return 0;
}

/*-----------------------------------------------------------------\
  Function Name	: OLE_init
  Returns Type	: int
  ----Parameter List
  1. struct OLE_object *ole , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:
20041127-2029:PLD: Added file_size initialization
\------------------------------------------------------------------*/
int OLE_init( struct OLE_object *ole )
{
	ole->debug = 0;
	ole->verbose = 0;
	ole->quiet = 0;
	ole->filename_report_fn = NULL;
	ole->f = NULL;
	ole->file_size = 0;

	ole->FAT = NULL;
	ole->FAT_limit = NULL;
	ole->miniFAT = NULL;
	ole->miniFAT_limit = NULL;

	ole->header_block[0] = '\0';
	ole->ministream = NULL;
	ole->properties = NULL;

	ole->header.sector_shift = 0;
	ole->header.mini_sector_shift = 0;

	return OLE_OK;
}

/*-----------------------------------------------------------------\
  Function Name	: OLE_dir_init
  Returns Type	: int
  ----Parameter List
  1. struct OLE_directory_entry *dir , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int OLE_dir_init(struct OLE_directory_entry *dir )
{
	memset(dir->element_name,'\0', OLE_DIRECTORY_ELEMENT_NAME_SIZE);
	dir->element_name_byte_count = 0;

	dir->element_type = 0;
	dir->element_colour = 0;

	dir->left_child = 0;
	dir->right_child = 0;
	dir->root = 0;

	dir->class[0] = '\0';
	dir->userflags = 0;
	dir->timestamps[0] = '\0';
	dir->start_sector = 0;
	dir->stream_size = 0;

	return 0;
};

/*-----------------------------------------------------------------\
  Function Name	: OLE_set_verbose
  Returns Type	: int
  ----Parameter List
  1. struct OLE_object *ole, 
  2.  int level , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int OLE_set_verbose( struct OLE_object *ole, int level )
{
	ole->verbose = level;
	return OLE_OK;
}



/*-----------------------------------------------------------------\
  Function Name	: OLE_set_quiet
  Returns Type	: int
  ----Parameter List
  1. struct OLE_object *ole, 
  2.  int level , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int OLE_set_quiet( struct OLE_object *ole, int level )
{
	ole->quiet = level;
	ole->verbose = 0;
	ole->debug = 0;

	return OLE_OK;
}


/*-----------------------------------------------------------------\
  Function Name	: OLE_set_debug
  Returns Type	: int
  ----Parameter List
  1. struct OLE_object *ole, 
  2.  int level , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int OLE_set_debug( struct OLE_object *ole, int level )
{
	ole->debug = level;
	if (ole->debug > 0) LOGGER_log("%s:%d:%s: Debug level set to %d",FL,__func__, ole->debug);

	return OLE_OK;
}


/*-----------------------------------------------------------------\
  Function Name	: OLE_set_save_unknown_streams
  Returns Type	: int
  ----Parameter List
  1. struct OLE_object  *ole, 
  2.  int level , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int OLE_set_save_unknown_streams( struct OLE_object  *ole, int level )
{
	ole->save_unknown_streams = level;

	return OLE_OK;
}


/*-----------------------------------------------------------------\
  Function Name	: OLE_sectorpos
  Returns Type	: int
  ----Parameter List
  1. struct OLE_object *ole, 
  2.  int SID , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:
Given a sector ID, this function will return the file position
offset.

Assumes that the offset for the file starts at 512 bytes (which
is the size of the OLE header)

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int OLE_sectorpos( struct OLE_object *ole, int SID )
{
	int pos = 0;

	pos = 512 +(SID *ole->header.sector_size);

	return pos;
}


/*-----------------------------------------------------------------\
  Function Name	: OLE_get_block
  Returns Type	: int
  ----Parameter List
  1. struct OLE_object *ole, 
  2.  int block_index,  Block indexes / Sector ID's are signed ints.
  3.  unsigned char *block_buffer , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int OLE_get_block( struct OLE_object *ole, int block_index, unsigned char *block_buffer )
{
	if (block_buffer == NULL)
	{
		LOGGER_log("%s:%d:%s:ERROR: Block buffer is NULL",FL,__func__);
		return -1;
	}

	if (ole->f != NULL)
	{
		int read_count = 0;
		int fseek_result = 0;
		size_t offset = 0;
		unsigned char *bb = NULL;

		bb = malloc(sizeof(unsigned char) *ole->header.sector_size);
		if (bb == NULL)
		{
			LOGGER_log("%s:%d:%s:ERROR: Cannot allocate %d bytes for OLE block",FL,__func__, ole->header.sector_size);
			return -1;
		}

		DOLE LOGGER_log("%s:%d:%s:DEBUG: BlockIndex=%d, Buffer=0x%x",FL,__func__, block_index, block_buffer);

		//20051211-2343:PLD: offset = (block_index +1) << ole->header.sector_shift;
		offset = OLE_sectorpos(ole, block_index);

		DOLE LOGGER_log("%s:%d:%s:DEBUG: Read offset in file = 0x%x size to read= 0x%x",FL,__func__,offset,ole->header.sector_size);

		fseek_result = fseek(ole->f, offset, SEEK_SET);
		if (fseek_result != 0)
		{
			if (bb != NULL) { free(bb); bb = NULL; }
			LOGGER_log("%s:%d:%s:ERROR: Seek failure (block=%d:%d)",FL,__func__, block_index,offset, strerror(errno));
			return OLEER_GET_BLOCK_SEEK;
		}

		//read_count = fread(block_buffer, sizeof(unsigned char), ole->header.sector_size, ole->f);
		read_count = fread(bb, sizeof(unsigned char), ole->header.sector_size, ole->f);
		DOLE LOGGER_log("%s:%d:%s:DEBUG: Read %d byte of data",FL,__func__,read_count);
		if (read_count != (int)ole->header.sector_size)
		{
			if (bb != NULL){ free(bb); bb = NULL; }
			VOLE LOGGER_log("%s:%d:Mismatch in bytes read. Requested %d, got %d\n", FL,__func__, ole->header.sector_size, read_count);
			return OLEER_GET_BLOCK_READ;
		}

		DOLE LOGGER_log("%s:%d:%s:DEBUG: Copying over memory read from file",FL,__func__);

		memcpy(block_buffer, bb, ole->header.sector_size);

		DOLE LOGGER_log("%s:%d:%s:DEBUG: memory block copied to block_buffer",FL,__func__);

		/* We're now done with BB, dispose of it */
		if (bb) { free(bb); bb = NULL; }

		DOLE LOGGER_log("%s:%d:%s:DEBUG: Disposed of temporary bb block",FL,__func__);

	} else {
		LOGGER_log("%s:%d:%s:ERROR: OLE file is closed\n",FL,__func__);
		return -1;
	}

	DOLE LOGGER_log("%s:%d:%s:DEBUG: Done",FL,__func__);

	return OLE_OK;
}


/*-----------------------------------------------------------------\
  Function Name	: OLE_get_miniblock
  Returns Type	: int
  ----Parameter List
  1. struct OLE_object *ole, 
  2.  unsigned int block_index, 
  3.  unsigned char *block_buffer , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int OLE_get_miniblock( struct OLE_object *ole, int block_index, unsigned char *block_buffer )
{
	if (ole->ministream)
	{
		int offset = block_index << ole->header.mini_sector_shift;

		memcpy( block_buffer, ole->ministream +offset, ole->header.mini_sector_size);
	}

	return OLE_OK;
}


/*-----------------------------------------------------------------\
  Function Name	: OLE_dbstosbs
  Returns Type	: int
  ----Parameter List
  1. char *raw_string, 
  2.  size_t char_count, 
  3.  char *clean_string , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int OLE_dbstosbs( char *raw_string, size_t byte_count, char *clean_string, int clean_string_len )
{
	char *limit = raw_string +byte_count -1;

	clean_string_len--;

	while ((raw_string < limit)&&(byte_count >0)&&(byte_count--)&&(clean_string_len--))
	{
		int v = (char)*raw_string;

		if (isprint(v))
		{
			*clean_string = v;
			clean_string++;
		}

		raw_string += 2;
	}
	*clean_string = '\0';

	return OLE_OK;
}

/*-----------------------------------------------------------------\
  Function Name	: OLE_print_string
  Returns Type	: int
  ----Parameter List
  1. char *string, 
  2.  size_t length , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int OLE_print_string( char *string, size_t char_count)
{
	while (char_count--)
	{
		printf("%c",*string);
		string += 2;
	}
	return OLE_OK;
}
/*-----------------------------------------------------------------\
  Function Name	: OLE_print_sector
  Returns Type	: int
  ----Parameter List
  1. struct OLE_object *ole, 
  2.  unsigned char *sector , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int OLE_print_sector( struct OLE_object *ole, char *sector, unsigned int bytes)
{
	int current_byte;
	int ubytes = bytes;

	printf("\n");
	for (current_byte = 0; current_byte < ubytes; current_byte++ )
	{
		printf("%02X ", *(sector +current_byte));
		if (((current_byte+1) %32)==0) {
			int j;
			for (j = current_byte -31; j <=current_byte; j++)
			{
				if (isalnum(*(sector +j))) printf("%c",*(sector+j));
				else printf(".");
			}
			printf("\n");
		}
	}
	printf("\n");

	return OLE_OK;
}

/*-----------------------------------------------------------------\
  Function Name	: OLE_is_OLE_file
  Returns Type	: int
  ----Parameter List
  1. struct OLE_object *ole , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int OLE_is_file_OLE( struct OLE_object *ole )
{

	if (memcmp(OLE_id_v1, ole->header_block, sizeof(OLE_id_v1))==0) return 1;
	if (memcmp(OLE_id_v2, ole->header_block, sizeof(OLE_id_v2))==0) return 1;

	return 0;

}

/*-----------------------------------------------------------------\
  Function Name	: OLE_get_header
  Returns Type	: int
  ----Parameter List
  1. struct OLE_object *ole , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int OLE_get_header( struct OLE_object *ole )
{
	int result = 0;
	ole->header.sector_size = OLE_HEADER_BLOCK_SIZE;
	result = OLE_get_block( ole, -1, ole->header_block );
	if (result != 0) return result;

	if (OLE_is_file_OLE( ole ) == 0)
	{
		return OLEER_NOT_OLE_FILE;
	}

	return OLE_OK;
}

/*-----------------------------------------------------------------\
  Function Name	: OLE_convert_header
  Returns Type	: int
  ----Parameter List
  1. struct OLE_object *ole , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int OLE_convert_header( struct OLE_object *ole )
{
	struct OLE_header *h;
	unsigned char *hb; /** pointer to the header block **/
	unsigned char *fat_start;
	unsigned int i;

	h = &(ole->header);
	hb = ole->header_block;

	/** Note that the header_*(hb) calls are actually macros which are
	 ** defined in the ole.h file.  These macros basically take the
	 ** hb value and add the required offset, they don't affect the
	 ** value of hb (or they certainly SHOULD NOT! )
	 **/

	h->minor_version = get_uint16((char *)header_minor_version(hb));
	h->dll_version = get_uint16((char *)header_dll_version(hb));
	h->byte_order = get_uint16((char *)header_byte_order(hb)); /** 0xFEFF = Little endian, 0xFFFE = big endian **/
	h->sector_shift = get_uint16((char *)header_sector_shift(hb));
	h->sector_size = 1 << h->sector_shift;

	h->mini_sector_shift = get_uint16((char *)header_mini_sector_shift(hb));
	h->mini_sector_size = 1 << h->mini_sector_shift;

	h->fat_sector_count = get_uint32((char *)header_fat_sector_count(hb)); /** Total number of sectors use for the SAT **/
	h->directory_stream_start_sector = get_uint32((char *)header_directory_stream_start_sector(hb)); /** Start sector-ID for the DIRECTORY STREAM **/
	h->mini_cutoff_size = get_uint32((char *)header_mini_cutoff_size(hb)); 
	h->mini_fat_start = get_uint32((char *)header_mini_fat_start(hb));
	h->mini_fat_sector_count = get_uint32((char *)header_mini_fat_sector_count(hb));
	h->dif_start_sector = get_uint32((char *)header_dif_start_sector(hb));
	h->dif_sector_count = get_uint32((char *)header_dif_sector_count(hb));

	/** Compute the maximum possible sector number by taking our OLE filesize
	 ** and dividing it by the size of our sector size.  While this isn't 
	 ** absolutely accurate it is at least useful in providing us with an 
	 ** upper-bound of what is an acceptable sector ID **/
	ole->last_sector = ole->file_size  >> h->sector_shift;

	/** Decode our first 109 sector-ID's into the master sector allocation table (MSAT/FAT) **/
	fat_start = header_fat_sectors(hb);
	for (i = 0; i < h->fat_sector_count; i++)
	{
		if (i >= OLE_HEADER_FAT_SECTOR_COUNT_LIMIT) break;
		h->FAT[i] = get_uint32( (char *)( fat_start +(LEN_ULONG *i)));
	}

	return OLE_OK;
}


/*-----------------------------------------------------------------\
  Function Name	: OLE_header_sanity_check
  Returns Type	: int
  ----Parameter List
  1. struct OLE_object *ole , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:
Determines the degree of insanity in the header, returning it
as an integer, 1 per degree of insanity.

--------------------------------------------------------------------
Changes:
20041127-2027:PLD: Initial version

\------------------------------------------------------------------*/
int OLE_header_sanity_check( struct OLE_object *ole )
{
	int insanity=0;
	int max_sectors;
	struct OLE_header *h;

	h = &(ole->header);

	max_sectors = ole->file_size / h->sector_size;

	if (h->sector_shift > 20) insanity++;
	if (h->mini_sector_shift > 10) insanity++;
	if (h->fat_sector_count < 0) insanity++;
	if (h->fat_sector_count > max_sectors) insanity++;
	if (h->directory_stream_start_sector > max_sectors) insanity++;

	return insanity;
}


/*-----------------------------------------------------------------\
  Function Name	: OLE_print_header
  Returns Type	: int
  ----Parameter List
  1. struct OLE_object *ole , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int OLE_print_header( struct OLE_object *ole )
{
	unsigned int i;
	struct OLE_header *h;

	h = &(ole->header);

	printf( "Minor version = %d\n"
			"DLL version = %d\n"
			"Byte order = %d\n\n"

			"Sector shift = %d\n"
			"Sector size  = %d\n"
			"Mini Sector shift = %d\n"
			"Mini sector size  = %d\n\n"

			"FAT sector count = %d\n"
			"First FAT sector = %d\n\n"

			"Maximum ministream size = %d\n\n"

			"First MiniFAT sector = %d\n"
			"MiniFAT sector count = %d\n\n"

			"First DIF sector = %d\n"
			"DIF sector count = %d\n"
			"--------------------------------\n"
			,h->minor_version
			,h->dll_version
			,h->byte_order
			,h->sector_shift
			,h->sector_size
			,h->mini_sector_shift
			,h->mini_sector_size
			,h->fat_sector_count
			,h->directory_stream_start_sector
			,h->mini_cutoff_size
			,h->mini_fat_start
			,h->mini_fat_sector_count
			,h->dif_start_sector
			,h->dif_sector_count
			);

	// Print out the FAT chain
	for (i = 0; i < h->fat_sector_count; i++)
	{
		if (i >= OLE_HEADER_FAT_SECTOR_COUNT_LIMIT) break; // We can't read beyond the 109th sector location
		printf("FAT[%d] = %d\n", i, h->FAT[i]);
	}

	return OLE_OK;
}


/*-----------------------------------------------------------------\
  Function Name	: OLE_convert_directory
  Returns Type	: int
  ----Parameter List
  1. unsigned char *buf, 
  2.  struct OLE_directory_entry *dir , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int OLE_convert_directory( struct OLE_object *ole, unsigned char *buf, struct OLE_directory_entry *dir )
{
	/** Converts a  raw block of 128 bytes from the file to a 
	 ** struct OLE_directory_entry data structure
	 **/

	/** Flush the element name **/
	memset( dir->element_name, '\0', OLE_DIRECTORY_ELEMENT_NAME_SIZE);

	/** The first 64 bytes of the structure are the element's name 
	 ** in 16-bite UNICODE, meaning a maximum of 31 characters when
	 ** we account for the trailing zero byte 
	 **/

	/** Copy the first 64 bytes of our *buf parameter into the element name **/
	memcpy( dir->element_name, buf, OLE_DIRECTORY_ELEMENT_NAME_SIZE );

	/** how many bytes of the above 64 bytes are used for the name (NOT CHARACTERS!), **
	 ** example, for a 8 character string with a trailing zero we use **
	 ** (8+1)*2 = 18 bytes
	 **/
	dir->element_name_byte_count = get_int16( (char *) buf + 0x40 );

	/** Element type is of the following: 
	 ** 0x00 - empty 
	 ** 0x01 - user storage  
	 ** 0x02 - user stream
	 ** 0x03 - lock bytes (we don't know what this is for)
	 ** 0x04 - property (again, we don't know)
	 ** 0x05 - root storage
	 **/
	dir->element_type = get_int8((char *) buf +0x42 );

	/** Element colour for the red-black tree:
	 ** 0x00 - Red
	 ** 0x01 - Black
	 **/
	dir->element_colour = get_int8((char*) buf +0x43 );

	/** Directory ID (DID) of the left child, -1 if no sibling **/
	dir->left_child = get_uint32((char*) buf +0x44 );

	/** Directory ID (DID) of the right child, -1 if no sibling **/
	dir->right_child = get_uint32((char *) buf +0x48 );


	/** Directory ID (DID) of the root node entry of the RB tree of all
	 ** storage members (if this entry is a storage), else -1. 
	 **/
	dir->root = get_uint32((char*) buf +0x4c );

	memcpy( dir->class, buf +0x50, 16 );
	dir->userflags = get_uint32((char*) buf +0x60 );
	memcpy( dir->timestamps, buf +0x64, 16 ); /** Actually consists of 2 8 byte stamps **/

	/** Sector ID of the first sector or short-sector **/
	dir->start_sector = get_uint32((char*) buf +0x74 );

	/** Size of this stream **/
	DOLE LOGGER_log("%s:%d:OLE_directory_entry:DEBUG: stream size = 0x%x %x %x %x"
			,FL,__func__
			,*(buf +0x78)
			,*(buf +0x79)
			,*(buf +0x7A)
			,*(buf +0x7B)
			);
	dir->stream_size = get_uint32((char *) buf +0x78 );

	return OLE_OK;
}

/*-----------------------------------------------------------------\
  Function Name	: OLE_print_directory
  Returns Type	: int
  ----Parameter List
  1. struct OLE *ole, 
  2.  struct OLE_directory_entry *dir , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int OLE_print_directory( struct OLE_object *ole, struct OLE_directory_entry *dir )
{
	char element[64];
	OLE_dbstosbs( dir->element_name, dir->element_name_byte_count, element, sizeof(element) );

	printf(
			"Element Name = %s\n"
			"Element type = %d\n"
			"Element colour = %d\n"
			"Left Child = %d\n"
			"Right Child = %d\n"
			"Root = %d\n"
			"User flags = %d\n"
			"Start sector = %ld\n"
			"Stream Size = %d\n"
			,element
			,dir->element_type
			,dir->element_colour
			,dir->left_child
			,dir->right_child
			,dir->root
			,dir->userflags
			,dir->start_sector
			,dir->stream_size
			);

	return OLE_OK;
}


/*-----------------------------------------------------------------\
  Function Name	: OLE_load_FAT
  Returns Type	: int
  ----Parameter List
  1. struct OLE_object *ole , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int OLE_load_FAT( struct OLE_object *ole )
{
	unsigned int FAT_size;

	FAT_size = ole->header.fat_sector_count << ole->header.sector_shift;
	DOLE LOGGER_log("%s:%d:%s:DEBUG:Allocating for %d sectors (%d bytes) \n"
			,FL,__func__,ole->header.fat_sector_count, FAT_size);

	ole->FAT = malloc( FAT_size *sizeof(unsigned char));
	ole->FAT_limit = ole->FAT +FAT_size;
	if (ole->FAT != NULL)
	{
		unsigned int i;
		unsigned char *fat_position = ole->FAT;
		unsigned int sector_count = ole->header.fat_sector_count;

		if (sector_count > OLE_HEADER_FAT_SECTOR_COUNT_LIMIT)
		{
			sector_count = OLE_HEADER_FAT_SECTOR_COUNT_LIMIT;
			DOLE LOGGER_log("%s:%d:%s:DEBUG: sector count greater than limit; set to %d",FL,__func__, sector_count);
		}

		// Load in all our primary-FAT sectors from the OLE file
		for (i = 0; i < sector_count; i++)
		{
			int getblock_result = 0;

			DOLE LOGGER_log("%s:%d:%s:DEBUG: Loading sector %d",FL,__func__, i);
			getblock_result = OLE_get_block(ole, ole->header.FAT[i], fat_position);
			if (getblock_result != 0)
			{
				// if the get block fails, return the error - but keep the FAT
				//		pointer alive - so that we can facilitate debugging
				//		otherwise the caller is always going to get a NULL pointer
				//		and have no idea to what extent the data was read.
				//
				// This behavior may be changed later - but for now (beta development)
				//		it'll be okay to leave it here - just make sure we know to
				//		free the FAT block later.

				return getblock_result;
			}

			fat_position += ole->header.sector_size;
			if (fat_position > ole->FAT_limit)
			{ 
				LOGGER_log("%s:%d:%s:DEBUG: FAT boundary limit exceeded %p > %p", FL,__func__, fat_position, ole->FAT_limit); 
				return -1;
			}
		}

		// If our DIF count is > 0, this means we have a pretty big
		//		file on hand (> 7Mb) and thus we now have to do some
		//		fancy double-dereferenced sector request - enough to 
		//		twist your brain if you're not alert, you have been
		//		warned.

		if (ole->header.dif_sector_count > 0)
		{
			unsigned char *fat_block;
			unsigned char *fat_block_end;
			unsigned int current_sector = ole->header.dif_start_sector;

			DOLE LOGGER_log("%s:%d:%s:DEBUG: Allocating %d bytes to fat_block\n",FL,__func__, ole->header.sector_size);

			fat_block = malloc( ole->header.sector_size );

			if (fat_block == NULL) 
			{
				LOGGER_log("%s:%d:%s:ERROR: Unable to allocate %d bytes\n", FL,__func__, ole->header.sector_size);
				return -1;
				//			exit(1);
			}

			// We need to know where the end of this block is - because it's
			//		used to show us where the NEXT FAT block is going to come from
			//		NOTE - this only occurs if we do have another block, else 
			//		we'll simply have to just realise that we don't need any more
			//		blocks and stop with this one.

			fat_block_end = fat_block +ole->header.sector_size -LEN_ULONG;

			// We know we've got 'dif_sector_count' blocks to read, each of
			//		these blocks hold no more than 127 sector addresses which
			//		contain the actual FAT data we're after (this is the double
			//		dereference bit that twists your brain )

			DOLE LOGGER_log("%s:%d:%s:DEBUG: Loading DIF sectors (count = %d)",FL,__func__,ole->header.dif_sector_count);

			for (i = 0; i < ole->header.dif_sector_count; i++)
			{
				int import_sector;
				unsigned char *DIF = fat_block;
				int tick = 0;
				int getblock_result;

				DOLE LOGGER_log("%s:%d:%s:DEBUG: Reading DIF/XBAT index-data[%d] from sector 0x%x",FL,__func__,i,current_sector);
				getblock_result = OLE_get_block(ole, current_sector, fat_block);
				if (getblock_result != OLE_OK)
				{
					if (fat_block) free(fat_block);
					return getblock_result;
				}

				if (OLE_DPEDANTIC(ole->debug)) OLE_print_sector(ole, (char *)fat_block, ole->header.sector_size);

				// Now, traverse this block until we hit a < 0
				//		If we get what is a non-valid sector value
				//		we know we've reached the end of the valid
				//		sectors from which to read more extended FAT
				//		data.

				do {
					import_sector = get_int32( (char *) DIF );
					DOLE LOGGER_log("%s:%d:%s:DEBUG: import sector = 0x%x",FL,__func__,import_sector);
					if (import_sector >= 0)
					{
						if (fat_position +ole->header.sector_size <= ole->FAT_limit)
						{
							DOLE LOGGER_log("%s:%d:%s:DEBUG: Reading DIF/XBAT-data[%d] from sector 0x%x",FL,__func__,tick,import_sector);
							getblock_result = OLE_get_block(ole, import_sector, fat_position);
							if (getblock_result != OLE_OK)
							{
								LOGGER_log("%s:%d:%s:ERROR: Not able to load block, import sector = 0x%x, fat position = 0x%x",FL,__func__, import_sector, fat_position);
								if (fat_block) free(fat_block);
								return getblock_result;
							}

							fat_position += ole->header.sector_size;
							DOLE LOGGER_log("%s:%d:%s:DEBUG: FAT position = 0x%x (start = 0x%x, end = 0x%x)"
									,FL,__func__
									,fat_position
									,fat_block
									,ole->FAT_limit
									);

							//if (fat_position +ole->header.sector_size > ole->FAT_limit)
							if (fat_position > ole->FAT_limit)
							{ 
								DOLE LOGGER_log("%s:%d:%s:ERROR: FAT memory boundary limit exceeded %p >= %p",FL,__func__,fat_position,ole->FAT_limit); 
								if (fat_block) free(fat_block);
								return OLEER_MEMORY_OVERFLOW;
							}
							tick++;
							DIF += LEN_ULONG;
						}  else {
							LOGGER_log("%s:%d:%s:ERROR: FAT memory boundary limit exceeded %p >= %p",FL,__func__,fat_position,ole->FAT_limit); 
							if (fat_block) free(fat_block);
							return OLEER_MEMORY_OVERFLOW;
						}
					} else {
						VOLE LOGGER_log("%s:%d:%s:ERROR: sector request was negative (%d)",FL,__func__, import_sector);
					}

					DOLE LOGGER_log("%s:%d:%s:DEBUG: DIF = 0x%x",FL,__func__,DIF);
				} while ((import_sector >= 0)&&(DIF < fat_block_end));

				// Get the next sector of DIF/XBAT data ...
				//
				// If we still have more sectors full of extended FAT
				//		sectors that we have to read, then we neet to
				//		obtain the address of the next FAT-sector filled
				//		sector

				if ( i < ole->header.dif_sector_count -1 )
				{
					current_sector = get_uint32((char *) fat_block_end );
					DOLE LOGGER_log("%s:%d:%s:DEBUG: Next DIF/XBAT index sector located at 0x%x",FL,__func__,current_sector);
					if (current_sector < 0) break;
				}
			} // For every DIF/XBAT sector we're supposed to read

			if (fat_block) free(fat_block);
		} // If we have DIF/XBAT sectors to read into the FAT

	} // If we managed to allocate memory for our FAT table

	return OLE_OK;
}

/*-----------------------------------------------------------------\
  Function Name	: OLE_follow_chain
  Returns Type	: int
  ----Parameter List
  1. struct OLE_object *ole, 
  2.  int FAT_sector_start , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int OLE_follow_chain( struct OLE_object *ole, int FAT_sector_start )
{
	int current_sector = FAT_sector_start;
	int chain_length=0;
	int last_sector_of_file = ole->last_sector;
	int break_out = 0;
	struct bti_node *n;

	BTI_init(&n);

	if (FAT_sector_start < 0) return 0;

	DOLE LOGGER_log("%s:%d:%s:DEBUG: Starting chain follow at sector %d",FL,__func__, FAT_sector_start );

	do {
		unsigned int next_sector;
		unsigned char *next_sector_location;

		next_sector_location = ole->FAT +(LEN_ULONG *current_sector);
		if (next_sector_location > (ole->FAT_limit -4)) {
			DOLE LOGGER_log("%s:%d:%s:DEBUG: ERROR: Next sector was outside of the limits of this file (%ld > %ld)",FL,__func__, next_sector_location, ole->FAT_limit);
			break;
		}

		//next_sector = get_4byte_value( ole->FAT +(LEN_ULONG *current_sector));
		next_sector = get_uint32((char*) next_sector_location );

		if (BTI_add(&n, next_sector) != 0)
		{
			DOLE LOGGER_log("%s:%d:%s:DEBUG: Sector collision, terminating chain traversal",FL,__func__);
			chain_length=-1;

			break;
		}

		DOLE LOGGER_log("%s:%d:%s:DEBUG: 0x%0X:%d)->(0x%0X:%d)\n",FL,__func__, current_sector, current_sector, next_sector, next_sector);

		// 20040729-10H37 Added this to prevent endless loop which sometimes occurs at sector 0

		if (next_sector == current_sector) break;

		//		fflush(stdout);
		current_sector = next_sector;
		chain_length++;

		/** Test to see if we should terminate this chain traversal **/
		switch (current_sector) {
			case OLE_SECTORID_MSAT:
			case OLE_SECTORID_SAT:
			case OLE_SECTORID_ENDOFCHAIN:
			case OLE_SECTORID_FREE:
				break_out=1;
				break;
			default:
				break_out=0;
		};
		 
		if (current_sector < 0) break_out = 1;

	} while ((break_out==0)&&(current_sector < last_sector_of_file));

	BTI_done(&n);

	return chain_length;
}

/*-----------------------------------------------------------------\
  Function Name	: OLE_follow_minichain
  Returns Type	: int
  ----Parameter List
  1. struct OLE_object *ole, 
  2.  int FAT_sector_start , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int OLE_follow_minichain( struct OLE_object *ole, int miniFAT_sector_start )
{
	//unsigned int current_sector = miniFAT_sector_start;
	int current_sector = miniFAT_sector_start;
	int chain_length=0;
	int break_out = 0;

	DOLE LOGGER_log("%s:%d:%s:DEBUG: Starting at sector %d",FL,__func__, miniFAT_sector_start);

	if (miniFAT_sector_start < 0) return 0;

	do {
		unsigned int next_sector;

		DOLE LOGGER_log("%s:%d:%s:DEBUG: Requesting 4-byte value at '%d'",FL,__func__, ole->miniFAT +(LEN_ULONG *current_sector));
		if (ole->miniFAT +(LEN_ULONG *current_sector) > ole->miniFAT_limit) {
			DOLE LOGGER_log("%s:%d:%s:DEBUG: Requested location is out of bounds\n",FL,__func__);
			return 0;
		}

		next_sector = get_uint32((char*) ole->miniFAT +(LEN_ULONG *current_sector));

		DOLE LOGGER_log("%s:%d:%s:DEBUG: Current Msector(0x%0X:%d)->next(0x%0X:%d)\n", FL,__func__, current_sector, current_sector, next_sector, next_sector);

		/** Check for conditions that indicate we should stop traversing this chain **/

		/** 1. We cannot point to ourselves **/
		if (current_sector == next_sector) break;

		chain_length++;
		current_sector = next_sector;

		/** Test for non-positive type sector ID's **/
		switch (current_sector) {
			case OLE_SECTORID_MSAT:
			case OLE_SECTORID_SAT:
			case OLE_SECTORID_ENDOFCHAIN:
			case OLE_SECTORID_FREE:
				break_out=1;
				break;
			default:
				break_out=0;
		};

		DOLE LOGGER_log("%s:%d:%s:DEBUG: current sector = %d",FL,__func__,current_sector);
	} while ((break_out==0)&&(current_sector <= ole->last_sector ));

	DOLE LOGGER_log("%s:%d:%s:DEBUG: Done.  Chainlength=%d",FL,__func__, chain_length);

	return chain_length;
}

/*-----------------------------------------------------------------\
  Function Name	: char
  Returns Type	: unsigned
  ----Parameter List
  1. *OLE_load_minichain( struct OLE_object *ole, 
  2.  int FAT_sector_start , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:
PLD:2003-Aug-28: Added sanity checking on the miniFAT_sector_start 
value so that we didn't try to load up a miniFAT starting on a 
negative value

\------------------------------------------------------------------*/
unsigned char *OLE_load_minichain( struct OLE_object *ole, int miniFAT_sector_start )
{

	int chain_length = 0;
	int current_sector = miniFAT_sector_start;
	unsigned char *buffer;
	unsigned char *bp;

	DOLE LOGGER_log("%s:%d:%s:DEBUG: Loading minichain starting at %d",FL,__func__, miniFAT_sector_start);

	// Added this sanity checking 2003 Aug 28
	if (miniFAT_sector_start < 0) return NULL;

	chain_length = OLE_follow_minichain( ole, miniFAT_sector_start );
	DOLE LOGGER_log("%s:%d:%s:DEBUG: Found %d mini-sectors to load (%d bytes)\n",FL,__func__, chain_length, chain_length *ole->header.mini_sector_size);

	// 20040911-21H59
	// If our chain is 0 length, then there's nothing to return
	if (chain_length == 0) return NULL;

	bp = buffer = malloc( chain_length *ole->header.mini_sector_size *sizeof(unsigned char));
	if (buffer != NULL)
	{
		do {
			unsigned int next_sector;

			DOLE LOGGER_log("%s:%d:%s:DEBUG: Loading sector %d",FL,__func__, current_sector);
			OLE_get_miniblock( ole, current_sector, bp );
			bp += ole->header.mini_sector_size;

			next_sector = get_uint32((char *)( ole->miniFAT +(LEN_ULONG *current_sector)));
			current_sector = next_sector;
		} while ((current_sector != OLE_SECTORID_ENDOFCHAIN)&&(current_sector >= 0)&&(current_sector <= ole->last_sector));
	} else {
		LOGGER_log("%s:%d:OLE_get_miniblock:ERROR: Failed to allocate enough memory for miniChain",FL,__func__);
	}

	DOLE LOGGER_log("%s:%d:%s:DEBUG: Done. buffer=%p",FL,__func__, buffer);

	return buffer;
}


/*-----------------------------------------------------------------\
  Function Name	: char
  Returns Type	: unsigned
  ----Parameter List
  1. *OLE_load_chain( struct OLE_object *ole, 
  2.  int FAT_sector_start , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:
Make the loading aware of negative-value sectors so that it can
make more intelligent exit strategies.

\------------------------------------------------------------------*/
unsigned char *OLE_load_chain( struct OLE_object *ole, int FAT_sector_start )
{

	int chain_length = 0;
	int current_sector = FAT_sector_start;
	unsigned char *buffer = NULL;
	unsigned char *bp = NULL;


	ole->last_chain_size = 0;

	if (FAT_sector_start < 0) return NULL;

	DOLE LOGGER_log("%s:%d:%s:DEBUG: Loading chain, starting at sector %d",FL,__func__,FAT_sector_start);

	chain_length = OLE_follow_chain( ole, FAT_sector_start );
	DOLE LOGGER_log("%s:%d:%s:DEBUG: %d sectors need to be loaded",FL,__func__,chain_length);

	if (chain_length > 0)
	{
		size_t offset;

		offset = ole->last_chain_size = chain_length << ole->header.sector_shift;
		bp = buffer = malloc( offset *sizeof(unsigned char));
		if (buffer == NULL)
		{
			LOGGER_log("%s:%d:%s:ERROR: Cannot allocate %d bytes for OLE chain",FL,__func__,offset);
			return NULL;
		}

		if (buffer != NULL)
		{
			int tick = 0;
			unsigned char *bp_limit;
			bp_limit = bp +offset;

			do {
				int next_sector;

				DOLE LOGGER_log("%s:%d:%s:DEBUG: Loading sector[%d] %d",FL,__func__, tick, current_sector );

				ole->error = OLE_get_block( ole, current_sector, bp );
				if (ole->error != OLE_OK)
				{
					//FREE5					if (bp != NULL) free(bp);
					return NULL;
				}

				bp += ole->header.sector_size;
				if (bp > bp_limit) {  
					if (buffer != NULL) { free(buffer); bp = buffer = NULL; }
					VOLE LOGGER_log("%s:%d:%s:ERROR: Load-chain went over memory boundary",FL,__func__); 
					return NULL;
				};

				next_sector = get_uint32((char *)( ole->FAT +(LEN_ULONG *current_sector)));
				current_sector = next_sector;
				tick++;
			} while ((current_sector >= 0)&&(current_sector <= ole->last_sector));
		}
	}
	DOLE LOGGER_log("%s:%d:%s:DEBUG: Done loading chain",FL,__func__);

	return buffer;
}

/*-----------------------------------------------------------------\
  Function Name	: OLE_input_file_data_ini
  Returns Type	: int
  ----Parameter List
  1. struct OLE_object *ole, 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:
\------------------------------------------------------------------*/
int OLE_input_file_data_ini( struct OLE_object *ole )
{
		fseek(ole->f, 0L, SEEK_END);
		ole->file_size = ftell(ole->f);
		fseek(ole->f, 0L, SEEK_SET);
		if (ole->file_size < OLE_HEADER_BLOCK_SIZE) {
			fclose(ole->f);
			ole->f = NULL;
			return OLEER_NOT_OLE_FILE;
		}
		ole->last_sector = -1;
	return OLE_OK;
}

/*-----------------------------------------------------------------\
  Function Name	: OLE_open_file
  Returns Type	: int
  ----Parameter List
  1. struct OLE_object *ole, 
  2.  char *fullpath , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:
20041127-2033:PLD: Added ole->file_size setting so that we 
can use this in the sanity checking to see if the 
requested sectors are outside of the possible valid
filesize range.
20220425-mkgrgis: rewrite file size to seek, exclude FS operations
\------------------------------------------------------------------*/
int OLE_open_file( struct OLE_object *ole, char *fullpath )
{
	ole->f = fopen(fullpath,"r");
	if (ole->f == NULL)
	{
		if (ole->quiet == 0)
		{
			LOGGER_log("%s:%d:%s:ERROR:Cannot open %s for reading (%s)\n",FL,__func__,fullpath, strerror(errno));
		}
		return -1;
	}
	else
		return OLE_OK;
}

/*-----------------------------------------------------------------\
  Function Name	: OLE_open_directory
  Returns Type	: int
  ----Parameter List
  1. struct OLE_object *ole, 
  2.  char *directory , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int OLE_open_directory( struct OLE_object *ole, RIPMIME_output *unpack_metadata )
{
	int result=0;

	result = mkdir( unpack_metadata->dir, S_IRWXU );
	if ((result != 0)&&(errno != EEXIST))
	{
		LOGGER_log("%s:%d:%s:ERROR: %s",FL,__func__,strerror(errno));
	} else result = OLE_OK;

	return result;
}


/*-----------------------------------------------------------------\
  Function Name	: OLE_set_filename_report_fn
  Returns Type	: int
  ----Parameter List
  1. int (*ptr_to_fn)(char *) , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:
This is merely a passthrough function to the OLEUW one, we do
this in order to avoid having to force the calling parent from
having to #include the OLEUW headers as well

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int OLE_set_filename_report_fn( struct OLE_object *ole, int (*ptr_to_fn)(char *) )
{
	ole->filename_report_fn = ptr_to_fn;
	return OLE_OK;
}


/*-----------------------------------------------------------------\
  Function Name	: OLE_store_stream
  Returns Type	: int
  ----Parameter List
  1. struct OLE_object *ole, 
  2.  char *stream_name, 
  3.  char *directory, 
  4.  unsigned char *stream , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int OLE_store_stream( struct OLE_object *ole, char *stream_name, RIPMIME_output *unpack_metadata, char *stream, size_t stream_size )
{
	MIME_element* cur_mime = MIME_element_add (NULL, unpack_metadata, stream_name, "OLE", "OLE", "OLE", 0, 1, 0, __func__);

	size_t written_bytes = fwrite( stream, 1, stream_size, cur_mime->f );
	if (written_bytes != stream_size)
	{
		LOGGER_log("%s:%d:%s:WARNING: Only wrote %d of %d bytes to file %s",FL,__func__,written_bytes,stream_size,cur_mime->fullpath);
	}
	MIME_element_deactivate (cur_mime, unpack_metadata);

	if ((OLE_VNORMAL(ole->verbose))&&(ole->filename_report_fn != NULL))
	{
		ole->filename_report_fn( stream_name );
	}
	return OLE_OK;
}

/*-----------------------------------------------------------------\
  Function Name	: OLE_decode_done
  Returns Type	: int
  ----Parameter List
  1. struct OLE_object *ole , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
void OLE_decode_done( struct OLE_object *ole )
{
	/** Why weren't these active? (they were commented out ) **/
	DOLE LOGGER_log("%s:%d:%s:DEBUG: OLE FAT",FL,__func__);
	if (ole->FAT) free(ole->FAT);
	if (ole->miniFAT) free(ole->miniFAT);
	DOLE LOGGER_log("%s:%d:%s:DEBUG: OLE streams",FL,__func__);
	if (ole->ministream) free(ole->ministream);
	if (ole->properties) free(ole->properties);
}


/*-----------------------------------------------------------------\
  Function Name	: OLE_terminate_and_return
  Returns Type	: int
  ----Parameter List
  1. struct OLE_object *ole, 
  2.  int result , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int OLE_terminate_and_return( struct OLE_object *ole, int result )
{
	OLE_decode_done(ole);
	DOLE LOGGER_log("%s:%d:%s:DEBUG: ole->f close",FL,__func__);
	if (ole->f) fclose(ole->f);
	return result;
}

/*-----------------------------------------------------------------\
  Date Code:	: 20081101-020137
  Function Name	: OLE_decode_stream
  Returns Type	: int
  	----Parameter List
	1. struct OLE_object *ole, 
	2.   struct OLE_directory_entry *adir, 
	3.  char *decode_path , 
	------------------
Exit Codes	: 
Side Effects	: 
--------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int OLE_decode_stream( struct OLE_object *ole, struct OLE_directory_entry *adir, RIPMIME_output *unpack_metadata )
{
	unsigned char *stream_data;
	struct OLEUNWRAP_object oleuw;
	int decode_result = OLEUW_STREAM_NOT_DECODED;
	char element_name[64];
	int result = 0;

	memset(element_name, '\0', 64);
	OLE_dbstosbs( adir->element_name, adir->element_name_byte_count, element_name, 64 );

	DOLE LOGGER_log("%s:%d:%s:DEBUG: Decoding stream '%s'",FL,__func__, element_name);

	DOLE LOGGER_log("%s:%d:%s:DEBUG: Initializing stream unwrapper",FL,__func__);
	OLEUNWRAP_init(&oleuw);
	OLEUNWRAP_set_debug(&oleuw,ole->debug);
	OLEUNWRAP_set_verbose(&oleuw,ole->verbose);
	OLEUNWRAP_set_filename_report_fn(&oleuw, ole->filename_report_fn);
	OLEUNWRAP_set_save_unknown_streams(&oleuw, ole->save_unknown_streams);
	DOLE LOGGER_log("%s:%d:%s:DEBUG: Unwrap engine set.",FL,__func__);

	if (adir->stream_size >= ole->header.mini_cutoff_size)
	{
		/** Standard size sector stored stream **/
		/** Standard size sector stored stream **/
		/** Standard size sector stored stream **/
		DOLE LOGGER_log("%s:%d:%s:DEBUG:  Loading normal sized chain starting at sector %d",FL,__func__, adir->start_sector);
		stream_data = OLE_load_chain( ole, (int)adir->start_sector );
		if (stream_data == NULL)
		{
			DOLE LOGGER_log("%s:%d:%s:DEBUG: Terminating from stream data being NULL  ",FL,__func__);
			//OLE_decode_done(ole);
			return OLEER_MINISTREAM_STREAM_READ_FAIL;
		}
		DOLE LOGGER_log("%s:%d:%s:DEBUG: Normal decode START. element name ='%s' stream size = '%ld'",FL,__func__, element_name, adir->stream_size);
		decode_result = OLEUNWRAP_decodestream( &oleuw, element_name, (char *)stream_data, adir->stream_size, unpack_metadata );
		DOLE LOGGER_log("%s:%d:%s:DEBUG: Normal decode done.",FL,__func__);
	} else {

		/** Minichain/Minisector stored stream **/
		DOLE LOGGER_log("%s:%d:%s:DEBUG: Minichain loader, starting at sector %d"
				,FL,__func__
				,adir->start_sector
				);
		stream_data = OLE_load_minichain( ole, adir->start_sector );
		if (stream_data == NULL)
		{
			DOLE LOGGER_log("%s:%d:%s:DEBUG: Ministream was non-existant, terminating",FL,__func__);
			//OLE_decode_done(ole);
			return OLEER_NORMALSTREAM_STREAM_READ_FAIL;
		}
		DOLE LOGGER_log("%s:%d:%s:DEBUG: Mini decode START.",FL,__func__);
		decode_result = OLEUNWRAP_decodestream( &oleuw, element_name, (char *)stream_data, adir->stream_size, unpack_metadata );
		DOLE LOGGER_log("%s:%d:%s:DEBUG: Mini decode done.",FL,__func__);
	}

	if ((stream_data != NULL)&&(decode_result == OLEUW_STREAM_NOT_DECODED)&&(ole->save_unknown_streams))
	{
		char *lfname;

		lfname = PLD_dprintf("ole-stream.%d",adir->start_sector);
		if (lfname != NULL)
		{
			DOLE LOGGER_log("%s:%d:%s:DEBUG: Saving stream to %s",FL,__func__,lfname);
			OLE_store_stream( ole, lfname, unpack_metadata, (char *) stream_data, adir->stream_size );
			free(lfname);
		} 
	} // If we needed to save an unknown stream

	// Clean up an stream_data which we may have 
	// read in from the chain-loader.
	if (stream_data) free(stream_data);

	return result;
}

int OLE_decode( struct OLE_object *ole, RIPMIME_output *unpack_metadata )
{
	unsigned char *current_property, *property_limit;
	int result = 0;
	int i;

	// Try create the output directory which we're using
	//		to write the decoded files out to.
	DOLE LOGGER_log("%s:%d:%s:DEBUG: opening output directory %s", FL,__func__, unpack_metadata->dir);
	result = OLE_open_directory( ole, unpack_metadata);
	if (result != 0) return result;

	// In order to successfully decode an OLE2 stream, we have to read
	//		and understand the first 512 bytes of the file, this is the 
	//		OLE2 header. 
	DOLE LOGGER_log("%s:%d:%s:DEBUG: Getting main header", FL,__func__);
	result = OLE_get_header( ole );
	if (result != 0) return result;

	DOLE LOGGER_log("%s:%d:%s:DEBUG: Converting main header", FL,__func__);
	result = OLE_convert_header( ole );
	if (result != 0) return result;

	result = OLE_header_sanity_check( ole );
	if (result > 0) return OLEER_INSANE_OLE_FILE;

	DOLE OLE_print_header( ole );

	DOLE LOGGER_log("%s:%d:%s:DEBUG: Loading FAT", FL,__func__);
	result = OLE_load_FAT( ole );
	if (result != 0) return result;

	DOLE LOGGER_log("%s:%d:%s:DEBUG: Loading miniFAT chain", FL,__func__);
	ole->miniFAT = OLE_load_chain( ole, ole->header.mini_fat_start );
	if (ole->miniFAT == NULL) return OLEER_MINIFAT_READ_FAIL;

	DOLE LOGGER_log("%s:%d:%s:DEBUG: Loading Directory stream chain", FL,__func__);
	ole->properties = OLE_load_chain( ole, ole->header.directory_stream_start_sector );
	if (ole->properties == NULL) return OLEER_PROPERTIES_READ_FAIL;

	i=0;
	current_property = ole->properties;
	property_limit = current_property +ole->last_chain_size ;
	// while(1)
	while (current_property < property_limit)
	{
		struct OLE_directory_entry a_dir_object, *adir;
		int property_value=0;

		adir = &a_dir_object;

		OLE_dir_init(adir);

		property_value = get_uint8((char *)current_property);
		if (property_value < 1) break;

		DOLE LOGGER_log("%s:%d:%s:DEBUG:--------- DIRECTORY INDEX: %d",FL,__func__,i);

		OLE_convert_directory( ole, current_property, adir );

		DOLE {
			LOGGER_log("%s:%d:%s:DEBUG: Printing directory details...",FL,__func__);
			OLE_print_directory( ole, adir);
			LOGGER_log("%s:%d:%s:DEBUG: End of directory details",FL,__func__);
		}

		if (adir->element_colour > 1) break;

		if ((adir->element_type == STGTY_INVALID)||(adir->element_type > STGTY_ROOT))
		{
			DOLE LOGGER_log("%s:%d:%s:DEBUG: breaking out due to element type %d",FL,__func__, adir->element_type);
			break;
		} else if (adir->element_type == STGTY_ROOT){
			/** ROOT DIRECTORY ENTRY **/
			DOLE LOGGER_log("%s:%d:%s:DEBUG: Loading ministream/SmallBlockArray",FL,__func__);
			ole->ministream = OLE_load_chain( ole, adir->start_sector );
			if (ole->ministream == NULL) return OLEER_MINISTREAM_READ_FAIL;
			DOLE LOGGER_log("%s:%d:%s:DEBUG: ministream done",FL,__func__);
		} else if (adir->element_type == STGTY_STORAGE) {
			/** STORAGE ELEMENT **/
			DOLE LOGGER_log("%s:%d:%s:DEBUG: Item is directory, start child is at index %d\n",FL,__func__,i);
			ole->ministream = OLE_load_chain( ole, adir->start_sector );
			if (ole->ministream == NULL) return OLEER_MINISTREAM_READ_FAIL;
			DOLE LOGGER_log("%s:%d:%s:DEBUG: DIRECTORY ministream done",FL,__func__);
		} else if (adir->element_type == STGTY_STREAM) {
			/** STREAM ELEMENT **/
			OLE_decode_stream( ole, adir, unpack_metadata );
		} else {
			/** If the element isn't of the above types then it's possibly 
			 ** an empty element or just one used for the MSAT/SAT
			 ** either way we just step over it and carry on **/
			DOLE LOGGER_log("%s:%d:%s:DEBUG: Element type %d does not need to be handled",FL,__func__,adir->element_type);
		}
		// Jump to the next property record, which
		//		is always 128 bytes ahead.
		current_property += 128;
		i++;

	} // While there are still more directory entries to read in.

	DOLE LOGGER_log("%s:%d:%s:DEBUG: Finished",FL,__func__);

	/* OLE_decode_done(ole);
	 */
	return OLE_OK;
}

/*-----------------------------------------------------------------\
  Function Name	: OLE_decode_diskfile
  Returns Type	: int
  ----Parameter List
  1. char *fname, 
  2.  char *decode_path , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int OLE_decode_diskfile( struct OLE_object *ole, char *fname, RIPMIME_output *unpack_metadata )
{
	int result = 0;

	// Reject any bad paramters.
	if (ole == NULL) return OLEER_DECODE_NULL_OBJECT;
	if (fname == NULL) return OLEER_DECODE_NULL_FILENAME;
	if (unpack_metadata == NULL || unpack_metadata->dir == NULL) return OLEER_DECODE_NULL_PATH;

	// We need to gain access to the OLE2 data file, without
	//		this pretty much everything is pointless.
	DOLE LOGGER_log("%s:%d:%s:DEBUG: opening %s", FL,__func__, fname );
	result = OLE_open_file( ole, fname );
	if (result != 0) return result;

	result = OLE_input_file_data_ini(ole);
	if (result != 0) return result;
	return OLE_decode( ole, unpack_metadata );
}

int OLE_decode_file( struct OLE_object *ole, FILE *f, RIPMIME_output *unpack_metadata )
{
	int result = 0;

	DOLE LOGGER_log("%s:%d:%s:DEBUG: opening %s", FL,__func__);
	// Reject any bad paramters.
	if (ole == NULL) return OLEER_DECODE_NULL_OBJECT;
	if (f == NULL) return OLEER_DECODE_NULL_FILENAME;
	if (unpack_metadata == NULL || unpack_metadata->dir == NULL) return OLEER_DECODE_NULL_PATH;

	ole->f = f;

	result = OLE_input_file_data_ini(ole);
	if (result != 0) return result;
	return OLE_decode( ole, unpack_metadata );
}
