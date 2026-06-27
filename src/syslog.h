#ifndef __SYSLOG_H__
#define __SYSLOG_H__

#include "memmap.h"
#include "util.h"
#include "hw/reg/reg.h"

#define AVD_LOG_SIZE	512
#define AVD_MAX_LOG	128
#define AVD_LOG_MASK	AVD_MAX_LOG - 1
#define AVD_LOG_NOTIFY	BIT(8)

enum avd_loglevel {
	AVD_LOG_DEBUG = 0,
	AVD_LOG_INFO,
	AVD_LOG_WARN,
	AVD_LOG_ERR,
};

struct avd_log_entry {
	enum avd_loglevel lvl;
	u32 stream_id;
	char log[AVD_MAX_LOG];
};

struct avd_log_ringbuf {
	volatile u32 w;
	volatile u32 r;

	struct avd_log_entry logs[AVD_LOG_SIZE];
};

static volatile struct avd_log_ringbuf *logbuf =
	(volatile struct avd_log_ringbuf *)AVD_LOG_BUF;

void avd_log(enum avd_loglevel, u32 stream_id, const char *log);

#endif /* __SYSLOG_H__ */
