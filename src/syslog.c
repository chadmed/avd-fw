#include "syslog.h"

void avd_log(enum avd_loglevel lvl, u32 stream_id, const char *log)
{
	u32 i = 0;
	u32 slot = logbuf->w & AVD_LOG_MASK;
	volatile struct avd_log_entry *e = &logbuf->logs[slot];\

	e->lvl = lvl;
	e->stream_id = stream_id;

	/* We are NOT pulling in a libc */
	while (i < sizeof(*e->log)) {
		if (log[i] == 0)
			break;

		e->log[i] = log[i];
		i++;
	}

	__asm volatile("dmb");

	logbuf->w++;

	reg_write(CM3_MBOX1_TX, AVD_LOG_NOTIFY);
}
