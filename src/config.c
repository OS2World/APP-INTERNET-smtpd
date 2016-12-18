/*
 * File: config.c
 *
 * SMTP daemon for receiving mail on Tavi network; to be invoked only
 * by INETD.
 *
 * Configuration file handler.
 *
 * Bob Eager   August 2003
 *
 */

#pragma	strings(readonly)

#pragma	alloc_text(a_init_seg, read_config)
#pragma	alloc_text(a_init_seg, config_error)
#pragma	alloc_text(a_init_seg, getcmd)

#include "smtpd.h"
#include "confcmds.h"

#define	MAXLINE		200		/* Maximum length of a config line */

/* Forward references */

static	VOID	config_error(INT, PUCHAR, ...);
static	INT	getcmd(PUCHAR);


/*
 * Read and parse the configuration file specified by 'configfile'.
 *
 * Returns:
 *	Number of errors encountered.
 *	Any error messages have already been issued.
 *
 * The configuration information is returned in the structure 'config'.
 *
 */

INT read_config(PUCHAR direnv, PUCHAR configfile, PCONFIG config)
{	INT line = 0;
	INT errors = 0;
	ULONG addr, mask;
	PUCHAR p, q, r, s, temp;
	UCHAR filename[CCHMAXPATH];
	FILE *fp;
	UCHAR buf[MAXLINE];
	PINADDRENT phost;

	p = getenv(direnv);
	if(p == (PUCHAR) NULL) {
		config_error(0, "environment variable %s is not set", direnv);
		return(++errors);
	}
	strcpy(filename, p);
	p = p + strlen(filename) - 1;	/* Point to last character */
	if(*p != '/' && *p != '\\') strcat(filename, "\\");
	strcat(filename, configfile);

	/* Set defaults */

	memset(config, 0, sizeof(CONFIG));
	config->nthosts = 0;
	config->thost_list = (PINADDRENT) NULL;
	config->log_type = LOGGING_FILE;

	fp = fopen(filename, "r");
	if(fp == (FILE *) NULL) {
		config_error(0, "cannot open configuration file %s", filename);
		return(++errors);
	}

	for(;;) {
		p = fgets(buf, MAXLINE, fp);
		if(p == (PUCHAR) NULL) break;
		temp = p + strlen(p) - 1;	/* Point to last character */
		if(*temp == '\n') *temp = '\0';	/* Remove any newline */
		line++;

		p = strchr(buf, '#');		/* Strip comments */
		if(p != (PUCHAR) NULL) *p = '\0';

		p = strtok(buf, " \t");
		q = strtok(NULL, " \t");
		r = strtok(NULL, " \t");
		s = strtok(NULL, " \t");

		/* Skip non-information lines */

		if((p == (PUCHAR) NULL) ||	/* No tokens */
		   (*p == '\n'))		/* Empty line */
			continue;

		switch(getcmd(p)) {
			case CMD_TRUSTED_HOST:
				if(s != (PUCHAR) NULL) {
					config_error(
						line,
						"syntax error (extra on end)");
					errors++;
					continue;
				}
				if(q == (PUCHAR) NULL) {
					config_error(
						line,
						"no address after "
						"TRUSTED_HOST command");
					errors++;
					break;
				}
				if(r == (PUCHAR) NULL) {
					config_error(
						line,
						"no mask after "
						"TRUSTED_HOST command");
					errors++;
					break;
				}
				addr = inet_addr(q);
				if(addr == INADDR_NONE) {
					config_error(
						line,
						"malformed address "
						"'%s'",
						q);
					errors++;
					break;
				}
				mask = inet_addr(r);
				if(mask == INADDR_NONE) {
					config_error(
						line,
						"malformed mask "
						"'%s'",
						r);
					errors++;
					break;
				}
				phost = (PINADDRENT) malloc(sizeof(INADDRENT));
				if(phost == (PUCHAR) NULL) {
					config_error(
						0,
						"cannot allocate memory");
					errors++;
					break;
				}
				phost->interface_address.s_addr = addr;
				phost->interface_mask.s_addr = mask;
				phost->next = (PINADDRENT) NULL;
				if(config->thost_list == (PINADDRENT) NULL) {
					config->thost_list = phost;
				} else {
					PINADDRENT temp = config->thost_list;

					while(temp->next != (PINADDRENT) NULL)
						temp = temp->next;
					temp->next = phost;
				}
				config->nthosts++;
				break;

			case CMD_LOGGING:
				if(r != (PUCHAR) NULL) {
					config_error(
						line,
						"syntax error (extra on end)");
					errors++;
					continue;
				}
				if(stricmp(q, "file") == 0) {
					config->log_type = LOGGING_FILE;
					continue;
				}
				if(stricmp(q, "syslog") == 0) {
					config->log_type = LOGGING_SYSLOG;
					continue;
				}
				config_error(
					line,
					"unrecognised logging type '%s'",
					q);
				errors++;
				continue;
				break;

			default:
				config_error(
					line,
					"unrecognised command '%s'", p);
				errors++;
				break;
		}
	}

	fclose (fp);

	if(config->nthosts == 0) {
		config_error(0, "at least one trusted host must be"
				" specified");
		return(++errors);
	}

	return(errors);
}


/*
 * Check command in 's' for validity, and return command code.
 * Case is immaterial.
 *
 * Returns CMD_BAD if command not recognised.
 *
 */

static INT getcmd(PUCHAR s)
{	INT i;

	for(i = 0; ; i++) {
		if(cmdtab[i].cmdcode == CMD_BAD) return(CMD_BAD);
		if(stricmp(s, cmdtab[i].cmdname) == 0) break;
	}

	return(cmdtab[i].cmdcode);
}


/*
 * Output configuration error message to standard error in printf style.
 *
 */

static VOID config_error(INT line, PUCHAR mes, ...)
{	va_list ap;
	UCHAR buf[MAXLOG];

	va_start(ap, mes);
	vsprintf(buf, mes, ap);
	va_end(ap);

	if(line == 0)
		error("config: %s", buf);
	else
		error("config: line %d: %s", line, buf);
}

/*
 * End of file: config.c
 *
 */
