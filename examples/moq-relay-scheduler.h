#ifndef IMQUIC_MOQ_RELAY_SCHEDULER_H
#define IMQUIC_MOQ_RELAY_SCHEDULER_H

#include <glib.h>
#include "imquic/moq.h"

/* Clear the cache before each selection scan. No object may be enqueued until
 * the scan ends, so all candidates on a connection share its admission result. */
typedef gboolean (*imquic_demo_fetch_admission_reader)(gpointer connection,
	gpointer user_data);
gboolean imquic_demo_fetch_admission_cached(GHashTable *cache,
	gpointer connection, imquic_demo_fetch_admission_reader reader,
	gpointer user_data);

imquic_moq_object *imquic_demo_fetch_cursor_peek(GList **cursor,
	gboolean ascending, const imquic_moq_location *start,
	const imquic_moq_location *end);

int imquic_demo_fetch_candidate_compare(uint8_t subscriber_priority_a,
	const imquic_moq_object *object_a, uint64_t request_id_a,
	uint8_t subscriber_priority_b, const imquic_moq_object *object_b,
	uint64_t request_id_b);

#endif
