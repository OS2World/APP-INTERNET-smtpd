/*
 * File: smtpd.h
 *
 * SMTP daemon for receiving mail on Tavi network; to be invoked only
 * by INETD.
 *
 * Header file
 *
 * Bob Eager   August 2003
 *
 */

#define INCL_DOSFILEMGR
#include <os2.h>

#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

#define OS2
#include <types.h>
#include <sys\ioctl.h>
#include <sys\socket.h>
#include <netdb.h>
#include <net\if.h>
#include <netinet\in.h>
#include <arpa\nameser.h>
#include <resolv.h>

#include "log.h"

#define VERSION                 4       /* Major version number */
#define EDIT                    1       /* Edit number within major version */

#define FALSE                   0
#define TRUE                    1

/* Configuration constants */

#define MAXADDR                 16      /* Size of buffer to hold dotted IP address */

/* Type definitions */

typedef struct hostent          HOST, *PHOST;           /* Host structure */
typedef struct ifreq            IFREQ, *PIFREQ;         /* Interface information */
typedef struct in_addr          INADDR, *PINADDR;       /* Internet address */
typedef	struct servent		SERV, *PSERV;		/* Service structure */
typedef struct sockaddr         SOCKG, *PSOCKG;         /* Generic structure */
typedef struct sockaddr_in      SOCK, *PSOCK;           /* Internet structure */

/* Structure definitions */

typedef struct _INADDRENT {             /* Interface address list entry */
struct _INADDRENT       *next;
INADDR                  interface_address;
INADDR                  interface_mask;
} INADDRENT, *PINADDRENT;

typedef struct _CONFIG {                /* Configuration information */
INT             nthosts;                /* Number of trusted hosts */
PINADDRENT      thost_list;             /* Trusted host list */
LOGTYPE		log_type;		/* Type of logging */
} CONFIG, *PCONFIG;

/* External references */

extern  VOID    error(PUCHAR, ...);
extern  INT     read_config(PUCHAR, PUCHAR, PCONFIG);
extern  BOOL    server(INT, PUCHAR, PUCHAR, PUCHAR, PUCHAR);

/*
 * End of file: smtpd.h
 *
 */

