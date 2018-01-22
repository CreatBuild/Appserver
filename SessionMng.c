#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif /* __cplusplus */
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <sys/epoll.h>

#include "log.h"
#include "MsgProc.h"
#include "SessionMng.h"


#define SHM_STATE_NAME     "AppSession"
#define SEM_NAME           "SemSession"
#define SHM_SIZE           PROCESS_COUNT * USER_PER_PROCESS * sizeof(Client_Data_S) + sizeof(Client_Date_Mng_S)

static int shmfd = -1;
static Client_Data_S *gUsersBuf[PROCESS_COUNT];
static void *gShmBuf = NULL;
static sem_t *sem = NULL;

int InitShm()
{
    struct stat st;
	
    shmfd = shm_open(SHM_STATE_NAME, O_CREAT | O_RDWR, 0644);
	if(shmfd < 0)
	{
	    Log_Trace(LOG_LEVEL_ERR, "shm_open fail err = %d!\n", errno);
		return -1;
	}

	if (ftruncate(shmfd, SHM_SIZE) == -1) 
	{
        Log_Trace(LOG_LEVEL_ERR, "ftruncate fail err = %d!\n", errno);
        close(shmfd);
        return -1;
    }


    gShmBuf = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
    if (gShmBuf == MAP_FAILED)
	{
        Log_Trace(LOG_LEVEL_ERR, "mmap fail err = %d!\n", errno);
        gShmBuf = NULL;
        close(shmfd);
        return -1;
    }

	Client_Date_Mng_S *pstClient_Mng;

    pstClient_Mng = (Client_Date_Mng_S *)gShmBuf;

	return 0;
}

int InitSem()
{
    sem = sem_open(SEM_NAME, O_CREAT, 0644, 1);	
	if (sem == SEM_FAILED)
	{
	    Log_Trace(LOG_LEVEL_ERR, "sem_open fail err = %d!\n", errno);
		return -1;
	}

	return 0;
}


int SessionMng_Create()
{
    int ret = 0;
	int i = 0;
	Client_Date_Mng_S *pstClient_Mng;
	Client_Data_S *pstCient;
	
    if((ret = InitShm()) < 0)
 	{
 	    Log_Trace(LOG_LEVEL_ERR, "InitShm fail!\n");
		return ret;
 	}

    for(i = 0; i < PROCESS_COUNT; i++)
	{
		gUsersBuf[i] = gShmBuf + sizeof(Client_Date_Mng_S) \
			+USER_PER_PROCESS * sizeof(Client_Data_S) * i;
	}
	pstClient_Mng = (Client_Date_Mng_S *)gShmBuf;

	memset(gShmBuf, 0, SHM_SIZE);

	if((ret = InitSem()) < 0)
	{
	   Log_Trace(LOG_LEVEL_ERR, "InitShm fail!\n");
	   return ret;
	}

	return ret;
}

int SessionMng_Destory()
{
    int ret = 0;
	struct stat st;

	ret = fstat(shmfd, &st);
    if (ret < 0) 
	{
        Log_Trace(LOG_LEVEL_ERR, "shm fd err = %d!\n", errno);
        return -1;
    }


	ret = munmap(gShmBuf, st.st_size);
    if (ret) 
	{
        Log_Trace(LOG_LEVEL_ERR, "munmap fail err = %d!\n", errno);
    }

	ret |= close(shmfd);
	ret |= sem_destroy(sem);

	return ret;
}

int SessionMng_open()
{
    int ret = 0;

	if((ret = InitShm()) < 0)
 	{
 	    Log_Trace(LOG_LEVEL_ERR, "InitShm fail!\n");
 	}

	return ret;
}

int SessionMng_Alloc(int pindex, int sock, Client_Data_S **ppHandle)
{
    int ret = 0;
	Client_Date_Mng_S *pstClient_Mng;
	Client_Data_S *pTemp = NULL;

	pstClient_Mng = (Client_Date_Mng_S *)gShmBuf;

    if(pstClient_Mng->user_buf_stata[pindex][sock] == BUF_STATE_FREE)
	{
	    pTemp = &gUsersBuf[pindex][sock];
	}
	else
	{
	    return -1;
	}

	if(NULL == pTemp)
	{
	    return -1;
	}

	pthread_mutex_lock( &(pstClient_Mng->lock ));

	pstClient_Mng->totalcount++;
	pstClient_Mng->childcount[pindex]++;
	pstClient_Mng->user_buf_stata[pindex][sock] = BUF_STATE_BUSY;
	
	pthread_mutex_unlock(&(pstClient_Mng->lock ));  

    *ppHandle = pTemp;
	

	return 0;
	
}

int SessionMng_Free(int pindex, int sock, Client_Data_S *pClient)
{
    int ret = 0;
	Client_Date_Mng_S *pstClient_Mng;
	Client_Data_S *pTemp = NULL;

	if(NULL == pClient)
	{
	    Log_Trace(LOG_LEVEL_DEBUG, "pClient is NULL!\n");
		return -1;
	}

	pstClient_Mng = (Client_Date_Mng_S *)gShmBuf;

    pTemp = &gUsersBuf[pindex][sock] ;
    if(pTemp != pClient)
	{
	    return -1;
	}
    
	pthread_mutex_lock( &(pstClient_Mng->lock ));

    pstClient_Mng->user_buf_stata[pindex][sock] = BUF_STATE_FREE;
	pstClient_Mng->totalcount--;
	pstClient_Mng->childcount[pindex]--;

	pthread_mutex_unlock(&(pstClient_Mng->lock ));

	memset(pTemp, 0, sizeof(Client_Data_S));

	return ret;
}


int SessionMng_CloseAll(int idx, int cfd)   //ÐèÒªÐÞ¸Ä
{
    int ret = 0;
	int i = 0;
	struct list_head *pos;
	Client_Data_S *pTemp = NULL;
	Client_Date_Mng_S *pstClient_Mng;

	pstClient_Mng = (Client_Date_Mng_S *)gShmBuf;


    for(i = 0; i < USER_PER_PROCESS; i++)
	{
	    if(pstClient_Mng->user_buf_stata[idx][i] == BUF_STATE_BUSY)
    	{
    	    pTemp = &gUsersBuf[idx][i];
			SendByeMessage(pTemp->conn_fd);
    	    epoll_ctl( cfd, EPOLL_CTL_DEL, pTemp->conn_fd, 0 );
		    close(pTemp->conn_fd);

            pthread_mutex_lock( &(pstClient_Mng->lock ));
			pstClient_Mng->totalcount--;
			pstClient_Mng->childcount[idx]--;
			pstClient_Mng->user_buf_stata[idx][i] = BUF_STATE_FREE;
			pthread_mutex_unlock(&(pstClient_Mng->lock ));
    	}
	}

	return ret;
}


int SessionMng_CloseASession(int idx, int fd)
{
    int ret = 0;
	Client_Data_S *pTemp = NULL;
	Client_Date_Mng_S *pstClient_Mng;

	pstClient_Mng = (Client_Date_Mng_S *)gShmBuf;


    pTemp = &gUsersBuf[idx][fd];
    if(pTemp->conn_fd == fd)
	{
		SendByeMessage(pTemp->conn_fd);
	    epoll_ctl( pstClient_Mng->child_epollfd[idx], EPOLL_CTL_DEL, pTemp->conn_fd, 0 );
	    close(pTemp->conn_fd);

		memset(pTemp, 0, sizeof(Client_Data_S));

	    pthread_mutex_lock( &(pstClient_Mng->lock ));
		pstClient_Mng->totalcount--;
		pstClient_Mng->childcount[idx]--;
		pstClient_Mng->user_buf_stata[idx][fd] = BUF_STATE_FREE;
		pthread_mutex_unlock(&(pstClient_Mng->lock ));
	}
	else
	{
	    return -1;
	}

	Log_Trace(LOG_LEVEL_INFO, "Connect fd = %d exit from process index = %d!\n", fd, idx);

	return ret;
}

int SessionMng_GetUserHead(int pindex, int sock, Client_Data_S **ppHandle)
{
    int ret = 0;
	Client_Data_S *pTemp = NULL;

	pTemp = &gUsersBuf[pindex][sock];
	if(pTemp->conn_fd != sock)
	{
	    return -1;
	}

    *ppHandle = pTemp;
	
	return 0;	
}

void* SessionMng_GetShBufHead()
{
    return gShmBuf;
}

Client_Data_S* SessionMng_GetUserBufHead(int pindex)
{
    return gUsersBuf[pindex];
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */

