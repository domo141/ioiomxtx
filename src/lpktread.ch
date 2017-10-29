/*
 * Author: Tomi Ollila -- too ät iki piste fi
 *
 *	Copyright (c) 1994-2017  Tomi Ollila
 *	    All rights reserved
 *
 * Created: Fri Feb 24 14:06:40 1995 too
 * Modified: Sat Jun 20 17:35:18 1998 too
 * Netpkt version: Sat Aug 25 13:33:30 EEST 2007 too
 * Here: Mon 28 Aug 2017 22:11:12 +0300 too
 * Last modified: Mon 04 Sep 2017 23:39:24 +0300 too
 */

/* LICENSE: 2-clause BSD license ("Simplified BSD License"): */

// try  gcc -x c lpktread.ch -DLPKTREAD_DATA_BUFFER_SIZE  for these includes

#include <unistd.h> // read
#include <string.h> // memmove
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>

#define char unsigned char
struct lpktread
{
    char * currp;    /* current scan point in buffer */
    char * endp;     /* pointer of last character in buffer */
    char * startp;   /* output start position */
    char * sizep;    /* size of pktread buffer */
    int    fd;       /* input file descriptor */
    char   state;    /* input read state */
    char   selected; /* has caller done select() or poll() */
    uint16_t len;     /* output length */
    char   data[LPKTREAD_DATA_BUFFER_SIZE];
};
typedef struct lpktread LPktRead;

enum { STATE_LEN1, STATE_LEN2, STATE_LENCHECK };

static void lpktread_init(LPktRead * pr, int fd)
{
    pr->sizep = pr->data + sizeof pr->data - 1; // - 1 so '\0' may be appended

    pr->fd = fd;

    pr->state = STATE_LEN1;
    pr->currp = pr->endp = (char*)0; /* any value works */
    pr->selected = true;
}

static int lpktread(LPktRead * pr, char ** datap)
{
    if (pr->currp == pr->endp) {
	int i;

	if (! pr->selected) {
	    pr->selected = true;
	    return 0;
	}
	if (pr->state == STATE_LEN1)
	    pr->currp = pr->endp = pr->data;
#if 0
	fprintf(stderr, "read data: %ld %d\n", pr->endp - pr->data, pr->state);
#endif
	while ((i = read(pr->fd, pr->currp, pr->sizep - pr->currp)) <= 0) {
	    if (i < 0) {
		if (errno == EINTR) continue;
		else *datap = (char *)0 - errno;
	    } else *datap = (char *)0;
	    return -1;
	}
	pr->selected = false;
	pr->endp += i;
#if 0
	fprintf(stderr, "bytes of data read: %d\n", i);
#endif
    }

    while (1)
	switch (pr->state) {
	case STATE_LEN1:
	    pr->len = *pr->currp++ << 8;
	    pr->state = STATE_LEN2;

	    if (pr->currp == pr->endp) {
		pr->currp = pr->endp = pr->data;
		pr->selected = true;
		return 0;
	    }
	    /* FALL THROUGH */

	case STATE_LEN2:
	    pr->len |= *pr->currp++;

	    //if (pr->len > pr->sizep - pr->data)
	    if (pr->len >= sizeof pr->data) { // aligned with .sizep setup
		/* msg too long -- consider other end broke protocol */
		*datap = (char *)1;
		return -1;
	    }
	    /* check if length zero (keepalive) */
	    if (pr->len == 0) {
		pr->state = STATE_LEN1;

		if (pr->currp == pr->endp) {
		    /* pr->currp = pr->endp = pr->data; -- done above */
		    pr->selected = true;
		    return 0;
		}
		else
		    continue;
	    }
	    pr->startp = pr->currp;
	    pr->state = STATE_LENCHECK;

	    /* FALL THROUGH */

	case STATE_LENCHECK:
#if 0
	    fprintf(stderr, "LENCHECK: hl %d, bp %d, len %d\n",
		    pr->endp - pr->data, pr->currp - pr->data, pr->len);
#endif
	    /* check if all data is available in read buffer return data if yes */
	    if (pr->endp - pr->startp >= pr->len) {
		pr->currp = pr->startp + pr->len;
		pr->state = STATE_LEN1;
		*datap = pr->startp;
		return pr->len;
	    }
	    /* check if there is enough room to receive needed data to the buffer */
	    if (pr->sizep - pr->currp < pr->len) {
		int len = pr->endp - pr->startp;
		memmove(pr->data, pr->startp, len); /* ... copy if not */
		pr->startp = pr->data;
		pr->endp = pr->data + len;
	    }
	    pr->currp = pr->endp;
	    pr->selected = true;
	    return 0;
	}
    /* NOTREACHED */
    return 0;
}
#undef char

/*
 * Local variables:
 * mode: c
 * c-file-style: "stroustrup"
 * tab-width: 8
 * End:
 */
