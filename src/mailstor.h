/*
 * File: mailstor.h
 *
 * General mail storage routines; header file.
 *
 * Bob Eager   August 2003
 *
 */

/* Miscellaneous constants */

#define	MAXMAILID		9	/* Maximum length of a mail ID */
#ifndef	NOLOG
#define	SECURITY_LOG		"I:\\MPTN\\ETC\\SECURITY\\"
#endif

#define	FALSE			0
#define	TRUE			1

/* Error codes */

#define	MAILINIT_OK		0	/* Initialisation successful */
#define	MAILINIT_NOENV		1	/* Environment variable not set */
#define	MAILINIT_BADDIR		2	/* Cannot access directory */

/* External references */

extern	BOOL	mail_close(VOID);
extern	INT	mail_init(PUCHAR);
extern	BOOL	mail_open(PUCHAR *);
extern	VOID	mail_reset(VOID);
extern	BOOL	mail_store(PUCHAR);

/*
 * End of file: mailstor.h
 *
 */
