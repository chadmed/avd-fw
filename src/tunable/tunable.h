#ifndef _TUNABLE_h_
#define _TUNABLE_h_

struct tunable {
	int sz;
	struct {
		unsigned int offset; /* offset from decode base */
		unsigned int mask;
		unsigned int value;
	} values[] __attribute__((__counted_by__(sz)));
};

extern const struct tunable tunable;

#endif /* _TUNABLE_h_ */
