SMTPD - a simple SMTP daemon for OS/2
-------------------------------------

This is a very simple and basic SMTP server daemon, which will accept
connections on the usual SMTP port (25) and collect mail for onward
transmission.  It is not sophisticated, but it is small and fast. 
Facilities are also provided for simple vetting of outgoing mail from
selected systems, e.g.  a child's machine. 

All companion programs are available from the same place:

     http://www.tavi.co.uk/os2pages/mail.html

Installation
------------

Copy the SMTPD.EXE file to any suitable directory which is named in the
system PATH. Copy the NETLIB.DLL file to any directory on the LIBPATH.

Configuration
-------------

First, ensure that you have a line in CONFIG.SYS of the form:

     SET TZ=....

This defines your time zone setting, names, and daylight saving rules.
If you don't have one, you need to add it; the actual value can be quite
complex.  For the United Kingdom, the line is:

     SET TZ=GMT0BST,3,-1,0,3600,10,-1,0,7200,3600

For other areas, download the TZCALC utility from the same place as this
program.  This will work out the correct setting for you.  It will be
necessary to reboot in order to pick up this setting, but wait until you
have completed the rest of these instructions.

Now edit CONFIG.SYS, adding a new line of the form:

     SET SMTP=directoryname

where 'directoryname' is the name of a directory (which must exist)
where outgoing mail is to be stored.  No other files should be kept in
this directory.  It will be necessary to reboot in order to pick up this
setting, but wait until you have completed the rest of these
instructions.

Locate the directory described by the ETC environment variable.  If you
are not sure, type the command:

     SET ETC

at a command prompt.  In this directory, create a configuration file,
which must be named MAIL.CNF.  A sample is provided, as SAMPLE.CNF.
The TRUSTED_HOST line may be included as many times as necessary; if you
are not worried about blocking SMTP calls from anywhere, just use the
line:

     TRUSTED_HOST    0.0.0.0    0.0.0.0

If, however, you don't want to become a spam conduit, specify the IP
addresses from which you are prepared to accept mail.  You can have
multiple TRUSTED_HOST lines, each specifying a single IP address, or you
can specify a whole group by using the mask effectively.  The check is
done by comparing a calling IP address with the one given as the first
value after TRUSTED_ADDRESS; however, before the comparison, both values
are masked with the second address.  So, for example, if your local
network was 192.168.55.0, you could use the line:

     TRUSTED_HOST 192.168.55.0 255.255.255.0

which would accept calls from anywhere on that network (192.168.55.11,
192.168.55.77, etc...).  All TRUSTED_HOST lines are checked on an
incoming call, and if any one of them provides a match, the call is
accepted.

Optionally, add a line in the configuration file to specify how to do
logging.  By default, log messages are written to the file SMTPD.LOG in
the same directory as the configuration file.  This can be specified
explicitly with the line:

     LOGGING   FILE

As an alternative, the line:

     LOGGING   SYSLOG

will cause log messages to be sent to a SYSLOG daemon, if one is
running.  This is particularly useful if the SMTP server is likely to be
used heavily, as logging to a file can cause problems if multiple
instances of the server are invoked by different clients (or one
multithreaded client) at the same time.  Note that you will need to
start the SYSLOG daemon unless you already use it; an easy way is to add
the line:

     DETACH x:\TCPIP\BIN\SYSLOGD     (where x: is the boot drive)

to the file x:\TCPIP\BIN\TCPEXIT.CMD (creating it if necessary). 

Lastly, edit the file INETD.LST, also found in the ETC directory.  Add a
line like this:

      smtp tcp smtpd

You can do this via the TCP/IP configuration notebook if you prefer.  If
INETD is not already running, edit the file TCPSTART.CMD (normally found
in \TCPIP\BIN) to un-comment the line that starts INETD; again, use the
TCP/IP configuration notebook if you prefer.  Reboot to activate INETD,
and also to pick up the SMTP environment setting (and the TZ one if you
added it).  INETD will now accept incoming SMTP calls and start SMTPD as
necessary.

Using an alternate port
-----------------------

SMTPD operates on whichever port is specified to INETD; the port is
specified by name (the 'smtp' at the start of the INETD.LST line).  This
is looked up in the SERVICES file in the ETC directory, and is normally
port 25. 

If another port (any port not specified as 'smtp' in the SERVICES file)
is used, then SMTPD uses a different environment variable to determine
its spool directory (the one it uses to store outgoing mail); this
environment variable is named SMTPH, and must be specified in CONFIG.SYS
in the same way as the SMTP environment variable.  A good choice is port
24 (normally used for private mail systems), and the service name must be
'smtph'.  It will be necessary to alter the entries in the SERVICES file
to reflect this; for example:

  smtph            24/tcp    #any private mail system
  smtph            24/udp    #any private mail system

(in fact, the second line isn't needed but is best edited to avoid
confusion). 

Now edit the INETD.LST file, so that SMTPD is started for connections on
either port:

     smtp tcp smtpd
     smtph tcp smtpd

or use the TCP/IP configuration notebook to do the same thing.

Why is this useful? It means that incoming mail on port 25 works as
before, but incoming mail on port 24 is stored in a different directory
where it is not normally transmitted via the SMTP client program.  This
is useful for vetting outgoing mail from (say) a child's machine, and
once vetted the mail can be moved to the usual outgoing directory.  It
is easy to set up a folder of shadows on the desktop to make the process
quick and easy. 

Logfile
-------

SMTPD maintains a logfile called SMTPD.LOG in the \MPTN\ETC directory. 
This will grow without bound if not pruned regularly!

If the LOGGING SYSLOG option is used, then the log information is sent
instead to the SYSLOG daemon, if it is running.  Normally, this sends
the output to the file SYSLOG.MSG in the \MPTN\ETC directory, although
this behaviour can be altered by editing the file \MPTN\ETC\SYSLOG.CNF
(this file contains comments which explain what to edit). 
Unfortunately, SYSLOG.MSG is locked open by the SYSLOG daemon, so to
read its contents it is easiest to use the command sequence:

     TYPE SYSLOG.MSG > Z
     E Z                     (or use any other program to view the file Z)


The spool directory
-------------------

Incoming mail is stored in the spool directory specified by the SMTP
environment variable (or SMTPH if the alternate port is used).  Don't
store anything else in this directory. 

Files have a special (but simple) format; the message is stored "as is",
but preceded by lines specifying sender and recipient(s).  This format
is understood by the SMTP client program (available separately).

Routing
-------

No special routing of mail is done; it is all placed in the spool
directory.  If any more functionality is required, use another program,
such as sendmail (which is roughly a hundred times the size).

Feedback
--------

SMTPD was written by me, Bob Eager. I can be contacted at the address below.

Please let me know if you actually use this program.  Suggestions for
improvements are welcomed.

History
-------

1.0     Initial version.
1.1     First release version.
1.2     Fix drive/directory problem for spool area.
1.3     Further fix for drive/directory problems.
1.4     Use new thread-safe logging module.
        Use OS/2 type definitions.
1.5     New, simplified network interface module.
        Grouped initialisation code together.
1.6     Diagnostics for occasional logfile open failures.
1.7     Added option security logging.
1.8     Added configuration file, and blocking of non-trusted clients.
1.9     Corrected handling of part line comments in config file.
2.0     Changed name of configuration file to MAIL.CNF.
        Added BLDLEVEL string.
        Additional error checking in logging module.
3.0	Recompiled to use 32 bit TCP/IP toolkit, in 4.0 mode.
3.1	Added support for service on alternate port (service 'smtpdh')
	to hold outgoing messages for validation.
	This is enabled by the use of a different mail directory
	defined by the environment variable SMTPH (instead of SMTP),
	and the service 'smtph'.
	Added support for using long filenames on JFS.
	Removed redundant 'addsockettolist' declaration (it is now
	properly defined and documented in the toolkit).
4.0	Added support for logging to 'syslog' instead of to file,
	selectable by LOGGING configuration file option. This has the
	advantage of avoiding clashing logfile usage.
4.1	Changes to conform more closely with RFC 2821.
	Now accept EHLO command as well as HELO, although no actual
	service extensions are currently defined; also give updated
	form of 250 response.
	Tolerate trailing spaces in commands.
	Reject commands with extraneous parameters (DATA, NOOP,
	RSET, QUIT).
	Implemented basic HELP command.
	Accept some commands (EXPN, HELP, NOOP, RSET, VRFY) before
	EHLO/HELO (if implemented).
	Tidied up and corrected some error response codes and text.
	Accept EHLO/HELO at any time, treating as RSET.
	Add client IP to Received: line, also use correct protocol
	name (SMTP or ESMTP) based on EHLO/HELO.
	Changed timezone to offset in Received: line, to comply
	with RFC2821.
	Make timestamps conform to RFC2821/RFC2822 in terms of
	leading zeros and four digit year values.

Bob Eager
rde@tavi.co.uk
August 2003
