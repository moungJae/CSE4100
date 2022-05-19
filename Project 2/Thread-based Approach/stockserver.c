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
	int readcnt;
	sem_t mutex;
} Item;

#define NTHREADS 1024
#define SBUFSIZE 1024

void add_item(Item item);
void show_item(Item* item, char *item_buf, int *idx, int connfd);
int make_buf(char buf[], Item* item);
void buy_item(Item* item, int connfd, int id, int stock);
void sell_item(Item* item, int connfd, int id, int stock);
void free_item(Item* item, FILE* pFile);
void close_client(int connfd);
void set_item();
void get_stock(char buf[], int* id, int* stock);
void* thread(void* vargp);

Item* items;
sem_t mutex;
sbuf_t sbuf;
int thread_cnt;

void sigint_handler(int sig)
{
	FILE* pFile;

	P(&mutex);
	sbuf_deinit(&sbuf);
	if (items) {
		pFile = fopen("stock.txt", "w");
		free_item(items, pFile);
		items = 0;
		fclose(pFile);
	}
	V(&mutex);

	exit(0);
}

int main(int argc, char** argv)
{
	int listenfd, connfd;
	int tid_idx;
	socklen_t clientlen;
	struct sockaddr_storage clientaddr;  /* Enough space for any address */  //line:netp:echoserveri:sockaddrstorage
	FILE* pFile;
	char client_hostname[MAXLINE], client_port[MAXLINE];
	pthread_t tid[NTHREADS];

	if (argc != 2) {
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(0);
	}

	signal(SIGINT, sigint_handler);
	listenfd = Open_listenfd(argv[1]);
	sbuf_init(&sbuf, SBUFSIZE);
	Sem_init(&mutex, 0, 1);
	items = 0;
	thread_cnt = tid_idx = 0;

	while (1) {
		clientlen = sizeof(struct sockaddr_storage);
		connfd = Accept(listenfd, (SA*)& clientaddr, &clientlen);
		P(&mutex);
		Getnameinfo((SA*)& clientaddr, clientlen, client_hostname, MAXLINE,
			client_port, MAXLINE, 0);
		sbuf_insert(&sbuf, connfd);
		V(&mutex);
		Pthread_create(&tid[tid_idx], NULL, thread, NULL);
		tid_idx = (tid_idx + 1) % NTHREADS;
		printf("\033[0;32mConnected\033[0m to (%s, %s)\n", client_hostname, client_port);
	}

	P(&mutex);
	sbuf_deinit(&sbuf);
	if (items) {
		pFile = fopen("stock.txt", "w");
		free_item(items, pFile);
		items = 0;
		fclose(pFile);
	}
	V(&mutex);

	exit(0);
}

void* thread(void* vargp)
{
	int n, connfd, idx;
	int id, stock;
	char buf[MAXLINE];
	char item_buf[MAXLINE];
	rio_t rio;

	Pthread_detach(pthread_self());
	
	P(&mutex);
	set_item();
	connfd = sbuf_remove(&sbuf);
	V(&mutex);
	
	Rio_readinitb(&rio, connfd);
	while (1) {
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
				close_client(connfd);
				return NULL;
			}
		}
		else {
			close_client(connfd);
			return NULL;
		}
	}
}

void set_item()
{
	FILE* pFile;
	Item item;
	int id, left_stock, price;

	thread_cnt++;
	if (!items) {
		pFile = fopen("stock.txt", "r");
		while (fscanf(pFile, "%d %d %d", &id, &left_stock, &price) != EOF) {
			item.ID = id, item.left_stock = left_stock, item.price = price;
			add_item(item);
		}
		fclose(pFile);
	}
}

void close_client(int connfd)
{
	FILE* pFile;

	P(&mutex);
	Close(connfd);
	thread_cnt--;
	if (thread_cnt == 0) {
		pFile = fopen("stock.txt", "w");
		free_item(items, pFile);
		items = 0;
		fclose(pFile);
	}
	V(&mutex);
}

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
	cur_item->price = item.price, cur_item->readcnt = 0;
	Sem_init(&cur_item->mutex, 0, 1);
	cur_item->left_item = cur_item->right_item = NULL;
}

int make_buf(char buf[], Item* item)
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
	return idx;
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

		P(&mutex);
		item->readcnt++;
		if (item->readcnt == 1)
			P(&item->mutex);
		V(&mutex);

		make_buf(buf, item);
		move_buf(item_buf, buf, idx);

		P(&mutex);
		item->readcnt--;
		if (item->readcnt == 0)
			V(&item->mutex);
		V(&mutex);

		show_item(item->right_item, item_buf, idx, connfd);
	}
}

void buy_item(Item* item, int connfd, int id, int stock)
{
	if (!item)
		return ;

	if (item->ID == id) {
		char success_message[40] = "[buy] \033[0;32msuccess\033[0m\n";
		char fail_message[30] = "Not enough left stock\n";

		success_message[strlen(success_message)] = '\0';
		fail_message[strlen(fail_message)] = '\0';
		P(&item->mutex);
		if (item->left_stock < stock) {
			Rio_writen(connfd, fail_message, MAXLINE);
		}
		else {
			item->left_stock -= stock;
			Rio_writen(connfd, success_message, MAXLINE);
		}
		V(&item->mutex);
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

		P(&item->mutex);
		item->left_stock += stock;
		Rio_writen(connfd, success_message, MAXLINE);
		V(&item->mutex);
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
/* $end echoserverimain */
