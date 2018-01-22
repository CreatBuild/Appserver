#ifndef __SESSIONMNG_H__
#define __SESSIONMNG_H__

#include <netinet/in.h>

#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif /* __cplusplus */

#define PROCESS_COUNT 8
#define USER_PER_PROCESS   65536

#define LINK_TIMEOUTVAL    10 * 60
#define LINK_TIMEVAL       20
#define LINK_LOST_COUNT    3

#define BUF_STATE_FREE 0
#define BUF_STATE_BUSY 1

typedef struct tagClient_Data
{
	time_t  times;
	time_t  last_link_time;
	int lost_link_count;
	int conn_fd;
	int login_status;
	int sessionID;
    int csseq;
}Client_Data_S;

typedef struct tagclient_data_mng{
	int totalcount;
	int childcount[PROCESS_COUNT];
	int child_epollfd[PROCESS_COUNT];
	int user_buf_stata[PROCESS_COUNT][USER_PER_PROCESS];

	pthread_mutex_t lock;
}Client_Date_Mng_S;

int SessionMng_Create();
int SessionMng_Destory();

int SessionMng_Alloc(int pindex, int sock, Client_Data_S **ppHandle);
int SessionMng_Free(int pindex, int sock, Client_Data_S *pClient);

int SessionMng_CloseAll(int idx, int cfd);   //ÐèÒªÐÞ¸Ä
int SessionMng_CloseASession(int idx, int fd);

void* SessionMng_GetShBufHead();
Client_Data_S* SessionMng_GetUserBufHead(int pindex);
int SessionMng_GetUserHead(int pindex, int sock, Client_Data_S **ppHandle);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */


#endif //__SESSIONMNG_H__