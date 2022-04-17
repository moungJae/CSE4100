/* $begin shellmain */
#include "myshell.h"
#include<errno.h>
#define MAXARGS   128

/* Function prototypes */
void eval(char* cmdline);
int parseline(char* buf, char** argv);
int builtin_command(char** argv);
void operate_pipe(char*** cmd, char* cmdline);
char*** set_command(char** argv);
void free_command(char*** cmd);

int main()
{
	char cmdline[MAXLINE]; /* Command line */
	
	while (1) {
		/* Read */
		printf("CSE4100-SP-P#1> ");
		fgets(cmdline, MAXLINE, stdin);
		if (feof(stdin))
			exit(0);

		/* Evaluate */
		eval(cmdline);
	}
}
/* $end shellmain */

void operate_pipe(char*** cmd, char* cmdline)
{
	int p[2];
	int fd;
	int status;
	pid_t pid;

	fd = 0;
	while ((*cmd)[0]) {
		pipe(p);
		if ((pid = fork()) == 0) {
			if ((*(cmd + 1))[0])
				dup2(p[1], 1);
			dup2(fd, 0);
			close(p[0]);
			if (!builtin_command(*cmd)) {
				if (execvp((*cmd)[0], *cmd) < 0) {
					printf("%s: Command not found.\n", (*cmd)[0]);
					exit(0);
				}
			}
			else
				exit(0);
		}
		else {
			if (waitpid(pid, &status, 0) < 0)
				unix_error("waitfg: waitpid error");
			close(p[1]);
			fd = p[0];
			cmd++;
		}
	}
}

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
	else {
		if ((pid = waitpid(pid, &status, WUNTRACED)) < 0) {
			unix_error("waitfg: waitpid error");
		}
	}
}

/* $begin eval */
/* eval - Evaluate a command line */
void eval(char* cmdline)
{
	char*** cmd;
	char* argv[MAXARGS]; /* Argument list execve() */
	char buf[MAXLINE];   /* Holds modified command line */
	int bg;              /* Should the job run in bg or fg? */
	pid_t pid;           /* Process id */

	strcpy(buf, cmdline);
	bg = parseline(buf, argv);

	if (argv[0] == NULL)
		return;   /* Ignore empty lines */

	if (!builtin_command(argv)) {
		cmd = set_command(argv);
		if (cmd[1][0] == NULL)
			operate_one(argv);
		else
			operate_pipe(cmd, cmdline);
		free_command(cmd);
	}
	return;
}

void free_command(char*** cmd)
{
	int i, j;

	for (i = 0; i < MAXARGS; i++) {
		for (j = 0; j < MAXARGS; j++) {
			free(cmd[i][j]);
		}
		free(cmd[i]);
	}
	free(cmd);
}

char*** set_command(char** argv)
{
	int i, j, k;
	char*** cmd;
	char* tmp;

	cmd = (char***)malloc(sizeof(char**) * MAXARGS);
	for (i = 0; i < MAXARGS; i++) {
		cmd[i] = (char**)malloc(sizeof(char*) * MAXARGS);
		for (j = 0; j < MAXARGS; j++) {
			cmd[i][j] = (char*)malloc(sizeof(char) * MAXARGS);
		}
	}

	i = j = 0;
	while (*argv) {
		tmp = *argv;
		k = 0;
		if (strchr(tmp, '|')) {
			if (*tmp == '|') {
				cmd[i][j] = 0;
				i++, j = 0;
				argv++;
				if (*(tmp + 1)) {
					tmp++;
					while (*tmp) {
						if (*tmp == '\'' || *tmp == '\"') {
							tmp++;
							continue;
						}
						cmd[i][j][k] = *tmp;
						k++, tmp++;
					}
					cmd[i][j][k] = '\0';
					j++;
				}
			}
			else {
				while (*tmp != '|') {
					if (*tmp == '\'' || *tmp == '\"') {
						tmp++;
						continue;
					}
					cmd[i][j][k] = *tmp;
					k++, tmp++;
				}
				cmd[i][j][k] = '\0';
				cmd[i][j + 1] = 0;
				i++, j = 0;
				if (*(tmp + 1))* argv = tmp + 1;
				else argv++;
			}
		}
		else {
			while (*tmp) {
				if (*tmp == '\'' || *tmp == '\"') {
					tmp++;
					continue;
				}
				cmd[i][j][k] = *tmp;
				k++, tmp++;
			}
			cmd[i][j][k] = '\0';
			j++;
			argv++;
		}
	}
	cmd[i][j] = 0;
	cmd[i + 1][0] = 0;
	return cmd;
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
		else
			chdir(getenv("HOME"));
		return 1;
	}
	return 0;                     /* Not a builtin command */
}
/* $end eval */

/* $begin parseline */
/* parseline - Parse the command line and build the argv array */
int parseline(char* buf, char** argv)
{
	char* tmp;
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

	tmp = argv[argc - 1];
	if (tmp[strlen(tmp) - 1] == '&' || *argv[argc - 1] == '&')
		bg = 1;
	else
		bg = 0;

	return bg;
}
/* $end parseline */

