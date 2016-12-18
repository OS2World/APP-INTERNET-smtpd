/*
 * File: confcmds.h
 *
 * SMTP daemon for receiving mail on Tavi network; to be invoked only
 * by INETD.
 *
 * Configuration command codes and table.
 *
 * Bob Eager   August 2003
 *
 */

/* Internal command codes */

#define	CMD_TRUSTED_HOST	1
#define	CMD_LOGGING		2
#define	CMD_BAD			3

static	struct {
	UCHAR	*cmdname;		/* Command name */
	INT	cmdcode;		/* Command code */
} cmdtab[] = {
	{ "TRUSTED_HOST",	CMD_TRUSTED_HOST },
	{ "LOGGING",		CMD_LOGGING },
	{ "",			CMD_BAD }	/* End of table marker */
};

/*
 * End of file: confcmds.h
 *
 */

