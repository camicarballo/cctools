#ifndef DATASWARM_WORKER_H
#define DATASWARM_WORKER_H

#include <time.h>
#include "hash_table.h"
#include "link.h"

struct ds_worker {
	// Network connection to the manager process.
	struct link *manager_link;

	// Table mapping taskids to ds_task objects.
	struct hash_table *task_table;

	// Path to top of workspace containing tasks and blobs.
	char *workspace;

	/***************************************************************/
	/* Internal tuning parameters set in ds_worker_create() */
	/***************************************************************/

	// Give up and reconnect if no message received after this time.
	int idle_timeout;

	// Abort a single message transmission if stuck for this long.
	int long_timeout;

	// Minimum time between connection attempts.
	int min_connect_retry;

	// Maximum time between connection attempts.
	int max_connect_retry;

	// Maximum time to wait for a catalog query
	int catalog_timeout;

	// id msg counter
	int message_id;

	// Time last status update was sent to manager.
	time_t last_status_report;

	// Seconds between updates.
	int status_report_interval;
};

struct ds_worker *ds_worker_create();

void ds_worker_connect_by_name( struct ds_worker *w, const char *manager_name );
void ds_worker_connect_loop( struct ds_worker *w, const char *manager_host, int manager_port );

void ds_worker_delete(struct ds_worker *w);


#endif
