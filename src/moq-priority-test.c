#include "imquic/imquic.h"
#include "imquic/moq.h"
#include "internal/moq.h"
#include "internal/connection.h"
#include "internal/network.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TEST_CERT_PATH "../../picoquic/certs/cert.pem"
#define TEST_KEY_PATH "../../picoquic/certs/key.pem"
#define TEST_TIMEOUT_STEPS 1000

static const uint8_t namespace_bytes[] = "priority-test";
static const uint8_t track_bytes[] = "track";
static const uint8_t first_payload[] = "priority-";
static const uint8_t second_payload[] = "payload";
static imquic_moq_namespace test_namespace = {
	.buffer = (uint8_t *)namespace_bytes,
	.length = sizeof(namespace_bytes) - 1,
};
static imquic_moq_track test_track = {
	.buffer = (uint8_t *)track_bytes,
	.length = sizeof(track_bytes) - 1,
};

static volatile gint test_done;
static volatile gint test_failed;
static volatile gint updates_received;
static volatile gint updates_accepted;
static volatile gint object_received;
static volatile gint non_fetch_rejected;
static uint64_t fetch_request_id = UINT64_MAX;

static void fail_test(const char *message) {
	if(g_atomic_int_compare_and_exchange(&test_failed, 0, 1))
		fprintf(stderr, "moq-priority-test: %s\n", message);
	g_atomic_int_set(&test_done, 1);
}

static void maybe_complete(void) {
	if(g_atomic_int_get(&object_received) == 2 &&
			g_atomic_int_get(&updates_received) == 2 &&
			g_atomic_int_get(&updates_accepted) == 2 &&
			g_atomic_int_get(&non_fetch_rejected))
		g_atomic_int_set(&test_done, 1);
}

static void connection_failed(void *user_data) {
	(void)user_data;
	fail_test("connection failed");
}

static void incoming_subscribe(imquic_connection *conn, uint64_t request_id,
		imquic_moq_namespace *tns, imquic_moq_track *tn,
		imquic_moq_request_parameters *parameters) {
	(void)tns;
	(void)tn;
	(void)parameters;
	if(imquic_moq_set_fetch_priority(conn, request_id, 1) != -1) {
		fail_test("non-FETCH request accepted a FETCH priority");
		return;
	}
	g_atomic_int_set(&non_fetch_rejected, 1);
	imquic_moq_reject_subscribe(conn, request_id,
		IMQUIC_MOQ_REQERR_DOES_NOT_EXIST, "test-only subscription", 0, NULL);
	maybe_complete();
}

static int send_final_object(imquic_connection *conn, uint64_t request_id) {
	imquic_moq_object object = {
		.request_id = request_id,
		.group_id = 0,
		.subgroup_id = 0,
		.object_id = 1,
		.priority = 64,
		.payload = (uint8_t *)second_payload,
		.payload_len = sizeof(second_payload) - 1,
		.delivery = IMQUIC_MOQ_USE_FETCH,
		.end_of_stream = TRUE,
	};
	return imquic_moq_send_object(conn, &object);
}

static void request_updated(imquic_connection *conn, uint64_t request_id,
		uint64_t sub_request_id, imquic_moq_request_parameters *parameters) {
	int update_index = g_atomic_int_add(&updates_received, 1);
	const uint8_t expected[] = { 30, 10 };
	int priority_result = parameters != NULL && parameters->subscriber_priority_set ?
		imquic_moq_set_fetch_priority(conn, sub_request_id,
			parameters->subscriber_priority) : -1;
	if(sub_request_id != fetch_request_id || update_index >= 2 ||
			parameters == NULL || !parameters->subscriber_priority_set ||
			parameters->subscriber_priority != expected[update_index] ||
			priority_result != 0) {
		fprintf(stderr, "moq-priority-test: update=%d sub=%" G_GUINT64_FORMAT
			" fetch=%" G_GUINT64_FORMAT " priority=%u result=%d\n",
			update_index, sub_request_id, fetch_request_id,
			parameters && parameters->subscriber_priority_set ?
			parameters->subscriber_priority : 255, priority_result);
		fail_test("REQUEST_UPDATE did not reprioritize the FETCH");
		return;
	}
	imquic_moq_request_parameters response;
	imquic_moq_request_parameters_init_defaults(&response);
	response.subscriber_priority_set = TRUE;
	response.subscriber_priority = parameters->subscriber_priority;
	if(imquic_moq_accept_request_update(conn, request_id, &response) != 0)
		fail_test("failed to accept REQUEST_UPDATE");
	else if(update_index == 1 && send_final_object(conn, sub_request_id) != 0)
		fail_test("failed to complete reprioritized FETCH");
	maybe_complete();
}

static void incoming_fetch(imquic_connection *conn, uint64_t request_id,
		imquic_moq_namespace *tns, imquic_moq_track *tn,
		imquic_moq_location_range *range,
		imquic_moq_request_parameters *parameters) {
	if(!imquic_moq_namespace_equals(tns, &test_namespace) ||
			tn->length != test_track.length ||
			memcmp(tn->buffer, test_track.buffer, test_track.length) != 0 ||
			range->start.group != 0 || range->start.object != 0 ||
			range->end.group != 0 || range->end.object != 0 ||
			parameters == NULL || !parameters->subscriber_priority_set ||
			parameters->subscriber_priority != 100) {
		fail_test("initial FETCH priority or range was not preserved");
		return;
	}
	fetch_request_id = request_id;
	if(imquic_moq_set_fetch_priority(conn, request_id, 80) != 0 ||
			imquic_moq_set_fetch_priority(conn, UINT64_MAX, 1) != -1) {
		fail_test("pre-stream FETCH priority validation failed");
		return;
	}
	imquic_moq_location largest = { .group = 0, .object = 0 };
	imquic_moq_request_parameters response;
	imquic_moq_request_parameters_init_defaults(&response);
	if(imquic_moq_accept_fetch(conn, request_id, &largest, &response, NULL) != 0) {
		fail_test("failed to accept FETCH");
		return;
	}
	imquic_moq_object object = {
		.request_id = request_id,
		.group_id = 0,
		.subgroup_id = 0,
		.object_id = 0,
		.priority = 64,
		.payload = (uint8_t *)first_payload,
		.payload_len = sizeof(first_payload) - 1,
		.delivery = IMQUIC_MOQ_USE_FETCH,
		.end_of_stream = FALSE,
	};
	if(imquic_moq_send_object(conn, &object) != 0 ||
			imquic_moq_set_fetch_priority(conn, request_id, 40) != 0) {
		fail_test("post-stream FETCH reprioritization failed");
		return;
	}
}

static void client_ready(imquic_connection *conn) {
	imquic_moq_request_parameters parameters;
	imquic_moq_request_parameters_init_defaults(&parameters);
	uint64_t subscribe_id = imquic_moq_get_next_request_id(conn);
	if(imquic_moq_subscribe(conn, subscribe_id, &test_namespace,
			&test_track, &parameters) != 0) {
		fail_test("failed to issue control SUBSCRIBE");
		return;
	}
	imquic_moq_location_range range = {
		.start = { .group = 0, .object = 0 },
		.end = { .group = 0, .object = 0 },
	};
	parameters.subscriber_priority_set = TRUE;
	parameters.subscriber_priority = 100;
	uint64_t request_id = imquic_moq_get_next_request_id(conn);
	if(imquic_moq_standalone_fetch(conn, request_id, &test_namespace,
			&test_track, &range, &parameters) != 0)
		fail_test("failed to issue FETCH");
}

static void fetch_accepted(imquic_connection *conn, uint64_t request_id,
		imquic_moq_location *largest, imquic_moq_request_parameters *parameters,
		GList *track_properties) {
	(void)largest;
	(void)parameters;
	(void)track_properties;
	imquic_moq_request_parameters update;
	imquic_moq_request_parameters_init_defaults(&update);
	update.subscriber_priority_set = TRUE;
	update.subscriber_priority = 30;
	uint64_t update_id = imquic_moq_get_next_request_id(conn);
	if(imquic_moq_update_request(conn, update_id, request_id, &update) != 0)
		fail_test("failed to issue first REQUEST_UPDATE");
}

static void update_accepted(imquic_connection *conn, uint64_t request_id,
		imquic_moq_request_parameters *parameters) {
	(void)conn;
	(void)request_id;
	if(parameters == NULL || !parameters->subscriber_priority_set) {
		fail_test("REQUEST_UPDATE acknowledgement omitted priority");
		return;
	}
	int accepted = g_atomic_int_add(&updates_accepted, 1) + 1;
	if(accepted == 1) {
		imquic_moq_request_parameters update;
		imquic_moq_request_parameters_init_defaults(&update);
		update.subscriber_priority_set = TRUE;
		update.subscriber_priority = 10;
		uint64_t update_id = imquic_moq_get_next_request_id(conn);
		if(imquic_moq_update_request(conn, update_id, fetch_request_id, &update) != 0) {
			fail_test("failed to issue second REQUEST_UPDATE");
			return;
		}
	}
	maybe_complete();
}

static void subscribe_error(imquic_connection *conn, uint64_t request_id,
		imquic_moq_request_error_code error_code, const char *reason,
		uint64_t retry_interval, imquic_moq_redirect *redirect) {
	(void)conn;
	(void)request_id;
	(void)error_code;
	(void)reason;
	(void)retry_interval;
	(void)redirect;
	maybe_complete();
}

static void incoming_object(imquic_connection *conn, imquic_moq_object *object) {
	(void)conn;
	const uint8_t *expected = object->object_id == 0 ? first_payload : second_payload;
	size_t expected_length = object->object_id == 0 ?
		sizeof(first_payload) - 1 : sizeof(second_payload) - 1;
	if(object->group_id != 0 || object->object_id > 1 ||
			object->payload_len != expected_length ||
			memcmp(object->payload, expected, expected_length) != 0) {
		fail_test("FETCH payload validation failed");
		return;
	}
	g_atomic_int_inc(&object_received);
	maybe_complete();
}

static int buffered_objects, buffered_terminals;
static void buffered_object(imquic_connection *conn, imquic_moq_object *object) {
	(void)conn;
	if(object->object_id != (uint64_t)buffered_objects || object->payload_len != 1 || object->payload[0] != 42)
		buffered_objects = -100;
	buffered_objects++;
	if(object->end_of_stream) buffered_terminals++;
}

/* A final QUIC read can coalesce more than one complete MoQ object. */
static int test_coalesced_fin(void) {
	imquic_network_endpoint endpoint = { 0 };
	endpoint.callbacks.moq.incoming_object = buffered_object;
	imquic_connection connection = { .name = "coalesced-fin", .socket = &endpoint };
	imquic_moq_context moq = { .conn = &connection, .version = IMQUIC_MOQ_VERSION_19 };
	for(int fetch = 0; fetch < 2; fetch++) {
		uint8_t bytes[256], payload = 42, error = 0;
		size_t used = 0;
		imquic_moq_stream *stream = imquic_moq_stream_create();
		stream->type = fetch ? IMQUIC_MOQ_FETCH_HEADER : IMQUIC_MOQ_SUBGROUP_HEADER;
		stream->ascending = TRUE;
		for(int index = 0; index < 2; index++) {
			size_t encoded;
			if(fetch) {
				uint64_t flags = imquic_moq_generate_fetch_serialization_flags(moq.version,
					IMQUIC_MOQ_FETCH_SUBGROUP_ID, TRUE, TRUE, TRUE, FALSE, FALSE, FALSE, FALSE);
				encoded = imquic_moq_add_fetch_header_object(&moq, bytes + used, sizeof(bytes) - used,
					flags, 0, 0, index, 64, IMQUIC_MOQ_NORMAL_OBJECT, NULL, 0, &payload, 1, NULL, 0);
			} else {
				stream->got_objects = index > 0; stream->last_object_id = 0;
				encoded = imquic_moq_add_subgroup_header_object(&moq, stream, bytes + used, sizeof(bytes) - used,
					0, IMQUIC_MOQ_NORMAL_OBJECT, NULL, 0, &payload, 1, NULL, 0);
			}
			if(!encoded) return -1;
			used += encoded;
		}
		stream->got_objects = FALSE;
		stream->buffer = imquic_buffer_create(bytes, used);
		buffered_objects = 0; buffered_terminals = 0;
		for(int index = 0; index < 2; index++) {
			int parsed = fetch ? imquic_moq_parse_fetch_header_object(&moq, stream, TRUE, &error) :
				imquic_moq_parse_subgroup_header_object(&moq, stream, TRUE, &error);
			if(parsed != 0 || buffered_objects != index + 1 || buffered_terminals != index || stream->closed != (index == 1)) {
				fprintf(stderr, "coalesced FIN regression: fetch=%d index=%d parsed=%d objects=%d terminals=%d closed=%d\n",
					fetch, index, parsed, buffered_objects, buffered_terminals, stream->closed);
				return -1;
			}
		}
		imquic_moq_stream_destroy(stream);
	}
	return 0;
}

int main(void) {
	imquic_server *server = NULL;
	imquic_client *client = NULL;
	int ret = 1;
	if(imquic_init(NULL) != 0)
		return 1;
	imquic_set_log_level(IMQUIC_LOG_WARN);
	if(test_coalesced_fin() != 0) goto done;
	server = imquic_create_moq_server("moq-priority-server",
		IMQUIC_CONFIG_INIT,
		IMQUIC_CONFIG_LOCAL_BIND, "127.0.0.1",
		IMQUIC_CONFIG_LOCAL_PORT, 0,
		IMQUIC_CONFIG_TLS_CERT, TEST_CERT_PATH,
		IMQUIC_CONFIG_TLS_KEY, TEST_KEY_PATH,
		IMQUIC_CONFIG_RAW_QUIC, TRUE,
		IMQUIC_CONFIG_MOQ_VERSION, IMQUIC_MOQ_VERSION_ANY,
		IMQUIC_CONFIG_MAX_BIDI_STREAMS, 16,
		IMQUIC_CONFIG_MAX_UNI_STREAMS, 16,
		IMQUIC_CONFIG_MOQ_MAX_REQUEST_UPDATES, 4,
		IMQUIC_CONFIG_DONE, NULL);
	if(server == NULL)
		goto done;
	imquic_set_incoming_subscribe_cb(server, incoming_subscribe);
	imquic_set_incoming_standalone_fetch_cb(server, incoming_fetch);
	imquic_set_request_updated_cb(server, request_updated);
	imquic_set_connection_failed_cb(server, connection_failed);
	imquic_start_endpoint(server);

	client = imquic_create_moq_client("moq-priority-client",
		IMQUIC_CONFIG_INIT,
		IMQUIC_CONFIG_LOCAL_BIND, "127.0.0.1",
		IMQUIC_CONFIG_LOCAL_PORT, 0,
		IMQUIC_CONFIG_REMOTE_HOST, "127.0.0.1",
		IMQUIC_CONFIG_REMOTE_PORT, imquic_get_endpoint_port(server),
		IMQUIC_CONFIG_SNI, "localhost",
		IMQUIC_CONFIG_TLS_NO_VERIFY, TRUE,
		IMQUIC_CONFIG_RAW_QUIC, TRUE,
		IMQUIC_CONFIG_MOQ_VERSION, IMQUIC_MOQ_VERSION_ANY,
		IMQUIC_CONFIG_MAX_BIDI_STREAMS, 16,
		IMQUIC_CONFIG_MAX_UNI_STREAMS, 16,
		IMQUIC_CONFIG_MOQ_MAX_REQUEST_UPDATES, 4,
		IMQUIC_CONFIG_DONE, NULL);
	if(client == NULL)
		goto done;
	imquic_set_moq_ready_cb(client, client_ready);
	imquic_set_fetch_accepted_cb(client, fetch_accepted);
	imquic_set_request_update_accepted_cb(client, update_accepted);
	imquic_set_subscribe_error_cb(client, subscribe_error);
	imquic_set_incoming_object_cb(client, incoming_object);
	imquic_set_connection_failed_cb(client, connection_failed);
	imquic_start_endpoint(client);
	for(int step = 0; step < TEST_TIMEOUT_STEPS &&
			!g_atomic_int_get(&test_done); step++)
		g_usleep(10000);
	if(!g_atomic_int_get(&test_done))
		fail_test("timed out");
	if(!g_atomic_int_get(&test_failed) &&
			g_atomic_int_get(&updates_received) == 2 &&
			g_atomic_int_get(&updates_accepted) == 2 &&
			g_atomic_int_get(&object_received) == 2 &&
			g_atomic_int_get(&non_fetch_rejected)) {
		puts("MOQT priority test: PASS");
		ret = 0;
	}

done:
	if(client != NULL)
		imquic_shutdown_endpoint(client);
	if(server != NULL)
		imquic_shutdown_endpoint(server);
	imquic_deinit();
	return ret;
}
