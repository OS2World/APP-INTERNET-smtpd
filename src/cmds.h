/*
 * File: cmds.h
 *
 * SMTP daemon for receiving mail on Tavi network; to be invoked only
 * by INETD.
 *
 * Command codes and table.
 *
 * Bob Eager   August 2003
 *
 */

/* Internal command codes */

#define	BAD	0
#define	HELO	1
#define	EHLO	2
#define	MAIL	3
#define	RCPT	4
#define	DATA	5
#define	RSET	6
#define	SEND	7
#define	SOML	8
#define	SAML	9
#define	VRFY	10
#define	EXPN	11
#define	HELP	12
#define	NOOP	13
#define	QUIT	14
#define	TURN	15

#define	CMDSIZE	4			/* Size of an SMTP command */

static	struct	cmdtab {
	PUCHAR	cmdname;		/* Command name */
	INT	cmdcode;		/* Command code */
	BOOL	supported;		/* True if command actually supported */
} cmdtab[] = {
	{ "DATA", DATA, TRUE  },
	{ "EHLO", EHLO, TRUE  },
	{ "EXPN", EXPN, FALSE },
	{ "HELO", HELO, TRUE  },
	{ "HELP", HELP, TRUE  },
	{ "MAIL", MAIL, TRUE  },
	{ "NOOP", NOOP, TRUE  },
	{ "QUIT", QUIT, TRUE  },
	{ "RCPT", RCPT, TRUE  },
	{ "RSET", RSET, TRUE  },
	{ "SAML", SAML, FALSE },
	{ "SEND", SEND, FALSE },
	{ "SOML", SOML, FALSE },
	{ "TURN", TURN, FALSE },
	{ "VRFY", VRFY, FALSE },
	{ "",     BAD , FALSE }			/* End of table marker */
};

/*
 * End of file: cmds.h
 *
 */

