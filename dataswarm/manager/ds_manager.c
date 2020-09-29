/*
Copyright (C) 2020- The University of Notre Dame
This software is distributed under the GNU General Public License.
See the file COPYING for details.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <getopt.h>
#include <errno.h>

#include "link.h"
#include "jx.h"
#include "jx_print.h"
#include "jx_parse.h"
#include "debug.h"
#include "stringtools.h"
#include "xxmalloc.h"
#include "cctools.h"
#include "hash_table.h"
#include "username.h"
#include "catalog_query.h"

#include "common/ds_message.h"
#include "common/ds_task.h"
#include "ds_worker_rep.h"
#include "ds_client_rep.h"
#include "ds_blob_rep.h"
#include "ds_task_rep.h"
#include "ds_manager.h"
#include "ds_client_ops.h"
#include "ds_file.h"

#include "ds_test.h"

struct jx * manager_status_jx( struct ds_manager *m )
{
	char owner[USERNAME_MAX];
	username_get(owner);

	struct jx * j = jx_object(0);
	jx_insert_string(j,"type","ds_manager");
	jx_insert_string(j,"project",m->project_name);
	jx_insert_integer(j,"starttime",(m->start_time/1000000));
	jx_insert_string(j,"owner",owner);
	jx_insert_string(j,"version",CCTOOLS_VERSION);
	jx_insert_integer(j,"port",m->server_port);

	return j;
}

void update_catalog( struct ds_manager *m, int force_update )
{
	if(!m->force_update && (time(0) - m->catalog_last_update_time) < m->update_interval)
		return;

	if(!m->catalog_hosts) m->catalog_hosts = strdup(CATALOG_HOST);

	struct jx *j = manager_status_jx(m);
	char *str = jx_print_string(j);

	debug(D_DATASWARM, "advertising to the catalog server(s) at %s ...", m->catalog_hosts);
	catalog_query_send_update_conditional(m->catalog_hosts, str);

	free(str);
	jx_delete(j);
	m->catalog_last_update_time = time(0);
}

void process_files( struct ds_manager *m )
{
}

void process_tasks( struct ds_manager *m )
{
}

/* declares a blob in a worker so that it can be manipulated via blob rpcs. */
struct ds_blob_rep *ds_manager_add_blob_to_worker( struct ds_manager *m, struct ds_worker_rep *r, const char *blobid) {
	struct ds_blob_rep *b = hash_table_lookup(r->blobs, blobid);
	if(b) {
		/* cannot create an already declared blob. This could only happen with
		 * a bug, as we have control of the create messages.*/
		fatal("blob-id %s already created at worker.", blobid);
	}

	b = calloc(1,sizeof(struct ds_blob_rep));
	b->state = DS_BLOB_WORKER_STATE_NEW;
	b->in_transition = b->state;
	b->result = DS_RESULT_SUCCESS;

	b->blobid = xxstrdup(blobid);

	hash_table_insert(r->blobs, blobid, b);

	return b;
}

/* declares a task in a worker so that it can be manipulated via task rpcs. */
struct ds_task_rep *ds_manager_add_task_to_worker( struct ds_manager *m, struct ds_worker_rep *r, const char *taskid) {
	struct ds_task_rep *t = hash_table_lookup(r->tasks, taskid);
	if(t) {
		/* cannot create an already declared task. This could only happen with
		 * a bug, as we have control of the create messages.*/
		fatal("task-id %s already created at worker.", taskid);
	}

	/* this should be a proper struct ds_task. */
	struct jx *description = hash_table_lookup(m->task_table, taskid);
	if(!description) {
		/* could not find task with taskid. This could only happen with a bug,
		 * as we have control of the create messages.*/
		fatal("task-id %s does not exist.", taskid);
	}

	t = calloc(1,sizeof(struct ds_task_rep));
	t->state = DS_TASK_WORKER_STATE_NEW;
	t->in_transition = t->state;
	t->result = DS_RESULT_SUCCESS;

	t->taskid = xxstrdup(taskid);
	t->description = description;

	hash_table_insert(r->tasks, taskid, t);

	return t;
}

char *ds_manager_submit_task( struct ds_manager *m, struct jx *description ) {
	char *taskid = string_format("task-%d", m->task_id++);

	/* do validation */
	/* convert to proper struct ds_task */

	jx_insert_string(description, "task-id", taskid);

	hash_table_insert(m->task_table, taskid, description);

	return taskid;
}


void handle_connect_message( struct ds_manager *m, time_t stoptime )
{
	struct link *l;

	while((l = link_accept(m->manager_link,stoptime))) {
		struct jx *msg = ds_json_recv(l,stoptime);
		if(!msg) {
			link_close(l);
			break;
		}

		char addr[LINK_ADDRESS_MAX];
		int port;
		link_address_remote(l,addr,&port);
		debug(D_DATASWARM,"new connection from %s:%d\n",addr,port);

		const char *method = jx_lookup_string(msg,"method");
		struct jx *params = jx_lookup(msg,"params");

		if(!method || !params) {
			/* ds_json_send_error_result(l, msg, DS_MSG_MALFORMED_MESSAGE, stoptime); */
			link_close(l);
			break;
		}

		if(strcmp(method, "handshake")) {
			/* ds_json_send_error_result(l, msg, DS_MSG_UNEXPECTED_METHOD, stoptime); */
			link_close(l);
			break;
		}

		jx_int_t id = jx_lookup_integer(msg, "id");
		if(id < 1) {
			/* ds_json_send_error_result(l, msg, DS_MSG_MALFORMED_ID, stoptime); */
			link_close(l);
			break;
		}

		char *manager_key = string_format("%p",l);
		//const char *msg_key = jx_lookup_string(msg, "uuid");  /* todo: replace manager_key when msg_key not null */
		const char *conn_type = jx_lookup_string(params, "type");

		struct jx *response;
		if(!strcmp(conn_type,"worker")) {
			debug(D_DATASWARM,"new worker from %s:%d\n",addr,port);
			struct ds_worker_rep *w = ds_worker_rep_create(l);
			hash_table_insert(m->worker_table,manager_key,w);
			response = ds_message_standard_response(id, DS_RESULT_SUCCESS, NULL);
			// XXX This is a HACK to get some messages going for testing
			dataswarm_test_script(m,w);

		} else if(!strcmp(conn_type,"client")) {
			debug(D_DATASWARM,"new client from %s:%d\n",addr,port);
			struct ds_client_rep *c = ds_client_rep_create(l);
			hash_table_insert(m->client_table,manager_key,c);
			response = ds_message_standard_response(id,DS_RESULT_SUCCESS,NULL);
		} else {
			/* ds_json_send_error_result(l, {"result": ["params.type"] }, DS_MSG_MALFORMED_PARAMETERS, stoptime); */
			link_close(l);
			break;
		}

		if(response) {
			/* this response probably shouldn't be here */
			ds_json_send(l, response, time(0) + m->stall_timeout);
			jx_delete(response);
		}

		free(manager_key);
		jx_delete(msg);

	}
}

void handle_client_message( struct ds_manager *m, struct ds_client_rep *c, time_t stoptime )
{
	struct jx *msg = ds_json_recv(c->link,stoptime);
	if(!msg) {
		// handle disconnected client
		return;
	}

	const char *method = jx_lookup_string(msg,"method");
	struct jx *params = jx_lookup(msg,"params");
	if(!method || !params) {
		/* ds_json_send_error_result(l, msg, DS_MSG_MALFORMED_MESSAGE, stoptime); */
		/* should we disconnect the client on a message error? */
		return;
	}

	if(!strcmp(method,"task-submit")) {
		/* dataswarm_submit_task(); */
	} else if(!strcmp(method,"task-delete")) {
		/* dataswarm_delete_task(); */
	} else if(!strcmp(method,"task-retrieve")) {
		/* dataswarm_retrieve_task(); */
	} else if(!strcmp(method,"file-submit")) {
		//dataswarm_declare_file();
	} else if(!strcmp(method,"file-commit")) {
		//dataswarm_commit_file();
	} else if(!strcmp(method,"file-delete")) {
		//dataswarm_delete_file();
	} else if(!strcmp(method,"file-copy")) {
		//dataswarm_copy_file();
	} else if(!strcmp(method,"service-submit")) {
		/* dataswarm_submit_service(); */
	} else if(!strcmp(method,"service-delete")) {
		/* dataswarm_delete_service(); */
	} else if(!strcmp(method,"project-create")) {
		/* dataswarm_create_project(); */
	} else if(!strcmp(method,"project-delete")) {
		/* dataswarm_delete_project(); */
	} else if(!strcmp(method,"wait")) {
		/* dataswarm_wait(); */
	} else if(!strcmp(method,"queue-empty")) {
		/* dataswarm_queue_empty(); */
	} else if(!strcmp(method,"status")) {
		/* dataswarm_status(); */
	} else {
		/* ds_json_send_error_result(l, msg, DS_MSG_UNEXPECTED_METHOD, stoptime); */
	}
}

void handle_worker_message( struct ds_manager *m, struct ds_worker_rep *w, time_t stoptime )
{
	struct jx *msg = ds_json_recv(w->link,stoptime);
	if(!msg) {
		// handle disconnected client
		return;
	}
	const char *method = jx_lookup_string(msg,"method");
	const char *params = jx_lookup_string(msg,"params");
	if(!method || !params) {
		/* ds_json_send_error_result(l, msg, DS_MSG_MALFORMED_MESSAGE, stoptime); */
		/* disconnect worker */
		return;
	}

	char addr[LINK_ADDRESS_MAX];
	int port;
	link_address_remote(w->link, addr, &port);
	debug(D_DATASWARM, "worker %s:%d rx: %s", w->addr, w->port, method);


	if(!strcmp(method,"task-change")) {
		/* */
	} else if(!strcmp(method,"blob-change")) {
		/* */
	} else if(!strcmp(method,"status-report")) {
		/* */
	} else {
		/* ds_json_send_error_result(l, msg, DS_MSG_UNEXPECTED_METHOD, stoptime); */
	}

}

int handle_messages( struct ds_manager *m, int msec )
{
	int n = hash_table_size(m->client_table) + hash_table_size(m->worker_table) + 1;

	struct link_info *table = malloc(sizeof(struct link_info)*(n+1));

	table[0].link = m->manager_link;
	table[0].events = LINK_READ;
	table[0].revents = 0;

	char *key;
	struct ds_worker_rep *w;
	struct ds_client_rep *c;

	n = 1;

	hash_table_firstkey(m->client_table);
	while(hash_table_nextkey(m->client_table, &key, (void **) &c)) {
		table[n].link = c->link;
		table[n].events = LINK_READ;
		table[n].revents = 0;
		n++;
	}

	hash_table_firstkey(m->worker_table);
	while(hash_table_nextkey(m->worker_table, &key, (void **) &w)) {
		table[n].link = w->link;
		table[n].events = LINK_READ;
		table[n].revents = 0;
		n++;
	}

	link_poll(table,n,msec);

	int i;
	for(i=0;i<n;i++) {
		if(table[i].revents&LINK_READ) {

			char *key = string_format("%p",table[i].link);

			if(i==0) {
				handle_connect_message(m,time(0)+m->connect_timeout);
			} else if((c=hash_table_lookup(m->client_table,key))) {
				handle_client_message(m,c,time(0)+m->stall_timeout);
			} else if((w==hash_table_lookup(m->worker_table,key))) {
				handle_worker_message(m,w,time(0)+m->stall_timeout);
			}

			free(key);

		}
	}

	free(table);

	return n;
}

void server_main_loop( struct ds_manager *m )
{
	while(1) {
		update_catalog(m,0);
		handle_messages(m,100);
		process_files(m);
		process_tasks(m);
	}
}

static const struct option long_options[] =
{
	{"name", required_argument, 0, 'N'},
	{"port", required_argument, 0, 'p'},
	{"debug", required_argument, 0, 'd'},
	{"debug-file", required_argument, 0, 'o'},
	{"help", no_argument, 0, 'h' },
	{"version", no_argument, 0, 'v' },
	{0, 0, 0, 0}
};

static void show_help( const char *cmd )
{
	printf("use: %s [options]\n",cmd);
	printf("where options are:\n");
	printf("-N --name=<name>          Set project name for catalog update.\n");
	printf("-p,--port=<port>          Port number to listen on.\n");
	printf("-d,--debug=<subsys>       Enable debugging for this subsystem.\n");
	printf("-o,--debug-file=<file>    Send debugging output to this file.\n");
	printf("-h,--help                 Show this help string\n");
	printf("-v,--version              Show version string\n");
}

int main(int argc, char *argv[])
{
	struct ds_manager * m = ds_manager_create();

	int c;
	while((c = getopt_long(argc, argv, "p:N:s:d:o:hv", long_options, 0))!=-1) {

		switch(c) {
			case 'N':
				m->project_name = optarg;
				break;
			case 'd':
				debug_flags_set(optarg);
				break;
			case 'o':
				debug_config_file(optarg);
				break;
			case 'p':
				m->server_port = atoi(optarg);
				break;
			case 'v':
				cctools_version_print(stdout, argv[0]);
				return 0;
				break;
			default:
			case 'h':
				show_help(argv[0]);
				return 0;
				break;
		}
	}

	m->manager_link = link_serve(m->server_port);
	if(!m->manager_link) {
		printf("could not serve on port %d: %s\n", m->server_port,strerror(errno));
		return 1;
	}

	char addr[LINK_ADDRESS_MAX];
	link_address_local(m->manager_link,addr,&m->server_port);
	debug(D_DATASWARM,"listening on port %d...\n",m->server_port);

	server_main_loop(m);

	debug(D_DATASWARM,"server shutting down.\n");

	return 0;
}

struct ds_manager * ds_manager_create()
{
	struct ds_manager *m = malloc(sizeof(*m));

	memset(m,0,sizeof(*m));

	m->worker_table = hash_table_create(0,0);
	m->client_table = hash_table_create(0,0);
	m->task_table   = hash_table_create(0,0);

    m->task_table = hash_table_create(0,0);
    m->file_table = hash_table_create(0,0);

	m->connect_timeout = 5;
	m->stall_timeout = 30;
	m->update_interval = 60;
	m->message_id = 1000;
	m->project_name = "dataswarm";

	return m;
}


/* vim: set noexpandtab tabstop=4: */
