#ifndef __MSGPROC_H__
#define __MSGPROC_H__

#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif /* __cplusplus */


#define   DID_LEN                          32        
#define   USER_LEN                         32

#define   LOGIN                             1
#define   LOGOUT                            0


#define   CMD_LOGIN                         1
#define   CMD_REG                           2
#define   CMD_REG_DEV                       4
#define   CMD_ADD_DEV                       5
#define   CMD_LIST_DEV                      6
#define   CMD_BYE                           7
#define   CMD_MODIFY_PW                     8
#define   CMD_DEL_DEV                       9
#define   FUNCTION_CMD_END                 80       //功能命令的结尾

#define   CMD_LOGIN_RESP                   81
#define   CMD_REG_RESP                     82
#define   CMD_REG_DEV_RESP                 83
#define   CMD_ADD_DEV_RESP                 84
#define   CMD_LIST_DEV_RESP                85
#define   CMD_BYE_RESP                     86
#define   CMD_MODIFY_PW_RESP               87
#define   CMD_DEL_DEV_RESP                 88

#define   CMD_LINK_CHECK                   101
#define   CMD_LINK_CHECK_PESP              102

typedef enum tagServer_Status_E
{
    EXEC_SUCCESS,
	EXEC_FAIL,
	USERNAME_OCCUPY,
	USERNAME_UNREG,
	PASSWORD_ERR,
	DEV_UNREG,
	DEV_REGISTERED
}Server_Status_E;

typedef struct tagMsgHead{
    char head;
	char resv[3];
	int  s32CmdId;
	int  s32Length;
	int  s32SessionId;
	int  Csseq;
}stMsgHead;

typedef struct tagMsgReg{
    char szUserName[USER_LEN];
	char szPassWord[USER_LEN];
}stMsgReg;

typedef struct tagMsgRegResp{
    int status;
}stMsgRegResp;

typedef struct tagMsgLogin{
	char szUserName[USER_LEN];
	char szPassWord[USER_LEN];
}stMsgLogin;

typedef struct tagLoginResp{
	int status;
	int uid;
}stLoginResp;

typedef struct tagRegDev{
	char DID[DID_LEN];
	char Byname[DID_LEN];
}stRegDev;

typedef struct tagRegDevResp{
	int status;
}stRegDevResp;


typedef struct tagAddDev{
	int uid;
	char DID[DID_LEN];
}stAddDev;

typedef struct tagAddDevResp{
	int status;
}stAddDevResp;

typedef struct tagDelDev{
	char DID[DID_LEN];
}stDelDev;

typedef struct tagelDevResp{
	int status;
}stDelDevResp;


typedef struct tagListDev{
	int uid;
}stListDev;

typedef struct tagDevInfo{
	char DID[DID_LEN];
}stDevInfo;

typedef struct tagModifyPw{
	char szUserName[USER_LEN];
	char szOldPassWord[USER_LEN];
	char szNewPassWord[USER_LEN];
}stModifyPw;

typedef struct tagModifyPwResp{
	int status;
}stModifyPwResp;


union unBody{
    stMsgLogin 	       MsgLogin;
	stLoginResp        LoginResp;
	stMsgReg           MsgReg;
	stMsgRegResp       MsgRegResp;
	stRegDev           RegDev;
	stRegDevResp       RegDevResp;
	stAddDev           AddDev;
	stAddDevResp       AddDevResp;
	stDelDev           DelDev;
	stDelDevResp       DelDevResp;
	stListDev          ListDev;
	stModifyPw         ModifyPw;
	stModifyPwResp     ModifyPwResp;
};


typedef struct tagMsg{
	stMsgHead MsgHead;
	union unBody Body;
}stMsg;


int MsgProc(int idx, int sockfd, stMsg *Msg);
int SendByeMessage(int sock);



#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */

#endif //#define 
