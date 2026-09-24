#include "moq-relay-scheduler.h"

static gboolean imquic_demo_fetch_object_in_range(
		const imquic_moq_object *object,
		const imquic_moq_location *start,
		const imquic_moq_location *end) {
	if(object == NULL || start == NULL || end == NULL ||
			object->group_id < start->group || object->group_id > end->group)
		return FALSE;
	if(object->group_id == start->group && object->object_id < start->object)
		return FALSE;
	if(object->group_id == end->group && end->object > 0 &&
			object->object_id > end->object)
		return FALSE;
	return TRUE;
}

imquic_moq_object *imquic_demo_fetch_cursor_peek(GList **cursor,
		gboolean ascending, const imquic_moq_location *start,
		const imquic_moq_location *end) {
	if(cursor == NULL)
		return NULL;
	while(*cursor != NULL) {
		imquic_moq_object *object = (imquic_moq_object *)(*cursor)->data;
		if(imquic_demo_fetch_object_in_range(object, start, end))
			return object;
		*cursor = ascending ? (*cursor)->prev : (*cursor)->next;
	}
	return NULL;
}

int imquic_demo_fetch_candidate_compare(uint8_t subscriber_priority_a,
		const imquic_moq_object *object_a, uint64_t request_id_a,
		uint8_t subscriber_priority_b, const imquic_moq_object *object_b,
		uint64_t request_id_b) {
	if(subscriber_priority_a < subscriber_priority_b)
		return -1;
	if(subscriber_priority_a > subscriber_priority_b)
		return 1;
	if(object_a == NULL)
		return object_b == NULL ? 0 : 1;
	if(object_b == NULL)
		return -1;
	if(object_a->priority < object_b->priority)
		return -1;
	if(object_a->priority > object_b->priority)
		return 1;
	if(request_id_a < request_id_b)
		return -1;
	if(request_id_a > request_id_b)
		return 1;
	return 0;
}
