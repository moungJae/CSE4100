/*
 * echoclient.c - An echo client
 */
/* $begin echoclientmain */
#include "csapp.h"

int main(int argc, char **argv) 
{
    int clientfd;
    char *host, *port, buf[MAXLINE], tmp[3];
    rio_t rio;

    if (argc != 3) {
	fprintf(stderr, "usage: %s <host> <port>\n", argv[0]);
	exit(0);
    }
    host = argv[1];
    port = argv[2];

    clientfd = Open_clientfd(host, port);
    Rio_readinitb(&rio, clientfd);
	srand((unsigned int) getpid());

	for (int i = 0; i < 10; i++) {
    	int option = rand() % 3;
		
		if (option == 0) 
			strcpy(buf, "show\n");
		else if (option == 1) {
			int list_num = rand() % 5 + 1;
			int num_to_buy = rand() % 5 + 1;
			
			strcpy(buf, "buy ");
			sprintf(tmp, "%d", list_num);
			strcat(buf, tmp);
			strcat(buf, " ");
			sprintf(tmp, "%d", num_to_buy);
			strcat(buf, tmp);
			strcat(buf, "\n");
		}
		else if (option == 2) {
			int list_num = rand() % 5 + 1;
			int num_to_sell = rand() % 5 + 1;

			strcpy(buf, "sell ");
			sprintf(tmp, "%d", list_num);
			strcat(buf, tmp);
			strcat(buf, " ");
			sprintf(tmp, "%d", num_to_sell);
			strcat(buf, tmp);
			strcat(buf, "\n");
		}

		Fputs(buf, stdout);
		Rio_writen(clientfd, buf, strlen(buf));
		Rio_readnb(&rio, buf, MAXLINE);
		Fputs(buf, stdout);
	}
	Close(clientfd); //line:netp:echoclient:close
    exit(0);
}
/* $end echoclientmain */
