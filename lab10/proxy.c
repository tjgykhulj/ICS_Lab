/*
 * proxy.c - CS:APP Web proxy
 *
 * TEAM MEMBERS:
 *     Andrew Carnegie, ac00@cs.cmu.edu 
 *     Harry Q. Bovik, bovik@cs.cmu.edu
 * 
 * IMPORTANT: Give a high level description of your code here. You
 * must also provide a header comment at the beginning of each
 * function that describes what that function does.
 */ 

#include "csapp.h"

typedef struct {
	int connfd;
	struct sockaddr_in sockaddr;
} mytype;

sem_t sem_log;
sem_t sem_dns;
sem_t sem_thr;
FILE *file;
/*
 * Function prototypes
 */
int parse_uri(char *uri, char *target_addr, char *path, int  *port);
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, int size);
void do_work(int connfd, struct sockaddr_in *sockaddr);
int open_clientfd_ts(char *hostname, int port);
void *thread(void *arg) 
{
	mytype tmp = *(mytype *) arg;
	free(arg);
	Pthread_detach(pthread_self());
	do_work(tmp.connfd, &tmp.sockaddr);
	close(tmp.connfd);
	return 0;
}
/* 
 * main - Main routine for the proxy program 
 */
int main(int argc, char **argv)
{
    /* Check arguments */
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
        exit(0);
    }
	file = fopen("proxy.log", "a");
	signal(SIGPIPE, SIG_IGN);
	sem_init(&sem_log, 0, 1);
	sem_init(&sem_dns, 0, 1);
	sem_init(&sem_thr, 0, 1);

	int port = atoi(argv[1]);
	int listenfd = Open_listenfd(port);
	socklen_t clientlen = sizeof(struct sockaddr_in);
	pthread_t tid;
	while (1) {
		mytype *arg = (mytype *)malloc(sizeof(mytype));	/*新建指针代入，预防多线程时出错*/
		arg->connfd = Accept(listenfd, (SA *)&arg->sockaddr, &clientlen);
		Pthread_create(&tid, 0, thread, (void*) arg);
	}
    exit(0);
}

/*
 *  照书上打的open_clientfd加上P和V
 *    */
int open_clientfd_ts(char *hostname, int port) 
{
	int clientfd;
	struct hostent *hp;
	struct sockaddr_in serveraddr;


	if ((clientfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) return -1;
	P(&sem_dns);	/*不安全，加锁。尝试过不将hp设为指针，但gethostbyname返回的是指针，太难处理了*/
	if ((hp = gethostbyname(hostname)) == NULL)
	{
		V(&sem_dns);
		return -1;
	}
	bzero((char *) &serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	bcopy((char *)hp->h_addr_list[0],  (char *)&serveraddr.sin_addr.s_addr, hp->h_length);

	V(&sem_dns);
	serveraddr.sin_port = htons(port);
	if (connect(clientfd, (SA *) &serveraddr, sizeof(serveraddr)) < 0)
		return -1;
	return clientfd;
}

/*
 * 重写的parse_uri
 */
int parse_uri(char *uri, char *hostname, char *pathname, int *port)  
{  
	int len = strlen(uri);  

	if (strncasecmp(uri, "http://", 7) != 0) return 0;

	char *hostbegin = uri + 7;
	char *pathbegin = strchr(hostbegin, '/');
	if(pathbegin == 0)
	{
		strcpy(pathname, "/");
		pathbegin = uri + len;
	}
	else  
	{  
		int path_len = len - (pathbegin - uri);  
		strncpy(pathname, pathbegin, path_len);  
		pathname[path_len] = 0;  
	}  

	char *portbegin = strchr(hostbegin, ':');
	if (portbegin != 0) 
		*port = atoi(portbegin + 1);
	else
		*port = 80;

	int host_len = pathbegin - hostbegin;
	strncpy(hostname, hostbegin, host_len);  
	hostname[host_len] = 0;  

	return 1;  
}

void do_work(int connfd, struct sockaddr_in *sockaddr)
{
	rio_t rio, svr_rio;
	char method[MAXLINE], uri[MAXLINE],
		 version[MAXLINE], buf[MAXLINE];

	rio_readinitb(&rio, connfd);
	rio_readlineb(&rio, buf, MAXLINE);
	if(sscanf(buf, "%s %s %s", method, uri, version) < 3) {
		printf("Bad Request : %s\n", buf);
		return;
	}
	printf("%s %s %s\n",method, uri, version);
	if(strcmp(method, "GET")) {
		printf("Not Implemented : %s\n", method);
		return;
	}
	char host[MAXLINE], path[MAXLINE];
	int port;
	if(!parse_uri(uri, host, path, &port) == -1) return;

	strcpy(buf, method);	strcat(buf, " ");  
	strcat(buf, path);		strcat(buf, " ");  
	strcat(buf, version);	strcat(buf, "\r\n");

	int svr_fd = open_clientfd_ts(host, port);
	if (svr_fd == -1) {
		printf("open clientfd error\n");
		return;
	}
	rio_readinitb(&svr_rio, svr_fd);
	rio_writen(svr_fd, buf, strlen(buf));

	while(rio_readlineb(&rio, buf, MAXLINE) > 2) {
		printf("conn: %s",buf);
		rio_writen(svr_fd, buf, strlen(buf));
	}
	rio_writen(svr_fd, "\r\n", 2);

	int len = 0;
	int found = 0;
	while(rio_readlineb(&svr_rio, buf, MAXLINE) > 2) {
	    char *result = strstr(buf, "Content-Length");
		if(result != 0)	{
			found = 1;
			result += 16;
			len = atoi(result);
		}
		printf("svr: %s",buf);
		rio_writen(connfd, buf, strlen(buf));
	}
	rio_writen(connfd, "\r\n", 2);

	if (!found) {
		rio_readlineb(&svr_rio, buf, MAXLINE);
		len = strtol(buf, 0, 16);
		rio_writen(connfd, buf, strlen(buf));
	}
	printf("Length: %d\n", len);

	int res_size;
	int size;
	while((size = rio_readnb(&svr_rio, buf, MAXLINE)) != 0)
	{
		res_size += size;
		rio_writen(connfd, buf, size);
	}

	format_log_entry(buf, sockaddr, uri, res_size);
	P(&sem_log);	/*预防写入文件时出错，加锁*/
	fprintf(file, "%s", buf);
	fflush(file);
	V(&sem_log);

	close(svr_fd);
}

/*自带的*/
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, 
		char *uri, int size)
{
	time_t now;
	char time_str[MAXLINE];
	unsigned long host;
	unsigned char a, b, c, d;

	now = time(NULL);
	strftime(time_str, MAXLINE, "%a %d %b %Y %H:%M:%S %Z", localtime(&now));
	
	host = ntohl(sockaddr->sin_addr.s_addr);
	a = host >> 24;
	b = (host >> 16) & 0xff;
	c = (host >> 8) & 0xff;
	d = host & 0xff;
	sprintf(logstring, "%s: %d.%d.%d.%d %s %d\n", time_str, a, b, c, d, uri, size);
}
