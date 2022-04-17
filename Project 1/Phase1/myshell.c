/* $begin shellmain */
#include "myshell.h"
#include<errno.h>
#define MAXARGS   128

/* Function prototypes */
void eval(char* cmdline);
int parseline(char* buf, char** argv);
int builtin_command(char** argv);
void operate_command(char** cmd, char* cmdline, int bg);

pid_t g_pid, main_pid;

void sigint_handler(int sig)
{
	if (g_pid != main_pid) {
		kill(g_pid, SIGINT);
		Sio_puts("\n");
	}
	else {
		Sio_puts("\nCSE4100-SP-P#1> ");
	}
}

void sigtstp_handler(int sig) {
	kill(g_pid, SIGTSTP);
	Sio_puts("\n");
}

int main()
{
	char cmdline[MAXLINE]; /* Command line */

	Signal(SIGTSTP, sigtstp_handler);
	Signal(SIGINT, sigint_handler);
	main_pid = getpid();

	while (1) {
		/* Read */
		g_pid = main_pid;
		printf("CSE4100-SP-P#1> ");
		fgets(cmdline, MAXLINE, stdin);
		if (feof(stdin))
			exit(0);

		/* Evaluate */
		eval(cmdline);
	}
}
/* $end shellmain */

void operate_one(char** cmd)
{
	int status;
	pid_t pid;

	if ((pid = fork()) == 0) {
		if (execvp(cmd[0], cmd) < 0) {
			printf("%s: Command not found. \n", cmd[0]);
			exit(0);
		}
	}
		g_pid = pid;
		if ((pid = waitpid(pid, &status, WUNTRACED)) < 0) {
			unix_error("waitfg: waitpid error");
		}
}

/* $begin eval */
/* eval - Evaluate a command line */
void eval(char* cmdline)
{
	char* argv[MAXARGS]; /* Argument list execve() */
	char buf[MAXLINE];   /* Holds modified command line */
	int bg;              /* Should the job run in bg or fg? */
	pid_t pid;           /* Process id */

	strcpy(buf, cmdline);
	bg = parseline(buf, argv);

	if (argv[0] == NULL)
		return;   /* Ignore empty lines */

	if (!builtin_command(argv))
		operate_one(argv);
	return;
}

/* If first arg is a builtin command, run it and return true */
int builtin_command(char** argv)
{
	if (!strcmp(argv[0], "exit")) /* quit command */
		exit(0);
	if (!strcmp(argv[0], "&"))    /* Ignore singleton & */
		return 1;
	if (!strcmp(argv[0], "cd")) {
		if (argv[1]) {
			if (chdir(argv[1]))
				printf("-bash: cd: %s: No such file or directory\n", argv[1]);
		}
		else chdir(getenv("HOME"));
		return 1;
	}
	return 0;                     /* Not a builtin command */
}
/* $end eval */

/* $begin parseline */
/* parseline - Parse the command line and build the argv array */
int parseline(char* buf, char** argv)
{
	char* delim;         /* Points to first space delimiter */
	int argc;            /* Number of args */
	int bg;              /* Background job? */

	buf[strlen(buf) - 1] = ' ';  /* Replace trailing '\n' with space */
	while (*buf && (*buf == ' ')) /* Ignore leading spaces */
		buf++;

	/* Build the argv list */
	argc = 0;
	while ((delim = strchr(buf, ' '))) {
		if (*buf == '\'') {
			buf++;
			delim = strchr(buf, '\'');
		}
		else if (*buf == '\"') {
			buf++;
			delim = strchr(buf, '\"');
		}
		argv[argc++] = buf;
		*delim = '\0';
		buf = delim + 1;
		while (*buf && (*buf == ' ')) /* Ignore spaces */
			buf++;
	}
	argv[argc] = NULL;

	if (argc == 0)  /* Ignore blank line */
		return 1;

	/* Should the job run in the background? */
	if ((bg = (*argv[argc - 1] == '&')) != 0)
		argv[--argc] = NULL;

	return bg;
}
/* $end parseline */

