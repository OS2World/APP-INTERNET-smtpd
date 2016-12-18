/*
 * File: server.c
 *
 * SMTP daemon for receiving mail on Tavi network; to be invoked only
 * by INETD.
 *
 * Protocol handler for server
 *
 * Bob Eager   August 2003
 *
 */

#pragma	strings(readonly)

#pragma	alloc_text(a_init_seg, server)
#pragma	alloc_text(a_init_seg, greeting)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#include "smtpd.h"
#include "cmds.h"
#include "mailstor.h"
#include "netio.h"

#define	MAXCMD		514		/* Maximum length of a command */
#define	MAXLINE		1002		/* Maximum length of line */
#define	MAXREPLY	514		/* Maximum length of reply message */
#define	CMD_TIMEOUT	60		/* Command timeout (secs) */
#define	DATA_TIMEOUT	60		/* Data timeout (secs) */
#define	MSG_TIMEOUT	5		/* Fatal message write timeout (secs) */

/* Type definitions */

typedef	enum	{ ST_CONNECT, ST_READY, ST_MAIL, ST_RCPT, ST_DATA }
	STATE;

/* Forward references */

static	BOOL	do_helo(INT, UCHAR [], PUCHAR);
static	BOOL	do_ehlo(INT, UCHAR [], PUCHAR);
static	VOID	do_help(INT);
static	BOOL	do_data(INT, PUCHAR, PUCHAR, PUCHAR, PUCHAR);
static	VOID	do_mail(INT, UCHAR []);
static	VOID	do_quit(INT, PUCHAR);
static	VOID	do_rcpt(INT, UCHAR []);
static	VOID	expect_ehlo(INT);
static	INT	getcmd(PUCHAR);
static	VOID	greeting(INT, PUCHAR);
static	VOID	net_read_error(INT, PUCHAR);
static	VOID	net_read_timeout(INT, PUCHAR);
static	BOOL	no_params(PUCHAR);
static	VOID	process_commands(INT, PUCHAR, PUCHAR, PUCHAR);

/* Local storage */

static	BOOL	esmtp;			/* True if EHLO seen */
static	UCHAR	logmsg[MAXREPLY];	/* Logging buffer */
static	UCHAR	*msg_id;		/* Message ID as a string */
static	INT	nrcpts;			/* Number of recipients so far */
static	STATE	state;			/* Internal state */


/*
 * Do the conversation between the server and the client.
 *
 * Returns:
 *	TRUE		server ran and terminated
 *	FALSE		server failed to start
 *
 */


BOOL server(INT sockno, PUCHAR clientname, PUCHAR clientip,
		PUCHAR servername, PUCHAR direnv)
{	INT rc;
	PUCHAR mes;

	rc = mail_init(direnv);
	switch(rc) {
		case MAILINIT_OK:
			break;

		case MAILINIT_NOENV:
			error("environment variable %s not set", direnv);
			return(FALSE);

		case MAILINIT_BADDIR:
			error("cannot access mail storage directory");
			return(FALSE);

		default:
			error("mail storage initialisation failed");
			return(FALSE);
	}

	if(netio_init() == FALSE) {
		error("network initialisation failure");
		return(FALSE);
	}

	greeting(sockno, servername);

	process_commands(sockno, clientname, clientip, servername);

	return(TRUE);
}


/*
 * Process SMTP commands.
 *
 */

static VOID process_commands(INT sockno, PUCHAR clientname, PUCHAR clientip,
			PUCHAR servername)
{	INT cmd, len, i;
	UCHAR cmdbuf[MAXCMD+1];
	BOOL nlflag;
#ifdef	DEBUG
	UCHAR dbugbuf1[10];
	UCHAR dbugbuf2[MAXCMD*3+1];
#endif
	state = ST_CONNECT;

	for(;;) {
		len = sock_gets(cmdbuf, sizeof(cmdbuf), sockno, CMD_TIMEOUT);
		if(len == SOCKIO_ERR) {
			net_read_error(sockno, servername);
			return;
		}
		if(len == SOCKIO_TIMEOUT) {
			net_read_timeout(sockno, servername);
			return;
		}
		if(len == SOCKIO_TOOLONG) {
			sock_puts("500 Line too long\n", sockno, CMD_TIMEOUT);
			continue;
		}

		cmd = getcmd(cmdbuf);
		for(i = 0; i < CMDSIZE; i++)
			cmdbuf[i] = toupper(cmdbuf[i]);

		/* Remove the trailing newline, strip trailing spaces,
		   then put it back. */

		nlflag = FALSE;
		if(cmdbuf[len-1] == '\n') {
			nlflag = TRUE;
			len--;
		}
		while(cmdbuf[len-1] == ' ') len--;
		if(nlflag == TRUE) cmdbuf[len++] = '\n';
		cmdbuf[len] = '\0';
#ifdef	DEBUG
		trace(cmdbuf);
		dbugbuf2[0] = '\0';
		for(i = 0; i < strlen(cmdbuf); i++) {
			sprintf(dbugbuf1, "%02x ", cmdbuf[i]);
			strcat(dbugbuf2, dbugbuf1);
		}
		trace(dbugbuf2);
#endif

		switch(cmd) {
			case EHLO:
				esmtp = TRUE;
				if(do_ehlo(sockno, cmdbuf, servername) == TRUE)
					state = ST_READY;
				break;

			case HELO:
				esmtp = FALSE;
				if(do_helo(sockno, cmdbuf, servername) == TRUE)
					state = ST_READY;
				break;

			case NOOP:
				if(no_params(cmdbuf) == TRUE) {
					sock_puts(
						"250 OK\n",
						sockno,
						CMD_TIMEOUT);
				} else {
					sock_puts(
						"501 Syntax error "
						"in parameters or arguments\n",
						sockno,
						CMD_TIMEOUT);
				}
				break;

			case MAIL:
				if(state == ST_CONNECT) {
					expect_ehlo(sockno);
					break;
				}
				if(state != ST_READY) {
					sock_puts(
						"503 Bad sequence of "
						"commands\n",
						sockno,
						CMD_TIMEOUT);
					break;
				}
				do_mail(sockno, cmdbuf);
				break;

			case RCPT:
				if(state == ST_CONNECT) {
					expect_ehlo(sockno);
					break;
				}
				if(state != ST_MAIL && state != ST_RCPT) {
					sock_puts(
						"503 Bad sequence "
						"of commands\n",
						sockno,
						CMD_TIMEOUT);
					break;
				}
				do_rcpt(sockno, cmdbuf);
				break;

			case DATA:
				if(state == ST_CONNECT) {
					expect_ehlo(sockno);
					break;
				}
				if(state != ST_RCPT) {
					sock_puts(
						"503 Bad sequence "
						"of commands\n",
						sockno,
						CMD_TIMEOUT);
					break;
				}
				if(do_data(
					sockno,
					clientname,
					clientip,
					servername,
					cmdbuf) == FALSE) return;
				state = ST_READY;
				break;

			case RSET:
				if(no_params(cmdbuf) == TRUE) {
					mail_reset();
					sock_puts(
						"250 OK\n",
						sockno,
						CMD_TIMEOUT);
					state = ST_READY;
					logmsg[0] = '\0';
				} else {
					sock_puts(
						"501 Syntax error "
						"in parameters or arguments\n",
						sockno,
						CMD_TIMEOUT);
				}
				break;

			case QUIT:
				if(no_params(cmdbuf) == TRUE) {
					do_quit(sockno, servername);
					return;
				} else {
					sock_puts(
						"501 Syntax error "
						"in parameters or arguments\n",
						sockno,
						CMD_TIMEOUT);
				}
				break;

			case HELP:
				do_help(sockno);
				break;

			case VRFY:
			case EXPN:
			case SEND:
			case SOML:
			case SAML:
			case TURN:
				sock_puts(
					"502 Command not implemented\n",
					sockno,
					CMD_TIMEOUT);
				break;

			case BAD:
				sock_puts(
					"500 Syntax error, "
					"command not recognized\n",
					sockno,
					CMD_TIMEOUT);
				break;

			default:
				error("bad case in command switch %d", cmd);
				sprintf(
					cmdbuf,
					"bad case in command switch %d\n",
					cmd);
				dolog(LOG_CRIT, cmdbuf);
				exit(EXIT_FAILURE);
		}
	}
}


/*
 * Parse a line for a valid command.
 *
 */

static INT getcmd(PUCHAR buf)
{	INT i;

	if(strlen(buf) < CMDSIZE) return(BAD);
	for(i = 0; ; i++) {
		if(cmdtab[i].cmdcode == BAD) return(BAD);
		if(strnicmp(buf, cmdtab[i].cmdname, CMDSIZE) == 0) break;
	}

	return(cmdtab[i].cmdcode);
}


/*
 * Action when a command is received and EHLO/HELO is expected.
 *
 */

static VOID expect_ehlo(INT sockno)
{	sock_puts("503 Bad sequence of commands\n", sockno, CMD_TIMEOUT);
}


/*
 * Send a message indicating a network read error.
 *
 */

static VOID net_read_error(INT sockno, PUCHAR servername)
{	UCHAR mes[MAXREPLY+1];

	sprintf(
		mes,
		"421 %s Service closing transmission channel\n",
		servername);
	sock_puts(mes, sockno, MSG_TIMEOUT); 
	dolog(LOG_ERR, "network read error\n");
#ifdef	DEBUG
	trace("sock_errno = %d", sock_errno());
#endif
	mail_reset();
}


/*
 * Send a message indicating a network read timeout.
 *
 */

static VOID net_read_timeout(INT sockno, PUCHAR servername)
{	UCHAR mes[MAXREPLY+1];

	sprintf(
		mes,
		"421 %s Service closing transmission channel\n",
		servername);
	sock_puts(mes, sockno, MSG_TIMEOUT); 
	dolog(LOG_ERR, "network read timeout\n");
	mail_reset();
}


/*
 * Output a greeting on initial connection.
 *
 */

static VOID greeting(INT sockno, PUCHAR servername)
{	UCHAR mes[MAXREPLY+1];

	sprintf(
		mes,
		"220 %s SMTP server version %d.%d ready\n",
		servername,
		VERSION,
		EDIT);
	sock_puts(mes, sockno, CMD_TIMEOUT);
}


/*
 * Handle a EHLO command.
 *
 * Returns:
 *	TRUE	if command OK
 *	FALSE	if syntax error
 *
 * Currently, there are no service extensions defined;
 * thus, EHLO is treated exactly the same as HELO.
 *
 */

static BOOL do_ehlo(INT sockno, PUCHAR cmdbuf, PUCHAR servername)
{
	return(do_helo(sockno, cmdbuf, servername));
}


/*
 * Handle a HELO command.
 *
 * Returns:
 *	TRUE	if command OK
 *	FALSE	if syntax error
 *
 */

static BOOL do_helo(INT sockno, PUCHAR cmdbuf, PUCHAR servername)
{	UCHAR mes[MAXREPLY+1];
	PUCHAR p = &cmdbuf[CMDSIZE];

	while(*p == ' ') p++;
	if(*p == '\n') {
		sock_puts(
			"501 Syntax error in parameters or arguments\n",
			sockno,
			CMD_TIMEOUT);
		return(FALSE);
	}

	sprintf(mes, "250 %s service ready\n", servername);
	sock_puts(mes, sockno, CMD_TIMEOUT);

	mail_reset();			/* In case this is not first time */
	logmsg[0] = '\0';

	return(TRUE);
}


/*
 * Handle the QUIT command.
 *
 */

static VOID do_quit(INT sockno, PUCHAR servername)
{	UCHAR mes[MAXREPLY+1];

	sprintf(
		mes,
		"221 %s Service closing transmission channel\n",
		servername);
	sock_puts(mes, sockno, CMD_TIMEOUT);
}


/*
 * Handle a MAIL command.
 *
 */

static VOID do_mail(INT sockno, PUCHAR cmdbuf)
{	static UCHAR from[] = { 'F', 'R', 'O', 'M', ':' };
	PUCHAR p = &cmdbuf[CMDSIZE];

	while(*p == ' ') p++;		/* Skip spaces and move to "From:" */

	if(((strlen(p) <= sizeof(from)+1) ||
	   (strnicmp(p, from, sizeof(from)) != 0))) {
		sock_puts(
			"501 Syntax error in parameters or arguments\n",
			sockno,
			CMD_TIMEOUT);
	} else {
		if(mail_open(&msg_id) == FALSE ||
		   mail_store(cmdbuf) == FALSE) {
			sock_puts(
				"452 Requested action not taken: "
				"insufficient system storage\n",
				sockno,
				CMD_TIMEOUT);
		} else {
			strcpy(logmsg, "mail from ");
			strcat(logmsg, p + sizeof(from));
			logmsg[strlen(logmsg)-1] = '\0';	/* Lose '\n' */
			sock_puts("250 OK\n", sockno, CMD_TIMEOUT);
			nrcpts = 0;
			state = ST_MAIL;
		}
	}
}


/*
 * Handle a RCPT command.
 *
 */

static VOID do_rcpt(INT sockno, PUCHAR cmdbuf)
{	static UCHAR to[] = { 'T', 'O', ':' };
	PUCHAR p = cmdbuf + CMDSIZE;

	while(*p == ' ') p++;		/* Skip spaces and move to "To:" */

	if(((strlen(p) <= sizeof(to)+1) ||
	   (strnicmp(p, to, sizeof(to)) != 0))) {
		sock_puts(
			"501 Syntax error in parameters or arguments\n",
			sockno,
			CMD_TIMEOUT);
	} else {
		if(mail_store(cmdbuf) == FALSE) {
			sock_puts(
				"452 Requested action not taken: "
				"insufficient system storage\n",
				sockno,
				CMD_TIMEOUT);
		} else {
			if(++nrcpts == 1) {
				strcat(logmsg, " to ");
				strcat(logmsg, p + sizeof(to));
				logmsg[strlen(logmsg)-1] = '\0';/* Lose '\n' */
			} else {
				if(nrcpts == 2) strcat(logmsg, "...");
			}
			sock_puts("250 OK\n", sockno, CMD_TIMEOUT);
			state = ST_RCPT;
		}
	}
}


/*
 * Handle a DATA command.
 *
 * Returns:
 *	TRUE	if message stored OK
 *	FALSE	if message not stored OK
 *
 */

static BOOL do_data(INT sockno, PUCHAR clientname, PUCHAR clientip,
			PUCHAR servername, PUCHAR cmdbuf)
{	time_t tod, utc, utcdiff;
	struct tm gtm;
	INT len, index;
	PUCHAR p = cmdbuf + CMDSIZE;
	UCHAR offset[20];
	UCHAR timeinfo[40];
	UCHAR buf[MAXLINE+1];
	UCHAR buf2[MAXLINE+1];

	while(*p == ' ') p++;		/* Skip spaces */

	if(*p != '\n') {		/* Something else on the line */
		sock_puts(
			"501 Syntax error in parameters or arguments\n",
			sockno,
			CMD_TIMEOUT);
		return(FALSE);
	}

	/* Set up timestamp. To be RFC2821 compliant, the timezone
	   must be displayed as an offset. To get that offset, we
	   get the UTC time into a 'tm' structure using gmtime, then
	   convert it back to a 'time_t' so that we can use difftime
	   to get the actual difference. */

	(VOID) time(&tod);
	gtm = *(gmtime(&tod));		/* Get UTC as a structure */
	utc = mktime(&gtm);		/* Convert back to time_t */
	utcdiff = difftime(tod, utc)/60;/* Offset in minutes */
	sprintf(
		offset,
		" %c%02d%02d",
		utcdiff >= 0 ? '+': '-', abs(utcdiff)/60, abs(utcdiff)%60);

	(VOID) strftime(
			timeinfo,
			sizeof(timeinfo),
			"%a, %Od %b %Y %X",
			localtime(&tod));
	strcat(timeinfo, offset);

	sprintf(
		buf,
		"Received: from %s (%s) by %s\n",
		clientname,
		clientip,
		servername);
	sprintf(buf2,
		"          with %s id %s; %s\n",
		esmtp == TRUE ? "ESMTP" : "SMTP",
		msg_id,
		timeinfo);
#ifdef	DEBUG
	trace("%s", buf);
	trace("%s", buf2);
#endif
	if(mail_store("DATA\n") == FALSE ||
	   mail_store(buf) == FALSE ||
	   mail_store(buf2) == FALSE) {
		sock_puts(
			"452 Requested action not taken: "
			"insufficient system storage\n",
			sockno,
			CMD_TIMEOUT);
		return(FALSE);
	}

	sock_puts("354 Start mail input; end with <CRLF>.<CRLF>\n",
		sockno, CMD_TIMEOUT);
	state = ST_DATA;

	for (;;) {
		index = 0;
		len = sock_gets(buf, sizeof(buf), sockno, DATA_TIMEOUT);
		if(len == SOCKIO_ERR || len == 0) {
			net_read_error(sockno, servername);
			return(FALSE);
		}
		if(len == SOCKIO_TIMEOUT) {
			net_read_timeout(sockno, servername);
			return(FALSE);
		}
		if(len == SOCKIO_TOOLONG) {
			sock_puts("500 Line too long\n", sockno, CMD_TIMEOUT);
			continue;
		}

		if (buf[0] == '.') {
			index = 1;			/* Un-stuff dots */
			if(buf[index] == '\n') break;	/* End of data */
		}
		if(mail_store(&buf[index]) == FALSE) {
			sock_puts(
				"452 Requested action not taken: "
				"insufficient system storage\n",
				sockno,
				CMD_TIMEOUT);
			return(FALSE);
		}
#ifdef	DEBUG
		trace("data(%d): %s", strlen(buf), buf);
#endif
	}

	if(mail_close() == FALSE) {
		sock_puts(
			"452 Requested action not taken: "
			"insufficient system storage\n",
			sockno,
			CMD_TIMEOUT);
	}

	dolog(LOG_INFO, logmsg);
	sock_puts("250 OK\n", sockno, CMD_TIMEOUT);
	return(TRUE);
}


/*
 * Handle a HELP command.
 *
 */

static VOID do_help(INT sockno)
{	INT i;
	UCHAR helpbuf[MAXREPLY+1];

	sock_puts(
		"214-Commands supported:\n",
		sockno,
		CMD_TIMEOUT);

	strcpy(helpbuf, "214 ");
	for(i = 0; cmdtab[i].cmdcode != BAD; i++) {
		if(cmdtab[i].supported == TRUE) {
			strcat(helpbuf, cmdtab[i].cmdname);
			strcat(helpbuf, ",");
		}
	}
	helpbuf[strlen(helpbuf)-1] = '\n';	/* Replace comma with newline */
	sock_puts(
		helpbuf,
		sockno,
		CMD_TIMEOUT);
}


/*
 * Check that the current command has been given with no parameters.
 *
 * Returns:
 *	TRUE	if command has no parameters
 *	FALSE	if command has parameters
 *
 */

static BOOL no_params(PUCHAR cmdbuf)
{	PUCHAR p = cmdbuf + CMDSIZE;

	while(*p == ' ') p++;		/* Skip spaces */

	if(*p != '\n')			/* Something else on the line */
		return(FALSE);

	return(TRUE);
}

/*
 * End of file: server.c
 *
 */

