#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include "internal/timer.h"

static gboolean fired(gpointer data) {
	(*(unsigned *)data)++;
	return G_SOURCE_REMOVE;
}

typedef struct busy_test {
	GMainContext *context;
	imquic_wake_timer timer;
	unsigned fired, receives;
} busy_test;

static gboolean busy_fired(gpointer data) {
	busy_test *test = data;
	imquic_wake_timer_dispatched(&test->timer);
	test->fired++;
	return G_SOURCE_REMOVE;
}

static gboolean busy_receive(gpointer data) {
	busy_test *test = data;
	test->receives++;
	imquic_wake_timer_schedule(&test->timer, test->context, 0, busy_fired, test, NULL);
	return G_SOURCE_CONTINUE;
}

static void busy_receive_test(void) {
	busy_test test = { .context = g_main_context_new() };
	GSource *receive = g_idle_source_new();
	g_source_set_priority(receive, G_PRIORITY_DEFAULT);
	g_source_set_callback(receive, busy_receive, &test, NULL);
	g_source_attach(receive, test.context);
	/* A continuously ready receive source runs before the pacing source and
	 * repeatedly requests a wake. It must not starve an already due send. */
	for(unsigned i = 0; i < 10000 && !test.fired; i++)
		g_main_context_iteration(test.context, FALSE);
	assert(test.receives > 0 && test.fired == 1);
	g_source_destroy(receive);
	g_source_unref(receive);
	imquic_wake_timer_cancel(&test.timer);
	assert(test.timer.source == NULL);
	/* A new earlier deadline replaces a future wake, while a later deadline
	 * retains it. Cancellation releases the owned source and its timerfd. */
	imquic_wake_timer_schedule(&test.timer, test.context, 5000000, busy_fired, &test, NULL);
	GSource *future = test.timer.source;
	assert(!imquic_wake_timer_schedule(&test.timer, test.context, 6000000, busy_fired, &test, NULL));
	assert(test.timer.source == future);
	g_source_ref(future);
	assert(imquic_wake_timer_schedule(&test.timer, test.context, 0, busy_fired, &test, NULL));
	assert(g_source_is_destroyed(future));
	g_source_unref(future);
	while(test.fired < 2) g_main_context_iteration(test.context, TRUE);
	assert(test.timer.source == NULL);
	g_main_context_unref(test.context);
}

int main(void) {
	busy_receive_test();
	GMainContext *context = g_main_context_new();
	unsigned count = 0;
	/* A future deadline is armed, rather than immediately pollable. */
	GSource *later = imquic_timer_source_new_us(5000000);
	g_source_set_callback(later, fired, &count, NULL);
	g_source_attach(later, context);
	assert(!g_main_context_iteration(context, FALSE));
	assert(count == 0);
#ifdef __linux__
	int fd = ((imquic_precision_timer *)later)->fd;
	struct itimerspec remaining;
	assert(timerfd_gettime(fd, &remaining) == 0);
	assert(remaining.it_value.tv_sec >= 0);
	assert(remaining.it_value.tv_nsec > 0);
	assert(fcntl(fd, F_GETFD) & FD_CLOEXEC);
	assert(fcntl(fd, F_GETFL) & O_NONBLOCK);
#endif
	g_source_destroy(later);
	g_source_unref(later);
#ifdef __linux__
	assert(fcntl(fd, F_GETFD) == -1 && errno == EBADF);
#endif
	/* Sub-millisecond and immediately due timers fire once and are removed. */
	const guint64 delays[] = { 400, 0 };
	for(unsigned i = 0; i < G_N_ELEMENTS(delays); i++) {
		GSource *timer = imquic_timer_source_new_us(delays[i]);
		g_source_set_callback(timer, fired, &count, NULL);
		g_source_attach(timer, context);
		while(count < i + 1)
			g_main_context_iteration(context, TRUE);
		assert(g_source_is_destroyed(timer));
		g_source_unref(timer);
		assert(!g_main_context_iteration(context, FALSE));
		assert(count == i + 1);
	}
	g_main_context_unref(context);
	return 0;
}
