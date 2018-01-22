#ifndef __SOCKET_H__
#define __SOCKET_H__
#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif /* __cplusplus */

int Msg_Recv(int Sock, char *szBuff, int BuffSize);
int Msg_Send(int Sock, char *szBuff, int ContentSize);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */

#endif //__SOCKET_H__