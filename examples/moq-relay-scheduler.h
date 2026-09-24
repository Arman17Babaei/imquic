#ifndef IMQUIC_MOQ_RELAY_SCHEDULER_H
#define IMQUIC_MOQ_RELAY_SCHEDULER_H

#include <glib.h>
#include "imquic/moq.h"

imquic_moq_object *imquic_demo_fetch_cursor_peek(GList **cursor,
	gboolean ascending, const imquic_moq_location *start,
	const imquic_moq_location *end);

int imquic_demo_fetch_candidate_compare(uint8_t subscriber_priority_a,
	const imquic_moq_object *object_a, uint64_t request_id_a,
	uint8_t subscriber_priority_b, const imquic_moq_object *object_b,
	uint64_t request_id_b);

#endif
