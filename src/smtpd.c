/*
 * File: smtpd.c
 *
 * SMTP daemon for receiving mail on Tavi network; to be invoked only
 * by INETD.
 *
 * Main program
 *
 * Bob Eager   August 2003
 *
 */

/*
 * History:
 *	1.0	Initial version.
 *	1.1	First release version.
 *	1.2	Fix drive/directory problem for spool area.
 *	1.3	Further fix for drive/directory problems.
 *	1.4	Use new thread-safe logging module.
 *		Use OS/2 type definitions.
 *	1.5	New, simplified network interface module.
 *		Grouped initialisation code together.
 *	1.6	Diagnostics for occasional logfile open failures.
 *	1.7	Added option security logging.
 *	1.8	Added configuration file, and blocking of non-trusted clients.
 *	1.9	Corrected handling of part line comments in config file.
 *	2.0	Changed name of configuration file to MAIL.CNF.
 *		Added BLDLEVEL string.
 *		Additional error checking in logging module.
 *	3.0	Recompiled using 32-bit TCP/IP toolkit, in 16-bit mode.
 *	3.1	Added support for service on alternate port (service 'smtpdh')
 *		to hold outgoing messages for validation.
 *		This is enabled by the use of a different mail directory
 *		defined by the environment variable SMTPH (instead of SMTP).
 *		Added support for using long filenames on JFS.
 *		Removed redundant 'addsockettolist' declaration (it is now
 *		properly defined and documented in the toolkit).
 *	4.0	Added support for logging to 'syslog' instead of to file,
 *		selectable by LOGGING configuration file option. This has the
 *		advantage of avoiding clashing logfile usage.
 *	4.1	Changes to conform more closely with RFC 2821.
 *		Now accept EHLO command as well as HELO, although no actual
 *		service extensions are currently defined; also give updated
 *		form of 250 response.
 *		Tolerate trailing spaces in commands.
 *		Reject commands with extraneous parameters (DATA, NOOP,
 *		RSET, QUIT).
 *		Implemented basic HELP command.
 *		Accept some commands (EXPN, HELP, NOOP, RSET, VRFY) before
 *		EHLO/HELO (if implemented).
 *		Tidied up and corrected some error response codes and text.
 *		Accept EHLO/HELO at any time, treating as RSET.
 *		Add client IP to Received: line, also use correct protocol
 *		name (SMTP or ESMTP) based on EHLO/HELO.
 *		Changed timezone to offset in Received: line, to comply
 *		with RFC2821.
 *		Make timestamps conform to RFC2821/RFC2822 in terms of
 *		leading zeros and four digit year values.
 *
 */

#pragma	strings(readonly)

#pragma	alloc_text(a_init_seg, main)
#pragma	alloc_text(a_init_seg, error)
#pragma	alloc_text(a_init_seg, fix_domain)
#pragma	alloc_text(a_init_seg, log_connection)

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "smtpd.h"
#include "mailstor.h"

#define	LOGFILE		"SMTPD.Log"	/* Name of log file */
#define	LOGENV		"ETC"		/* Environment variable for log dir */
#define	SMTPDIR		"SMTP"		/* Environment variable for spool dir */
#define	SMTPHDIR	"SMTPH"		/* Environment variable for alt spool dir */
#define	CONFIGFILE	"Mail.Cnf"	/* Name of configuration file */
#define	ETC		"ETC"		/* Environment variable for misc files */
#define	SMTPSERVICE	"smtp"		/* Name of SMTP service */
#define	SMTPHSERVICE	"smtph"		/* Name of SMTP hold service */
#define	TCP		"tcp"		/* TCP protocol */

/* Forward references */

static	VOID	fix_domain(PUCHAR);
static	VOID	log_connection(VOID);

/* Local storage */

static	CONFIG	config;
static	UCHAR	hostip[16];
static	UCHAR 	hostname[MAXDNAME+1];
static	UCHAR 	myname[MAXDNAME+1];
static	USHORT	myport;
static	PUCHAR	progname;


/*
 * Parse arguments and handle options.
 *
 */

INT main(INT argc, PUCHAR argv[])
{	INT sockno, namelen, rc;
	SOCK serv, client;
	PSERV smtpserv;
	USHORT main_serv_port, alt_serv_port;
	PHOST host;
	BOOL trusted;
	PUCHAR p, smtpdir;
	IFREQ ifr;
	PINADDRENT phost;
#ifdef	DEBUG
	INT i;
#endif

	progname = strrchr(argv[0], '\\');
	if(progname != (PUCHAR) NULL)
		progname++;
	else
		progname = argv[0];
	p = strchr(progname, '.');
	if(p != (PUCHAR) NULL) *p = '\0';
	strlwr(progname);

	tzset();			/* Set time zone */
	res_init();			/* Initialise resolver */

	if(argc != 2) {
		error("usage: %s sockno", progname);
		exit(EXIT_FAILURE);
	}

	sockno = atoi(argv[1]);
	if(sockno <= 0) {
		error("bad arg from INETD");
		exit(EXIT_FAILURE);
	}

	rc = sock_init();		/* Initialise socket library */
	if(rc != 0) {
		error("INET.SYS not running");
		exit(EXIT_FAILURE);
	}
	addsockettolist(sockno);	/* Ensure socket belongs to us now */

	/* Get IP address of the client */

	client.sin_family = AF_INET;
	namelen = sizeof(client);
	rc = getpeername(sockno, (PSOCKG) &client, &namelen);
	if(rc != 0) {
		error("cannot get peer name, errno = %d", sock_errno());
		exit(EXIT_FAILURE);
	}

	/* Get the IP address and port to which the socket passed to us was
	   bound. */

	serv.sin_family = AF_INET;
	namelen = sizeof(serv);
	rc = getsockname(sockno, (PSOCKG) &serv, &namelen);
	if(rc != 0) {
		error("cannot get local name, errno = %d", sock_errno());
		exit(EXIT_FAILURE);
	}
	myport = ntohs(serv.sin_port);

	/* Get the host name of this server; if not possible, set it to the
	   dotted address. */

	rc = gethostname(myname, sizeof(myname));
	if(rc != 0) {
		INADDR myaddr;

		myaddr.s_addr = htonl(gethostid());
		sprintf(myname, "[%s]", inet_ntoa(myaddr));
	} else {
		fix_domain(myname);
	}

	/* Get the host name of the client; if not possible, set it to the
	   dotted address. Store the dotted address anyway, as it's needed
	   for the Received: line. */

	host = gethostbyaddr((PUCHAR) &client.sin_addr,
			     sizeof(client.sin_addr), AF_INET);
	if(host == (struct hostent *) NULL) {
		if(h_errno == HOST_NOT_FOUND) {
			sprintf(hostname, "[%s]", inet_ntoa(client.sin_addr));
		} else {
			error("cannot get host name, errno = %d", h_errno);
			exit(EXIT_FAILURE);
		}
	} else {
		strcpy(hostname, host->h_name);
		fix_domain(hostname);
	}
	strcpy(hostip, inet_ntoa(client.sin_addr));

	/* Get main and alternate service ports */

	smtpserv = getservbyname(SMTPSERVICE, TCP);
	if(smtpserv == (PSERV) NULL) {
		endservent();
		error("cannot get port for %s/%s service", SMTPSERVICE, TCP);
		exit(EXIT_FAILURE);
	}
	main_serv_port = ntohs(smtpserv->s_port);
	smtpserv = getservbyname(SMTPHSERVICE, TCP);
	if(smtpserv == (PSERV) NULL) {
		alt_serv_port = -1;
	} else {
		alt_serv_port = ntohs(smtpserv->s_port);
	}
	endservent();

	/* Read configuration */

	rc = read_config(ETC, CONFIGFILE, &config);
	if(rc != 0) {
		error(
			"%d configuration error%s",
			rc, rc == 1 ? "" : "s");
		exit(EXIT_FAILURE);
	}

	/* Start logging */

	rc = open_log(config.log_type, LOGENV, LOGFILE, myname, progname);
	if(rc != LOGERR_OK) {
		error(
		"logging initialisation failed - %s",
		rc == LOGERR_NOENV    ? "environment variable "LOGENV" not set" :
		rc == LOGERR_OPENFAIL ? "file open failed" :
					"internal log type failure");
		exit(EXIT_FAILURE);
	}

#ifdef	DEBUG
	trace(
		"config: number of trusted hosts = %d", config.nthosts);
	phost = config.thost_list;
	i = 1;
	while(phost != (PINADDRENT) NULL) {
		UCHAR ms[MAXADDR];

		strcpy(ms, inet_ntoa(phost->interface_mask));
		trace(
			"config: trusted host %d is %s; mask %s",
			i,
			inet_ntoa(phost->interface_address),
			ms);
		i++;
		phost = phost->next;
	}
	trace(
		"config: logging type = %s", config.log_type == LOGGING_FILE ?
						"FILE" : "SYSLOG");
#endif

	log_connection();

	smtpdir = (myport == main_serv_port) ? SMTPDIR : SMTPHDIR;
#ifdef	DEBUG
	trace("socket is bound to port %d", myport);
	trace("main service port is %d, alt service port is %d",
		main_serv_port, alt_serv_port);
	trace("SMTP directory is '%s'", smtpdir);
	trace("hostname = '%s', host IP = %s", hostname, hostip);
#endif

	/* Check that the client is a trusted host */

	trusted = FALSE;
	phost = config.thost_list;
	while(phost != (PINADDRENT) NULL) {
#ifdef	DEBUG
		UCHAR as[MAXADDR];
		UCHAR ms[MAXADDR];

		strcpy(as, inet_ntoa(phost->interface_address));
		strcpy(ms, inet_ntoa(phost->interface_mask));
		trace(
			"checking client %s against: %s mask %s",
			inet_ntoa(client.sin_addr),
			as,
			ms);
#endif
		if((phost->interface_address.s_addr &
		    phost->interface_mask.s_addr) ==
		   (client.sin_addr.s_addr & phost->interface_mask.s_addr)) {
			trusted = TRUE;
#ifdef	DEBUG
			trace("trusted check succeeded");
#endif
			break;
		}
#ifdef	DEBUG
		trace("trusted check failed");
#endif
		phost = phost->next;
	}

	if(trusted == FALSE) {
		UCHAR mes[MAXLOG+1];

		sprintf(
			mes,
			"attempted connection from non-trusted host: %s",
			inet_ntoa(client.sin_addr));
		dolog(LOG_ERR, mes);
		rc = 1;			/* Force failure */
	} else {			/* Run the server */
		rc = server(sockno, hostname, hostip, myname, smtpdir);
	}

	/* Shut down */

	(VOID) soclose(sockno);
	mail_reset();			/* Tidy any partial file */
	close_log();

	return(rc == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}


/*
 * Print message on standard error in printf style,
 * accompanied by program name.
 *
 */

VOID error(PUCHAR mes, ...)
{	va_list ap;

	fprintf(stderr, "%s: ", progname);

	va_start(ap, mes);
	vfprintf(stderr, mes, ap);
	va_end(ap);

	fputc('\n', stderr);
}


/*
 * Check for a full domain name; if not present, add default domain name.
 *
 */

static VOID fix_domain(PUCHAR name)
{	if(strchr(name, '.') == (PUCHAR) NULL && _res.defdname[0] != '\0') {
		strcat(name, ".");
		strcat(name, _res.defdname);
	}
}


/*
 * Log details of the connection to standard output and to the logfile.
 *
 */

static VOID log_connection(VOID)
{	time_t tod;
	UCHAR timeinfo[35];
	UCHAR buf[100];

	(VOID) time(&tod);
	(VOID) strftime(timeinfo, sizeof(timeinfo),
		"on %a %d %b %Y at %X %Z", localtime(&tod));
	sprintf(buf, "%s: connection from %s, %s",
		progname, hostname, timeinfo);
	fprintf(stdout, "%s\n", buf);

	sprintf(buf, "connection from %s", hostname);
	dolog(LOG_INFO, buf);
}

/*
 * End of file: smtpd.c
 *
 */
