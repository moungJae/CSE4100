/* $begin shellmain */
#include "myshell.h"
#include<errno.h>
#define MAXARGS   128

/* Function prototypes */
void eval(char* cmdline);
int parseline(char* buf, char** argv);
int builtin_command(char** argv);
void set_command(char *cmd[][MAXARGS], char** argv);
void set_cmdline(char* cmdline, char** argv, int bg);
int check_pipe(char* cmdline);

// ***************Job Condition******************
//                                              *
// FG   : Execute Foreground Process            *
// BG   : Execute Background Process            *
// DIE  : Process Die (kill)                    *
// STOP : Process Stop (FG ctrl+Z)              *
// DONE : Finish Background Process             *
// SINT : Signal SIGINT (FG ctrl+C)             *
//                                              *
// **********************************************

const int FG = 0, BG = 1, DIE = 2, STOP = 3, DONE = 4, SINT = 5;
const char* COND_MESSAGE[6] = { NULL, "running\t", "terminated", "suspended\t", "done\t", NULL };
char dummy[MAXARGS * MAXARGS][MAXARGS];

typedef struct job {
	struct job* back;
	struct job* next;
	pid_t pid;
	int cond;
	int fin_num;
	int num;
	int count;
	int stop_count;
	char* cmd;
} Job;

Job* jobs;

void cmd_change(char* cmd, int cond)
{
	int i;

	i = strlen(cmd);
	if (cond == BG) {
		cmd[i] = ' ', cmd[i + 1] = '&', cmd[i + 2] = '\0';
	}
	else {
		if (strchr(cmd, '&')) {
			cmd[i - 2] = '\0';
		}
	}
}

void free_all_jobs(void)
{
	Job* cur, * before;

	cur = jobs;
	while (cur) {
		before = cur;
		cur = cur->next;
		if (cur && cur->cond != DIE && cur->cond != DONE && cur->cond != SINT)
			kill(-(cur->pid), SIGINT);
		if (before->cmd) 
			free(before->cmd);
		free(before);
	}
}

void add_job(pid_t pid, int cond, char* cmd)
{
	Job* job;

	job = (Job*)malloc(sizeof(Job));
	job->cond = cond;
	if (jobs->back)
		job->num = jobs->back->num + 1;
	else
		job->num = 1;
	job->pid = pid;
	job->cmd = cmd;

	if (jobs->count == 0) {
		jobs->back = job;
		jobs->next = job;
	}
	else {
		jobs->back->next = job;
		jobs->back = job;
	}

	if (cond == STOP)
		jobs->stop_count += 1;
	jobs->count += 1;
	jobs->fin_num = job->num;
}

void delete_job(pid_t pid)
{
	Job* cur, * before;

	cur = jobs;
	while (cur->next) {
		before = cur;
		cur = cur->next;
		if (cur->pid == pid) {
			if (cur->cond == STOP)
				jobs->stop_count -= 1;
			if (jobs->next == cur && jobs->back == cur) {
				jobs->next = jobs->back = 0;
			}
			else if (jobs->next == cur) {
				jobs->next = cur->next;
			}
			else if (jobs->back == cur) {
				jobs->back = before;
				before->next = 0;
			}
			else {
				before->next = cur->next;
			}
			free(cur->cmd);
			free(cur);
			jobs->count -= 1;
			break;
		}
	}
}

void clear_jobs(int op)
{
	Job* cur, * before;

	cur = jobs;
	while (cur->next) {
		before = cur;
		cur = cur->next;
		if (cur->cond == DONE || cur->cond == DIE || cur->cond == SINT) {
			cmd_change(cur->cmd, DONE);
			if (op) {
				if (cur->cond == DONE)
					printf("[%d]   Done                             %s\n", cur->num, cur->cmd);
				if (cur->cond == DIE)
					printf("[%d]   Terminated                       %s\n", cur->num, cur->cmd);
			}
			if (jobs->next == cur && jobs->back == cur) {
				jobs->next = jobs->back = 0;
			}
			else if (jobs->next == cur) {
				jobs->next = cur->next;
			}
			else if (jobs->back == cur) {
				jobs->back = before;
				before->next = 0;
			}
			else {
				before->next = cur->next;
			}
			free(cur->cmd);
			free(cur);
			jobs->count -= 1;
			cur = before;
		}
	}
}

void set_done_job(pid_t pid)
{
	Job* cur;

	cur = jobs;
	while (cur->next) {
		cur = cur->next;
		if (cur->pid == pid) {
			cur->cond = DONE;
			break;
		}
	}
}

void print_all_jobs(Job* job, int stop_count)
{
	if (job->cond == DONE) {
		char* delim;

		if (delim = strchr(job->cmd, '&'))
			* delim = '\0';
	}

	if (jobs->back == job) {
		if (job->cond == STOP)
			printf("[%d]+  %s                       %s\n", job->num, COND_MESSAGE[job->cond], job->cmd);
		else {
			if (jobs->stop_count >= 2)
				printf("[%d]   %s                       %s\n", job->num, COND_MESSAGE[job->cond], job->cmd);
			else if (jobs->stop_count == 1)
				printf("[%d]-  %s                       %s\n", job->num, COND_MESSAGE[job->cond], job->cmd);
			else if (jobs->stop_count == 0)
				printf("[%d]+  %s                       %s\n", job->num, COND_MESSAGE[job->cond], job->cmd);
		}
		return;
	}

	if (job->cond == STOP) {
		if (stop_count + 1 == jobs->stop_count)
			printf("[%d]+  %s                       %s\n", job->num, COND_MESSAGE[job->cond], job->cmd);
		else if (stop_count + 1 == jobs->stop_count - 1)
			printf("[%d]-  %s                       %s\n", job->num, COND_MESSAGE[job->cond], job->cmd);
		else
			printf("[%d]   %s                       %s\n", job->num, COND_MESSAGE[job->cond], job->cmd);
		print_all_jobs(job->next, stop_count + 1);
	}
	else {
		if (jobs->stop_count == 0) {
			if (job->next == jobs->back)
				printf("[%d]-  %s                       %s\n", job->num, COND_MESSAGE[job->cond], job->cmd);
			else
				printf("[%d]   %s                       %s\n", job->num, COND_MESSAGE[job->cond], job->cmd);
		}
		else if (jobs->stop_count == 1) {
			if (job->next == jobs->back && stop_count == 0)
				printf("[%d]-  %s                       %s\n", job->num, COND_MESSAGE[job->cond], job->cmd);
			else
				printf("[%d]   %s                       %s\n", job->num, COND_MESSAGE[job->cond], job->cmd);

		}
		else if (jobs->stop_count >= 2) {
			printf("[%d]   %s                       %s\n", job->num, COND_MESSAGE[job->cond], job->cmd);
		}
		print_all_jobs(job->next, stop_count);
	}
}

void sigchld_handler(int sig)
{
	pid_t pid;
	int status;

	while ((pid = waitpid(-1, &status, WNOHANG | WCONTINUED)) > 0) {
		if (WIFEXITED(status) || WIFSIGNALED(status)) {
			set_done_job(pid);
		}
	}
}

void sigtstp_handler(int sig)
{
	pid_t pid;
	Job* job = jobs;

	Sio_puts("\n");
	while (job->next) {
		job = job->next;
		if (job->cond == FG) {
			jobs->stop_count += 1;
			cmd_change(job->cmd, STOP);
			job->cond = STOP;
			pid = job->pid;
			kill(-pid, SIGTSTP);
			printf("[%d]+ Stopped                           %s\n", job->num, job->cmd);
			return;
		}
	}
}

void sigint_handler(int sig)
{
	pid_t pid;
	Job* job = jobs;

	Sio_puts("\n");
	while (job->next) {
		job = job->next;
		if (job->cond == FG) {
			pid = job->pid;
			job->cond = SINT;
			kill(-pid, SIGINT);
			break;
		}
	}
}

int main()
{
	char cmdline[MAXLINE]; /* Command line */

	jobs = (Job*)malloc(sizeof(Job));
	jobs->back = jobs->next = 0;
	jobs->cmd = 0;
	jobs->count = jobs->stop_count = 0;

	Signal(SIGCHLD, sigchld_handler);
	Signal(SIGTSTP, sigtstp_handler);
	Signal(SIGINT, sigint_handler);

	while (1) {
		/* Read */
		printf("CSE4100-SP-P#1> ");
		fgets(cmdline, MAXLINE, stdin);
		if (feof(stdin))
			exit(0);

		/* Evaluate */
		eval(cmdline);
		clear_jobs(1);
	}
}
/* $end shellmain */

void operate(char *cmd[][MAXARGS], int fd, int depth)
{
	int p[2];
	int status;
	pid_t pid;

	if (cmd[depth + 1][0] == 0) {
		dup2(fd, 0);
		close(fd);
		if (!builtin_command(cmd[depth])) {
			if (execvp(cmd[depth][0], cmd[depth]) < 0) {
				printf("%s: Command not found. \n", cmd[depth][0]);
				exit(0);
			}
		}
		else
			exit(0);
	}
	else {
		if (pipe(p) != -1) {
			if ((pid = fork()) == 0) {
				close(p[0]);
				dup2(fd, 0);
				close(fd);
				dup2(p[1], 1);
				close(p[1]);
				if (!builtin_command(cmd[depth])) {
					if (execvp(cmd[depth][0], cmd[depth]) < 0) {
						printf("%s: Command not found.\n", cmd[depth][0]);
						exit(0);
					}
				}
				else exit(0);
			}
			else {
				close(p[1]);
				operate(cmd, p[0], depth + 1);
				if (waitpid(pid, &status, 0) < 0)
					unix_error("waitfg: waitpid error");
			}
		}
		else
			exit(0);
	}
}

void operate_one(char** cmd)
{
	if (execvp(cmd[0], cmd) < 0) {
		printf("%s: Command not found. \n", cmd[0]);
		exit(0);
	}
}

int check_grouping(char **argv, char *(cmd[][MAXARGS]), int pipe)
{
	if (pipe) {
		for (int i = 0; cmd[i][0]; i++) {
			if (!strcmp(cmd[i][0], "cat") && !cmd[i][1])
				return 0;
			else if (!strcmp(cmd[i][0], "grep") && ((i == 0 && cmd[i][1]) || (i >= 1 && !cmd[i][1])))
				return 0;
			else if (!strcmp(cmd[i][0], "less") && ((i == 0 && cmd[i][1]) || (i >= 1 && !cmd[i][1])))
				return 0;
		}
		return 1;
	}
	else {
		if (!strcmp(argv[0], "cat") && !argv[1])
			return 0;
		else if (!strcmp(argv[0], "grep") && argv[1])
			return 0;
		else if (!strcmp(argv[0], "less") && argv[1])
			return 0;
		else 
			return 1;
	}
}

/* $begin eval */
/* eval - Evaluate a command line */
void eval(char* cmdline)
{
	char* cmd[MAXARGS][MAXARGS];
	char* copyline;
	char* argv[MAXARGS]; /* Argument list execve() */
	char buf[MAXLINE];   /* Holds modified command line */
	int bg;              /* Should the job run in bg or fg? */
	int status;
	pid_t pid;           /* Process id */

	strcpy(buf, cmdline);
	bg = parseline(buf, argv);
	copyline = strdup(cmdline);
	set_cmdline(copyline, argv, bg);

	if (argv[0] == NULL)
		return;   /* Ignore empty lines */

	if (builtin_command(argv))
		return;

	if (bg) {
		if ((pid = fork()) == 0) {
			set_command(cmd, argv);
			if (cmd[1][0] != 0) {
				if (check_grouping(argv, cmd, 1))
					setpgrp();
				operate(cmd, 0, 0);
			}
			else {
				if (check_grouping(argv, cmd, 0)) 
					setpgrp();
				operate_one(argv);
			}
		}
		else {
			add_job(pid, BG, copyline);
		}
	}
	else {
		if ((pid = fork()) == 0) {
			set_command(cmd, argv);
			if (cmd[1][0] != 0) {
				if (check_grouping(argv, cmd, 1))
					setpgrp();
				operate(cmd, 0, 0);
			}
			else {
				if (check_grouping(argv, cmd, 0))
					setpgrp();
				operate_one(argv);
			}
		}

		add_job(pid, FG, copyline);
		if (waitpid(pid, &status, WUNTRACED) < 0) {
			unix_error("waitfg: waitpid error");
		}
		else {
			if (WIFEXITED(status)) {
				delete_job(pid);
			}
		}
	}
	return;
}

void set_cmdline(char* cmdline, char** argv, int bg)
{
	int i;
	int j;
	char* tmp;
	char buf[MAXLINE];

	i = 0;
	while (*argv) {
		tmp = *argv;
		while (*tmp) {
			cmdline[i++] = *tmp;
			tmp++;
		}
		cmdline[i++] = ' ';
		argv++;
	}
	i -= 1;
	cmdline[i] = '\0';
	if (bg)
		cmdline[i] = ' ', cmdline[i + 1] = '&', cmdline[i + 2] = '\0';

	i = j = 0;
	while (cmdline[i]) {
		if (cmdline[i] == '|') {
			if (i - 1 >= 0 && cmdline[i - 1] != ' ') {
				buf[j++] = ' ';
				buf[j++] = '|';
			}
			else
				buf[j++] = '|';
			if (cmdline[i + 1] != ' ')
				buf[j++] = ' ';
			i++;
		}
		else {
			buf[j] = cmdline[i];
			j++, i++;
		}
	}
	buf[j] = '\0';
	strcpy(cmdline, buf);
}

void set_command(char *cmd[][MAXARGS], char** argv)
{
	int i, j, k, idx;
	char* tmp;

	i = j = idx = 0;
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
						if (*tmp == '\"') {
							tmp++;
							continue;
						}
						dummy[idx][k] = *tmp;
						k++, tmp++;
					}
					dummy[idx][k] = '\0';
					cmd[i][j] = dummy[idx];
					idx++, j++;
				}
			}
			else {
				while (*tmp != '|') {
					if (*tmp == '\"') {
						tmp++;
						continue;
					}
					dummy[idx][k] = *tmp;
					k++, tmp++;
				}
				dummy[idx][k] = '\0';
				cmd[i][j] = dummy[idx];
				cmd[i][j + 1] = 0;
				idx++, i++, j = 0;
				if (*(tmp + 1)) *argv = tmp + 1;
				else argv++;
			}
		}
		else {
			while (*tmp) {
				if (*tmp == '\"') {
					tmp++;
					continue;
				}
				dummy[idx][k] = *tmp;
				k++, tmp++;
			}
			dummy[idx][k] = '\0';
			cmd[i][j] = dummy[idx];
			idx++, j++;
			argv++;
		}
	}
	cmd[i][j] = 0;
	cmd[i + 1][0] = 0;
}

/* If first arg is a builtin command, run it and return true */
int builtin_command(char** argv)
{
	Job* job;
	int job_num;
	int check;
	pid_t pid;

	if (!strcmp(argv[0], "exit")) {
		free_all_jobs();
		exit(0);
	}
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
	if (!strcmp(argv[0], "jobs")) {
		if (jobs->next) {
			print_all_jobs(jobs->next, 0);
			clear_jobs(0);
		}
		return 1;
	}
	if (!strcmp(argv[0], "bg")) {
		if (argv[1][0] == '%' && (job_num = atoi(argv[1] + 1)) > 0) {
			job = jobs;
			check = 0;
			while (job->next) {
				job = job->next;
				if (job->num == job_num) {
					if (job->cond == BG) {
						printf("-bash: bg: job %d already in background\n", job_num);
						return 1;
					}
					check = 1;
					jobs->stop_count -= 1;
					cmd_change(job->cmd, BG);
					job->cond = BG;
					pid = job->pid;
					kill(pid, SIGCONT);
					break;
				}
			}
			if (!check)
				printf("-bash: bg: %%%d: no such job\n", job_num);
		}
		return 1;
	}
	if (!strcmp(argv[0], "fg")) {
		if (argv[1][0] == '%' && (job_num = atoi(argv[1] + 1)) > 0) {
			job = jobs;
			check = 0;
			while (job->next) {
				job = job->next;
				if (job->num == job_num) {
					check = 1;
					if (job->cond == STOP)
						jobs->stop_count -= 1;
					cmd_change(job->cmd, FG);
					job->cond = FG;
					pid = job->pid;
					kill(pid, SIGCONT);
					break;
				}
			}
			if (check) {
				while (job->cond == FG);
			}
			else
				printf("-bash: fg: %%%d: no such job\n", job_num);
		}
		return 1;
	}
	if (!strcmp(argv[0], "kill")) {
		if (argv[1][0] == '%' && (job_num = atoi(argv[1] + 1)) > 0) {
			job = jobs;
			check = 0;
			while (job->next) {
				job = job->next;
				if (job->num == job_num) {
					check = 1;
					if (job->cond == STOP)
						jobs->stop_count -= 1;
					job->cond = DIE;
					pid = job->pid;
					kill(-pid, SIGINT);
					break;
				}
			}
			if (!check)
				printf("-bash: kill: %%%d: no such job\n", job_num);
		}
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
			delim = strchr(buf + 1, '\'');
			delim = strchr(delim, ' ');
		}
		else if (*buf == '\"') {
			delim = strchr(buf + 1, '\"');
			delim = strchr(delim, ' ');
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
	if (*argv[argc - 1] == '&')
		argv[argc - 1] = NULL, bg = 1;
	else if (tmp[strlen(tmp) - 1] == '&')
		tmp[strlen(tmp) - 1] = '\0', bg = 1;
	else
		bg = 0;

	return bg;
}
/* $end parseline */
