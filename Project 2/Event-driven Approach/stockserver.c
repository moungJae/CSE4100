/*
 * echoserveri.c - An iterative echo server
 */
 /* $begin echoserverimain */
#include "csapp.h"

typedef struct item {
	struct item* left_item;
	struct item* right_item;
	int ID;
	int left_stock;
	int price;
} Item;

typedef struct { /* Represents a pool of connected descriptors */ //line:conc:echoservers:beginpool
	int maxfd;        /* Largest descriptor in read_set */
	fd_set read_set;  /* Set of all active descriptors */
	fd_set ready_set; /* Subset of descriptors ready for reading  */
	int nready;       /* Number of ready descriptors from select */
	int maxi;         /* Highwater index into client array */
	int clientfd[FD_SETSIZE];    /* Set of active descriptors */
	rio_t clientrio[FD_SETSIZE]; /* Set of active read buffers */
	int client_cnt;
} pool; //line:conc:echoservers:endpool

/* end echoserversmain */
void init_pool(int listenfd, pool* p);
void add_client(int connfd, pool* p);
void check_clients(pool* p);
/* $begin echoserversmain */

void add_item(Item item);
void make_buf(char buf[], Item* item);
void show_item(Item* item, char item_buf[], int *idx, int connfd);
void buy_item(Item* item, int connfd, int id, int stock);
void sell_item(Item* item, int connfd, int id, int stock);
void free_item(Item* item, FILE* pFile);

Item* items;

void sigint_handler(int sig)
{
	FILE* pFile;

	if (items) {
		pFile = fopen("stock.txt", "w");
		free_item(items, pFile);
		items = 0;
		fclose(pFile);
	}
	exit(0);
}

int main(int argc, char** argv)
{
	int listenfd, connfd;
	int id, left_stock, price;
	socklen_t clientlen;
	struct sockaddr_storage clientaddr;  /* Enough space for any address */  //line:netp:echoserveri:sockaddrstorage
	static pool pool;
	Item item;
	FILE* pFile;
	char client_hostname[MAXLINE], client_port[MAXLINE];

	if (argc != 2) {
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(0);
	}

	signal(SIGINT, sigint_handler);
	listenfd = Open_listenfd(argv[1]);
	init_pool(listenfd, &pool);
	items = 0;

	while (1) {
		pool.ready_set = pool.read_set;
		pool.nready = Select(pool.maxfd + 1, &pool.ready_set, NULL, NULL, NULL);

		if (FD_ISSET(listenfd, &pool.ready_set)) {
			clientlen = sizeof(struct sockaddr_storage);
			connfd = Accept(listenfd, (SA*)& clientaddr, &clientlen);
			Getnameinfo((SA*)& clientaddr, clientlen, client_hostname, MAXLINE,
				client_port, MAXLINE, 0);
			add_client(connfd, &pool);
			if (!items) {
				pFile = fopen("stock.txt", "r");
				while (fscanf(pFile, "%d %d %d", &id, &left_stock, &price) != EOF) {
					item.ID = id, item.left_stock = left_stock, item.price = price;
					add_item(item);
				}
				fclose(pFile);
			}
			printf("\033[0;32mConnected\033[0m to (%s, %s)\n", client_hostname, client_port);
		}

		check_clients(&pool);
	}

	if (items) {
		pFile = fopen("stock.txt", "w");
		free_item(items, pFile);
		items = 0;
		fclose(pFile);
	}
	exit(0);
}
/* $end echoserverimain */

void add_item(Item item)
{
	Item* before_item, * cur_item;

	if (items == NULL) {
		items = (Item*)malloc(sizeof(Item));
		cur_item = items;
	}
	else {
		cur_item = items;
		while (cur_item) {
			before_item = cur_item;
			if (cur_item->ID > item.ID) cur_item = cur_item->left_item;
			else cur_item = cur_item->right_item;
		}
		cur_item = before_item;
		if (cur_item->ID > item.ID) {
			cur_item->left_item = (Item*)malloc(sizeof(Item));
			cur_item = cur_item->left_item;
		}
		else {
			cur_item->right_item = (Item*)malloc(sizeof(Item));
			cur_item = cur_item->right_item;
		}
	}
	cur_item->ID = item.ID, cur_item->left_stock = item.left_stock;
	cur_item->price = item.price;
	cur_item->left_item = cur_item->right_item = NULL;
}

void make_buf(char buf[], Item* item)
{
	char item_buf[3][20];
	char temp;
	int item_type[3] = { item->ID, item->left_stock, item->price };
	int idx;

	for (int i = 0; i < 3; i++) {
		idx = 0;
		if (item_type[i] == 0) {
			item_buf[i][idx++] = '0';
		}
		else {
			while (item_type[i]) {
				item_buf[i][idx++] = (item_type[i] % 10) + '0';
				item_type[i] /= 10;
			}
			for (int j = 0; j < (idx / 2); j++) {
				temp = item_buf[i][j];
				item_buf[i][j] = item_buf[i][idx - j - 1];
				item_buf[i][idx - j - 1] = temp;
			}
		}
		item_buf[i][idx] = '\0';
	}
	idx = 0;
	for (int i = 0; i < 3; i++) {
		for (int j = 0; item_buf[i][j] != '\0'; j++) {
			buf[idx++] = item_buf[i][j];
		}
		if (i != 2) buf[idx++] = ' ';
		else buf[idx++] = '\n';
	}
	buf[idx] = '\0';
}

void move_buf(char *item_buf, char buf[], int *idx)
{
	int i = 0;

	while (buf[i]) {
		item_buf[*idx] = buf[i];
		*idx += 1;
		i++;
	}
}

void show_item(Item* item, char *item_buf, int *idx, int connfd)
{
	char buf[30];

	if (item) {
		show_item(item->left_item, item_buf, idx, connfd);
		make_buf(buf, item);
		move_buf(item_buf, buf, idx);
		show_item(item->right_item, item_buf, idx, connfd);
	}
}

void buy_item(Item* item, int connfd, int id, int stock)
{
	if (!item)
		return ;

	if (item->ID == id) {
		char success_message[40] = "[buy] \033[0;32msuccess\033[0m\n\0";
		char fail_message[30] = "Not enough left stock\n\0";
		if (item->left_stock < stock) {
			Rio_writen(connfd, fail_message, MAXLINE);
		}
		else {
			item->left_stock -= stock;
			Rio_writen(connfd, success_message, MAXLINE);
		}
	}
	else if (item->ID < id) {
		buy_item(item->right_item, connfd, id, stock);
	}
	else {
		buy_item(item->left_item, connfd, id, stock);
	}
}

void sell_item(Item* item, int connfd, int id, int stock)
{
	if (!item)
		return ;

	if (item->ID == id) {
		char success_message[40] = "[sell] \033[0;32msuccess\033[0m\n\0";
		item->left_stock += stock;
		Rio_writen(connfd, success_message, MAXLINE);
	}
	else if (item->ID < id) {
		sell_item(item->right_item, connfd, id, stock);
	}
	else {
		sell_item(item->left_item, connfd, id, stock);
	}
}

void free_item(Item* item, FILE* pFile)
{
	if (item) {
		free_item(item->left_item, pFile);
		fprintf(pFile, "%d %d %d\n", item->ID, item->left_stock, item->price);
		free_item(item->right_item, pFile);
		free(item);
	}
}

void init_pool(int listenfd, pool* p)
{
	int i;

	p->maxi = -1;
	for (i = 0; i < FD_SETSIZE; i++)
		p->clientfd[i] = -1;
	p->maxfd = listenfd;
	p->client_cnt = 0;
	FD_ZERO(&p->read_set);
	FD_SET(listenfd, &p->read_set);
}

void add_client(int connfd, pool* p)
{
	int i;

	p->nready--;
	for (i = 0; i < FD_SETSIZE; i++) {
		if (p->clientfd[i] < 0) {
			p->client_cnt++;
			p->clientfd[i] = connfd;
			Rio_readinitb(&p->clientrio[i], connfd);
			FD_SET(connfd, &p->read_set);
			if (connfd > p->maxfd)
				p->maxfd = connfd;
			if (i > p->maxi)
				p->maxi = i;

			break;
		}
	}
	if (i == FD_SETSIZE)
		app_error("add_client error => Too many clients!!!");
}

void close_client(pool* p, int connfd, int i)
{
	FILE* pFile;

	Close(connfd);
	FD_CLR(connfd, &p->read_set);
	p->clientfd[i] = -1;
	p->client_cnt--;
	if (p->client_cnt == 0) {
		pFile = fopen("stock.txt", "w");
		free_item(items, pFile);
		items = 0;
		fclose(pFile);
	}
}

void check_clients(pool* p)
{
	int i, connfd, n, idx;
	int id, stock;
	char buf[MAXLINE];
	char item_buf[MAXLINE];
	rio_t rio;

	for (i = 0; (i <= p->maxi) && (p->nready > 0); i++) {
		connfd = p->clientfd[i];
		rio = p->clientrio[i];
		if ((connfd > 0) && (FD_ISSET(connfd, &p->ready_set))) {
			p->nready--;
			if ((n = Rio_readlineb(&rio, buf, MAXLINE)) > 0) {
				printf("Server received %d bytes\n", n);
				if (!strncmp("show", buf, 4)) {
					idx = 0;
					show_item(items, item_buf, &idx, connfd);
					item_buf[idx] = '\0';
					Rio_writen(connfd, item_buf, MAXLINE);
				}
				else if (!strncmp("buy", buf, 3)) {
					idx = 4;
					id = atoi(buf + idx);
					while (buf[idx] != ' ')
						idx++;
					stock = atoi(buf + idx);
					buy_item(items, connfd, id, stock);
				}
				else if (!strncmp("sell", buf, 4)) {
					idx = 5;
					id = atoi(buf + idx);
					while (buf[idx] != ' ')
						idx++;
					stock = atoi(buf + idx);
					sell_item(items, connfd, id, stock);
				}
				else if (!strncmp("exit", buf, 4)) {
					close_client(p, connfd, i);
				}
			}
			else {
				close_client(p, connfd, i);
			}
		}
	}
}
