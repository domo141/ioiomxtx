
/* ************************* lineread.c ************************* */
/*
 * lineread.c - functions to read lines from fd:s efficiently
 *
 * Created: Mon Jan 14 06:45:00 1991 too
 */

/* LICENSE: 2-clause BSD license ("Simplified BSD License"): */

struct lineread
{
  char * currp;         /* current scan point in buffer */
  char * endp;          /* pointer of last read character in buffer */
  char * startp;        /* pointer to start of output */
  char * sizep;         /* pointer to the end of read buffer */
  int    fd;            /* input file descriptor */
  char   selected;      /* has caller done select()/poll() or does he care */
  char   line_completed;/* line completion in LineRead */
  char   saved;         /* saved char in LineRead */
  char   pad_unused;
  char   data[LINEREAD_DATA_BUFFER_SIZE];
};
typedef struct lineread LineRead;

static int lineread(LineRead * lr, char ** ptr)
{
  int i;

  if (lr->currp == lr->endp)

    if (lr->selected)   /* user called select() (or wants to block) */
    {
      if (lr->line_completed)
        lr->startp = lr->currp = lr->data;

      // XXX add EINTR handling -- perhaps... //
      if ((i = read(lr->fd,
                    lr->currp,
                    lr->sizep - lr->currp)) <= 0) {
        /*
         * here if end-of-file or on error. set endp == currp
         * so if non-blocking I/O is in use next call will go to read()
         */
        lr->endp = lr->currp;
        *ptr = (char *)(intptr_t)i; /* user compares ptr (NULL, (char *)-1, ... */
        return -1;
      }
      else
        lr->endp = lr->currp + i;
    }
    else /* Inform user that next call may block (unless select()ed) */
    {
      lr->selected = true;
      return 0;
    }
  else /* currp has not reached endp yet. */
  {
    *lr->currp = lr->saved;
    lr->startp = lr->currp;
  }

  /*
   * Scan read string for next newline.
   */
  while (lr->currp < lr->endp)
    if (*lr->currp++ == '\n')  /* memchr ? (or rawmemchr & extra \n at end) */
    {
      lr->line_completed = true;
      lr->saved = *lr->currp;
      *lr->currp = '\0';
      lr->selected = false;
      *ptr = lr->startp;

      return lr->currp - lr->startp;
    }

  /*
   * Here if currp == endp, but no NLCHAR found.
   */
  lr->selected = true;

  if (lr->currp == lr->sizep) {
    /*
     * Here if currp reaches end-of-buffer (endp is there also).
     */
    if (lr->startp == lr->data) /* (data buffer too short for whole string) */
    {
      lr->line_completed = true;
      *ptr = lr->data;
      *lr->currp = '\0';
      return -1;
    }
    /*
     * Copy partial string to start-of-buffer and make control ready for
     * filling rest of buffer when next call to lineread() is made
     * (perhaps after select()).
     */
    memmove(lr->data, lr->startp, lr->endp - lr->startp);
    lr->endp-=  (lr->startp - lr->data);
    lr->currp = lr->endp;
    lr->startp = lr->data;
  }

  lr->line_completed = false;
  return 0;
}

static void lineread_init(LineRead * lr, int fd)
{
  lr->fd = fd;
  lr->currp = lr->endp = (void*)0; /* any value works */
  lr->sizep = lr->data + sizeof lr->data;
  lr->selected = lr->line_completed = true;
}

/* ^^^^^^^^^^^^^^^^^^^^^^^^^ lineread.c ^^^^^^^^^^^^^^^^^^^^^^^^^ */
