#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/epoll.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "MsgProc.h"
#include "erron.h"
#include "log.h"

#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif /* __cplusplus */

int Recv_Data(int Sock, char *szBuff, int NeedRecvSize)
{
    int RecvSize = 0;
	int ret;
	
    do
	{
	    ret = recv(Sock, szBuff + RecvSize, NeedRecvSize, 0);
		if(ret < 0)
		{
		    return FAIL;
		}
		else
		{
		    RecvSize += ret;
		}
	}while(NeedRecvSize > RecvSize);

	return RecvSize;
}

int Msg_Recv(int Sock, char *szBuff, int BuffSize)
{
    int ret = 0;

	ret = Recv_Data(Sock, szBuff, sizeof(stMsgHead));
	if( ret < 0 )
    {
        if( errno != EAGAIN )
        {
            return ERR_SOCKET_RECV;
        }
    }
    else if( ret == 0 )
    {
        return SUCCESS;
    }
	else
	{
	    int NeedRecv = ((stMsgHead *)szBuff)->s32Length;

		if(NeedRecv > BuffSize)
		{
		    Log_Trace(LOG_LEVEL_INFO, "Msg_Recv fail NeedRecv = %d, BuffSize = %d\n", NeedRecv, BuffSize);
		    return ERR_SOCKET_BUFF;
		}
		else if(NeedRecv== 0)
		{
		    return SUCCESS;
		}
		else
		{
		    ret = Recv_Data(Sock, szBuff + sizeof(stMsgHead), NeedRecv);
			if(ret < 0)
			{
			    return FAIL;
			}
		}				
	}

	return 0;
}

int Msg_Send(int Sock, char *szBuff, int ContentSize)
{
    int SendSize = 0;
	int ret = 0;

	do
	{
	    ret = send(Sock, szBuff + SendSize, ContentSize - SendSize, 0);
		if(ret <= 0)
		{
		    return ERR_SOCKET_SEND;
		}

		SendSize += ret;
	}while(ContentSize > SendSize);

	return ret;
}


#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */

