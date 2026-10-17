/* Microsecond one-shot timers for QUIC pacing. */
#ifndef IMQUIC_TIMER_H
#define IMQUIC_TIMER_H

#include <stdint.h>
#include <glib.h>
#ifdef __linux__
#include <sys/timerfd.h>
#include <unistd.h>

typedef struct imquic_precision_timer {
	GSource parent;
	int fd;
	gpointer tag;
} imquic_precision_timer;

static gboolean imquic_precision_timer_check(GSource *source) {
	imquic_precision_timer *timer = (imquic_precision_timer *)source;
	return g_source_query_unix_fd(source, timer->tag) != 0;
}

static gboolean imquic_precision_timer_dispatch(GSource *source,
		GSourceFunc callback, gpointer data) {
	imquic_precision_timer *timer = (imquic_precision_timer *)source;
	uint64_t expirations;
	if(read(timer->fd, &expirations, sizeof(expirations)) != sizeof(expirations))
		return G_SOURCE_CONTINUE;
	if(callback != NULL)
		callback(data);
	return G_SOURCE_REMOVE;
}

static void imquic_precision_timer_finalize(GSource *source) {
	imquic_precision_timer *timer = (imquic_precision_timer *)source;
	close(timer->fd);
}

static GSourceFuncs imquic_precision_timer_funcs = {
	NULL, imquic_precision_timer_check, imquic_precision_timer_dispatch,
	imquic_precision_timer_finalize, NULL, NULL
};
#endif

/* The caller attaches and unreferences the returned source. A zero delay means
 * immediately eligible, while positive delays must never be rounded down. */
static inline GSource *imquic_timer_source_new_us(guint64 us) {
#ifdef __linux__
	int fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
	if(fd >= 0) {
		struct itimerspec when = { 0 };
		when.it_value.tv_sec = us / G_USEC_PER_SEC;
		when.it_value.tv_nsec = us % G_USEC_PER_SEC * 1000;
		if(us == 0)
			when.it_value.tv_nsec = 1;
		if(timerfd_settime(fd, 0, &when, NULL) == 0) {
			GSource *source = g_source_new(&imquic_precision_timer_funcs,
				sizeof(imquic_precision_timer));
			imquic_precision_timer *timer = (imquic_precision_timer *)source;
			timer->fd = fd;
			timer->tag = g_source_add_unix_fd(source, fd, G_IO_IN);
			return source;
		}
		close(fd);
	}
#endif
	/* Platforms without timerfd retain GLib timers, rounding positive delays
	 * up instead of turning sub-millisecond pacing into a busy poll. */
	return g_timeout_source_new((guint)MIN(us / 1000 + (us % 1000 != 0), G_MAXUINT));
}
/* A slot owns one source reference. Call these helpers under the endpoint's
 * mutex when scheduling or cancelling from outside its event-loop thread. */
typedef struct imquic_wake_timer {
	GSource *source;
	guint64 deadline_us;
} imquic_wake_timer;

static inline void imquic_wake_timer_cancel(imquic_wake_timer *timer) {
	GSource *source = timer->source;
	timer->source = NULL;
	timer->deadline_us = 0;
	if(source != NULL) {
		g_source_destroy(source);
		g_source_unref(source);
	}
}

static inline void imquic_wake_timer_dispatched(imquic_wake_timer *timer) {
	if(timer->source == g_main_current_source()) {
		GSource *source = timer->source;
		timer->source = NULL;
		timer->deadline_us = 0;
		g_source_unref(source);
	}
}

static inline gboolean imquic_wake_timer_schedule(imquic_wake_timer *timer,
		GMainContext *context, guint64 delay_us, GSourceFunc callback, gpointer data, GDestroyNotify destroy) {
	guint64 now = (guint64)g_get_monotonic_time();
	guint64 deadline = delay_us > G_MAXUINT64 - now ? G_MAXUINT64 : now + delay_us;
	/* Incoming ACKs must not replace an earlier, possibly already due timer. */
	if(timer->source != NULL && !g_source_is_destroyed(timer->source) &&
			timer->deadline_us <= deadline)
		return FALSE;
	imquic_wake_timer_cancel(timer);
	timer->source = imquic_timer_source_new_us(delay_us);
	timer->deadline_us = deadline;
	g_source_set_callback(timer->source, callback, data, destroy);
	g_source_attach(timer->source, context);
	return TRUE;
}

#endif
