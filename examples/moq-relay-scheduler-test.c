#include "moq-relay-scheduler.h"

#include <stdio.h>

static int cursor_test(void) {
	imquic_moq_object objects[] = {
		{ .group_id = 0, .object_id = 0 },
		{ .group_id = 1, .object_id = 0 },
		{ .group_id = 1, .object_id = 1 },
		{ .group_id = 1, .object_id = 2 },
		{ .group_id = 2, .object_id = 0 },
	};
	GList *cache = NULL;
	for(size_t i = 0; i < G_N_ELEMENTS(objects); i++)
		cache = g_list_prepend(cache, &objects[i]);
	imquic_moq_location start = { .group = 1, .object = 0 };
	imquic_moq_location end = { .group = 1, .object = 2 };
	GList *cursor = g_list_last(cache);
	for(uint64_t expected = 0; expected <= 2; expected++) {
		imquic_moq_object *object = imquic_demo_fetch_cursor_peek(
			&cursor, TRUE, &start, &end);
		if(object == NULL || object->group_id != 1 || object->object_id != expected)
			return -1;
		cursor = cursor->prev;
	}
	if(imquic_demo_fetch_cursor_peek(&cursor, TRUE, &start, &end) != NULL)
		return -1;
	cursor = cache;
	for(uint64_t expected = 2; expected != UINT64_MAX; expected--) {
		imquic_moq_object *object = imquic_demo_fetch_cursor_peek(
			&cursor, FALSE, &start, &end);
		if(object == NULL || object->group_id != 1 || object->object_id != expected)
			return -1;
		cursor = cursor->next;
	}
	if(imquic_demo_fetch_cursor_peek(&cursor, FALSE, &start, &end) != NULL)
		return -1;
	/* An end object of zero keeps the historical meaning: the end group is
	 * bounded, but every object in that group remains eligible. */
	end.object = 0;
	cursor = cache;
	for(uint64_t expected = 2; expected != UINT64_MAX; expected--) {
		imquic_moq_object *object = imquic_demo_fetch_cursor_peek(
			&cursor, FALSE, &start, &end);
		if(object == NULL || object->object_id != expected)
			return -1;
		cursor = cursor->next;
	}
	start.group = 7;
	end.group = 8;
	cursor = cache;
	if(imquic_demo_fetch_cursor_peek(&cursor, FALSE, &start, &end) != NULL ||
			cursor != NULL)
		return -1;
	cursor = NULL;
	if(imquic_demo_fetch_cursor_peek(&cursor, TRUE, &start, &end) != NULL)
		return -1;
	g_list_free(cache);
	return 0;
}

static int priority_test(void) {
	imquic_moq_object urgent = { .priority = 4 };
	imquic_moq_object normal = { .priority = 8 };
	if(imquic_demo_fetch_candidate_compare(10, &normal, 4,
			20, &urgent, 1) >= 0)
		return -1;
	if(imquic_demo_fetch_candidate_compare(10, &urgent, 4,
			10, &normal, 1) >= 0)
		return -1;
	if(imquic_demo_fetch_candidate_compare(10, &urgent, 1,
			10, &urgent, 4) >= 0)
		return -1;
	if(imquic_demo_fetch_candidate_compare(10, &urgent, 4,
			10, &urgent, 4) != 0)
		return -1;
	return 0;
}

typedef struct admission_fixture {
	guint calls;
	gboolean admitted;
} admission_fixture;

static gboolean admission_read(gpointer connection, gpointer user_data) {
	(void)user_data;
	admission_fixture *fixture = connection;
	fixture->calls++;
	return fixture->admitted;
}

static int admission_test(void) {
	GHashTable *cache = g_hash_table_new(g_direct_hash, g_direct_equal);
	admission_fixture available = { .admitted = TRUE };
	admission_fixture blocked = { .admitted = FALSE };
	/* Interleaved FETCHes must neither reread the same connection nor share
	 * admission between different connections. */
	for(guint i = 0; i < 1484; i++) {
		g_assert_true(imquic_demo_fetch_admission_cached(cache, &available,
			admission_read, NULL));
		g_assert_false(imquic_demo_fetch_admission_cached(cache, &blocked,
			admission_read, NULL));
	}
	g_assert_cmpuint(available.calls, ==, 1);
	g_assert_cmpuint(blocked.calls, ==, 1);
	/* A new scan must observe both a drained and a newly full connection. */
	g_hash_table_remove_all(cache);
	available.admitted = FALSE;
	blocked.admitted = TRUE;
	g_assert_false(imquic_demo_fetch_admission_cached(cache, &available,
		admission_read, NULL));
	g_assert_true(imquic_demo_fetch_admission_cached(cache, &blocked,
		admission_read, NULL));
	g_assert_cmpuint(available.calls, ==, 2);
	g_assert_cmpuint(blocked.calls, ==, 2);
	g_hash_table_unref(cache);
	return 0;
}

int main(void) {
	if(cursor_test() != 0 || priority_test() != 0 || admission_test() != 0) {
		fprintf(stderr, "MoQ relay scheduler test failed\n");
		return 1;
	}
	puts("MoQ relay scheduler test: PASS");
	return 0;
}
