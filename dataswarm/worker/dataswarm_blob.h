#ifndef DS_BLOB_H
#define DS_BLOB_H

#include "jx.h"

typedef enum {
	DS_BLOB_RW,
	DS_BLOB_RO,
	DS_BLOB_DELETING,
	DS_BLOB_DELETED
} dataswarm_blob_state_t;

struct dataswarm_blob {
	char *blobid;
	dataswarm_blob_state_t state;
	int64_t size;
	struct jx *meta;
};

struct dataswarm_blob * dataswarm_blob_create( const char *blobid, jx_int_t size, struct jx *meta);
struct dataswarm_blob * dataswarm_blob_create_from_jx( struct jx *jblob );
struct dataswarm_blob * dataswarm_blob_create_from_file( const char *filename );

struct jx * dataswarm_blob_to_jx( struct dataswarm_blob *b );
int dataswarm_blob_to_file( struct dataswarm_blob *b, const char *filename );

void dataswarm_blob_delete( struct dataswarm_blob *b );

#endif
