/*
 * File: mailstor.c
 *
 * General mail storage routines.
 *
 * Bob Eager   August 2003
 *
 */

#pragma	strings(readonly)

#pragma	alloc_text(a_init_seg, mail_init)
#pragma	alloc_text(a_init_seg, fstype)

#include <errno.h>
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys\stat.h>
#include <ctype.h>

#include <os2.h>

#include "smtpd.h"
#include "mailstor.h"

#define	PATCHSIZE	4		/* Size of first line patch area */
#define	FSQBUFSIZE	100		/* Size of FS query buffer */

/* Type definitions */

typedef	enum	{ FS_CDFS, FS_FAT, FS_HPFS, FS_NFS, FS_JFS }
	FSTYPE;

/* Forward references */

static	FSTYPE	fstype(PUCHAR);

/* Local storage */

static	INT	first_line_seen;
static	UCHAR	maildir[CCHMAXPATH+1];
static	UCHAR	mailfile[CCHMAXPATH+1];
static	FILE	*mailfp;
static	FSTYPE	mailfstype;
static	UCHAR	save_temp[PATCHSIZE];
static	UCHAR	temp[] = "TEMP";

/*
 * Initialise storage, etc.
 *
 * Returns:
 *	MAILINIT_OK		Initialisation successful
 *	MAILINIT_NOENV		Mail spool directory env variable not set
 *	MAILINIT_BADDIR		Cannot access mail spool directory
 *
 */

INT mail_init(PUCHAR direnv)
{	APIRET rc;
	INT last;
	INT disk = 0;
	PUCHAR dir;

#ifdef	DEBUG
	if(strlen(temp) != PATCHSIZE) {
		fprintf(stderr, "internal patch area size mismatch\n");
		abort();
	}
#endif
	dir = getenv(direnv);
	if(dir == (PUCHAR) NULL) return(MAILINIT_NOENV);

	if(isalpha(dir[0]) && dir[1] == ':') {
		disk = toupper(dir[0]) - 'A' + 1;

		rc = DosSetDefaultDisk(disk);
		if(rc != 0) return(MAILINIT_BADDIR);
	}

	strcpy(maildir, dir);
	last = strlen(maildir) - 1;
	if(maildir[last] == '\\' || maildir[last] == '/')
		maildir[last] = '\0';
	rc = DosSetCurrentDir(maildir);
	if(rc != 0) return(MAILINIT_BADDIR);

	mailfstype = fstype(maildir);
#ifdef	DEBUG
	trace("mail directory = \"%s\", FS type = %s\n",
		maildir, mailfstype == FS_FAT  ? "FAT"  :
			 mailfstype == FS_HPFS ? "HPFS" :
			 mailfstype == FS_CDFS ? "CDFS" :
			 mailfstype == FS_NFS  ? "NFS"  :
			 mailfstype == FS_JFS  ? "JFS"  :
			 "????");
#endif
	mailfp = (FILE *) NULL;

	return(MAILINIT_OK);
}


/*
 * Function to determine the type of file system on the drive specified
 * in 'path'.
 *
 * Returns:
 *	 FS_CDFS, FS_NFS, FS_FAT, FS_HPFS or FS_JFS.
 *
 * Any error causes the result to default to FS_FAT.
 *
 */

static FSTYPE fstype(PUCHAR path)
{	ULONG drive, dummy;
	ULONG fsqbuflen = FSQBUFSIZE;
	UCHAR fsqbuf[FSQBUFSIZE];
	PUCHAR p;
	UCHAR drv[3];

	if(path[1] != ':') {		/* No drive in path - use default */
		(VOID) DosQueryCurrentDisk(&drive, &dummy);
		drv[0] = drive + 'a' - 1;
	} else {
		drv[0] = path[0];
	}
	drv[1] = ':';
	drv[2] = '\0';

	if(DosQueryFSAttach(
			drv,
			0L,
			FSAIL_QUERYNAME,
			(PFSQBUFFER2) fsqbuf,
			&fsqbuflen) != 0)
		return(FS_FAT);

	/* Set 'p' to point to the file system name */

	p = fsqbuf + sizeof(USHORT);	/* Point past device type */
	p += (USHORT) *p + 3*sizeof(USHORT) + 1;
					/* Point past drive name and FS name */
					/* and FSDA length */

	if(strcmp(p, "CDFS") == 0) return(FS_CDFS);
	if(strcmp(p, "HPFS") == 0) return(FS_HPFS);
	if(strcmp(p, "NFS") == 0) return(FS_NFS);
	if(strcmp(p, "JFS") == 0) return(FS_JFS);

	return(FS_FAT);
}


/*
 * Set up for storage of a new mail message.
 * 'idptr' points to a pointer to be set to the message ID allocated.
 *
 * Returns:
 *	TRUE		storage set up OK
 *	FALSE		storage set up failed
 *
 */

BOOL mail_open(PUCHAR *idptr)
{	INT fd;
	UCHAR c = 'a';
	time_t tod;

	static UCHAR mail_id[MAXMAILID+1];

	(VOID) time(&tod);

	/* Generate a unique mail ID and thus mail filename. This
	   is done simply by seeing if a file with that name already
	   exists. The derivation of the filename depends on whether
	   long filenames are permitted in the mail directory, but in any
	   case it is ultimately the existence of the file that determines
	   whether a mail ID is unique. */

	for(;;) {
		sprintf(mail_id, "%8x%c", tod, c);
		if((mailfstype == FS_HPFS) || (mailfstype == FS_JFS)) {
						/* xxxxxxxxx.mail */
			sprintf(mailfile, "%s.mail", mail_id);
		} else {			/* xxxxxxxx.xml */
			mailfile[0] = '\0';
			strncat(mailfile, mail_id, 8);
			strcat(mailfile, ".");
			strcat(mailfile, &mail_id[8]);
			strcat(mailfile, "ml");
		}
#ifdef	DEBUG
		trace("creating mail file \"%s\"\n", mailfile);
#endif
		fd = open(mailfile,
			  O_CREAT | O_EXCL | O_WRONLY,
			  S_IREAD | S_IWRITE);
		if(fd == -1) {
			if(errno == EEXIST) {
#ifdef	DEBUG
				trace("mail file exists\n");
#endif
				if(c == 'z') return(FALSE);
				c++;
				continue;
			}
			return(FALSE);		/* Some other error */
		}
		break;
	}
	*idptr = mail_id;

	mailfp = fdopen(fd, "w");
	if(mailfp == (FILE *) NULL) return(FALSE);

	first_line_seen = FALSE;
	return(TRUE);
}


/*
 * Close and store a completed mail message, and commit the file for
 * onward transmissiom.
 *
 * Returns:
 *	TRUE		message stored OK
 *	FALSE		message storage failed
 *
 */

BOOL mail_close(VOID)
{	INT p, rc;
	UCHAR temp[CCHMAXPATH+1];

	if(mailfp != (FILE *) NULL) {
		(VOID) fflush(mailfp);

		/* Restore the patched characters at the start of the file,
		   thus indicating that the file is legal and complete. */

		rc = fseek(mailfp, 0L, SEEK_SET);
		if(rc == 0) {
			rc = fwrite(save_temp, PATCHSIZE, 1, mailfp);
			rc = (rc == 1) ? 0 : 1;
		}
		if(rc == 0) rc = fclose(mailfp);
		mailfp = (FILE *) NULL;
		if(rc != 0) return(FALSE);
	}

#ifdef	SECURITY_LOG
	strcpy(temp, SECURITY_LOG);
	strcat(temp, "S_");
	strcat(temp, mailfile);
	(void) DosCopy(mailfile, temp, 0L);
#endif

	return(TRUE);
}


/*
 * Reset state after an incomplete transaction.
 * Simply close and delete any partial mail file.
 *
 */

VOID mail_reset(VOID)
{	if(mailfp != (FILE *) NULL) {
		(VOID) fclose(mailfp);
		(VOID) remove(mailfile);	/* Ignore failure */
	}
}


/*
 * Store a line of mail text for future use.
 *
 * Returns:
 *	TRUE		line stored OK
 *	FALSE		line storage failed
 *
 */

BOOL mail_store(PUCHAR buf)
{	/* First line is treated specially. The first four characters
	   (usually "MAIL") are replaced by "TEMP", the original contents
	   being saved for replacement when the mail file is closed and
	   committed for transmission. Partial files thus look illegal
	   to the transmission software, and will not be processed. */

	if(first_line_seen == FALSE) {
#ifdef	DEBUG
		if(strlen(buf) < PATCHSIZE) {
			fprintf(stderr, "first mail line too short\n");
			abort();
		}
#endif
		strncpy(save_temp, buf, PATCHSIZE);
		strncpy(buf, temp, PATCHSIZE);
		first_line_seen = TRUE;
	}

	if(fputs(buf, mailfp) == EOF) return(FALSE);

	return(TRUE);
}

/*
 * End of file: mailstor.c
 *
 */
