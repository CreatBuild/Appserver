#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <malloc.h>
#include <syslog.h>
#include <sys/time.h>
#include <sys/epoll.h>
#include <stdbool.h>


#include <mysql/mysql.h>
#include <zdb.h>

#include "SessionMng.h"
#include "MsgProc.h"
#include "log.h"

#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif /* __cplusplus */

#define LINK_TIMEOUT         1
#define LINK_NOT_TIMEOUT     0

MYSQL *conn;
extern ConnectionPool_T pool;

int ProduceUID( Connection_T con )
{
	int maxUID = 0;
	
	ResultSet_T result = Connection_executeQuery(con, "select max(UID) from account;");
	
	ResultSet_next(result);

	maxUID = ResultSet_getInt(result, 1);
	if(maxUID == 0)
	{
	    return 100000;
	}
    else
	{
		return maxUID + 1;
	}
}

int CheckLinkTime(Client_Data_S *pstCient)
{
    if( (time(NULL) - pstCient->times) > LINK_TIMEOUTVAL)
	{
	    return LINK_TIMEOUT;
	}
	else
	{
	    return LINK_NOT_TIMEOUT;
	}
}


int SendCheckLink(int sock, Client_Data_S *pstCient)
{
    int ret = 0;
	stMsgHead MsgHeadResp;

	memset(&MsgHeadResp, 0, sizeof(MsgHeadResp));

	if(pstCient == NULL)
	{
	    return -1;
	}

	if(CheckLinkTime( pstCient ) == LINK_TIMEOUT)
	{
	    return LINK_TIMEOUT;
	}
		
	MsgHeadResp.head = '#';
	MsgHeadResp.s32Length = 0;
	MsgHeadResp.s32CmdId = CMD_LINK_CHECK_PESP;

	if((ret = Msg_Send(sock, &MsgHeadResp, sizeof(stMsgHead))) < 0)
	{
		Log_Trace(LOG_LEVEL_DEBUG, "Send check link fail! \n");
	}

	return ret;
}

int MsgProc(int idx, int sockfd, stMsg *Msg)
{
    int ret;
    int Cmd = Msg->MsgHead.s32CmdId;
	stMsg MsgResp;

	char szUszername[32];
	char szPassword[32];
	char szDID[32];
	ResultSet_T result;
	PreparedStatement_T p;
	char szTemp[512];

	Client_Data_S *pstCient = NULL;

	Log_Trace(LOG_LEVEL_DEBUG, "recv a CMD = %d \n", Cmd);

    if(SessionMng_GetUserHead(idx, sockfd, &pstCient) < 0)
	{
	    Log_Trace(LOG_LEVEL_DEBUG, "SessionMng_GetUserHead return fail!\n");
	}
	else
	{
		if(Cmd < FUNCTION_CMD_END)
		{
			pstCient->times = time(NULL);
		}
		else if(Cmd == CMD_LINK_CHECK)
		{
			pstCient->last_link_time = time(NULL);
		}
	}

	Connection_T con = ConnectionPool_getConnection(pool);

	switch(Cmd)
	{
	    case CMD_REG:
			{			
				strcpy(szUszername, Msg->Body.MsgReg.szUserName);
				strcpy(szPassword, Msg->Body.MsgReg.szPassWord);

				MsgResp.MsgHead.head = '#';
				MsgResp.MsgHead.s32Length = sizeof(stMsgRegResp);
				MsgResp.MsgHead.s32CmdId = CMD_REG_RESP;
	
		
				TRY
				{
					Connection_execute(con, "lock table account write;");
					
					int uid = ProduceUID(con);

					p = Connection_prepareStatement(con, "select Username from account where Username=?;");
					PreparedStatement_setString(p, 1, szUszername);
					result = PreparedStatement_executeQuery(p);

					if(ResultSet_next(result) == false)        //账号名不存在
					{
						//Connection_beginTransaction(con);
					    Connection_execute(con, "insert into account(UID, Username, Password)values('%d', '%s', '%s');", \
							uid, szUszername, szPassword);    
						//Connection_commit(con);
						
						MsgResp.Body.MsgRegResp.status = EXEC_SUCCESS;					
					}
					else                //账号名存在
					{
						MsgResp.Body.MsgRegResp.status = USERNAME_OCCUPY;					
					}
				}
				CATCH(SQLException)
				{
				    //Connection_rollback(con);
				    MsgResp.Body.MsgRegResp.status = EXEC_FAIL;
					Log_Trace(LOG_LEVEL_DEBUG, "CMD = %d---SQLException -- %s\n", Cmd, Exception_frame.message);
				}
				FINALLY
				{
					Connection_execute(con, "unlock tables;");
				}
				END_TRY;

				memcpy(szTemp, &MsgResp.MsgHead, sizeof(stMsgHead));
				memcpy(szTemp + sizeof(stMsgHead), &MsgResp.Body.MsgRegResp, sizeof(stMsgRegResp));

				Msg_Send(sockfd, szTemp, sizeof(stMsgHead) + sizeof(stMsgRegResp)); 
	    	}			
			break;
	    case CMD_LOGIN:
			{
				strcpy(szUszername, Msg->Body.MsgLogin.szUserName);
				strcpy(szPassword, Msg->Body.MsgLogin.szPassWord);

				MsgResp.MsgHead.head = '#';
				MsgResp.MsgHead.s32Length = sizeof(stLoginResp);
				MsgResp.MsgHead.s32CmdId = CMD_LOGIN_RESP;

                TRY
            	{
					p = Connection_prepareStatement(con, "select UID,Username,Password from account where Username=?;");
					PreparedStatement_setString(p, 1, szUszername);
					result = PreparedStatement_executeQuery(p);
					if(ResultSet_next(result) == false)
					{
						MsgResp.Body.LoginResp.status = USERNAME_UNREG;
						MsgResp.Body.LoginResp.uid = -1;			
						break;
					}			

	                if(strcmp(szUszername, ResultSet_getString(result, 2)) == 0 &&
						strcmp(szPassword, ResultSet_getString(result, 3)) == 0)
	            	{
						MsgResp.Body.LoginResp.status = EXEC_SUCCESS;
						MsgResp.Body.LoginResp.uid = ResultSet_getInt(result, 1);
						
						pstCient->login_status = LOGIN;
	            	}
					else
					{
						MsgResp.Body.LoginResp.status = PASSWORD_ERR;
						MsgResp.Body.LoginResp.uid = -1;
					}
            	}
				CATCH(SQLException)
				{					
				    MsgResp.Body.LoginResp.status = EXEC_FAIL;
					Log_Trace(LOG_LEVEL_DEBUG, "CMD = %d---SQLException -- %s\n", Cmd, Exception_frame.message);
				}
				FINALLY
				{
				    
				}
				END_TRY;

				memcpy(szTemp, &MsgResp.MsgHead, sizeof(stMsgHead));
				memcpy(szTemp + sizeof(stMsgHead), &MsgResp.Body.LoginResp, sizeof(stLoginResp));

				Msg_Send(sockfd, szTemp, sizeof(stMsgHead) + sizeof(stLoginResp));                    
	    	}
			break;
		case CMD_MODIFY_PW:
			{
				const char *pUszername = NULL;
				const char *pPassword = NULL;

				MsgResp.MsgHead.head = '#';
				MsgResp.MsgHead.s32Length = sizeof(stModifyPwResp);
				MsgResp.MsgHead.s32CmdId = CMD_MODIFY_PW_RESP;

				if(pstCient->login_status != LOGIN)
				{
				    break;
				}
		
				TRY
				{
					Connection_execute(con, "lock table account write;");

					p = Connection_prepareStatement(con, "select Username,Password from account where Username=?;");
					PreparedStatement_setString(p, 1, Msg->Body.ModifyPw.szUserName);
					result = PreparedStatement_executeQuery(p);

					if(ResultSet_next(result) == false)        //账号名不存在
					{						
						MsgResp.Body.ModifyPwResp.status = USERNAME_UNREG;
						break;
					}
					else
					{
						pUszername = ResultSet_getString(result, 1);
						pPassword = ResultSet_getString(result, 2);
					}

					Log_Trace(LOG_LEVEL_DEBUG, "username = %s---oldpw = %s---newpw = %s\n", \
						Msg->Body.ModifyPw.szUserName, Msg->Body.ModifyPw.szOldPassWord, \
						Msg->Body.ModifyPw.szNewPassWord);
							
					if(strcmp(pUszername, Msg->Body.ModifyPw.szUserName) == 0 && \
						strcmp(pPassword, Msg->Body.ModifyPw.szOldPassWord) == 0 )                 
					{
					    Connection_execute(con, "update account set Password=%s where Username='%s';", \
							Msg->Body.ModifyPw.szNewPassWord, Msg->Body.ModifyPw.szUserName); 
						
						MsgResp.Body.ModifyPwResp.status = EXEC_SUCCESS;					
					}
					else
					{
					    MsgResp.Body.ModifyPwResp.status = PASSWORD_ERR;
					}
				}
				CATCH(SQLException)
				{
				    MsgResp.Body.ModifyPwResp.status = EXEC_FAIL;
					Log_Trace(LOG_LEVEL_DEBUG, "CMD = %d---SQLException -- %s\n", Cmd, Exception_frame.message);
				}
				FINALLY
				{
					Connection_execute(con, "unlock tables;");
				}
				END_TRY;

				memcpy(szTemp, &MsgResp.MsgHead, sizeof(stMsgHead));
				memcpy(szTemp + sizeof(stMsgHead), &MsgResp.Body.ModifyPwResp, sizeof(stModifyPwResp));

				Msg_Send(sockfd, szTemp, sizeof(stMsgHead) + sizeof(stModifyPwResp)); 
			}
		    break;
		case CMD_REG_DEV:
			{
				MsgResp.MsgHead.head = '#';
				MsgResp.MsgHead.s32Length = sizeof(stRegDevResp);
				MsgResp.MsgHead.s32CmdId = CMD_REG_DEV_RESP;

				if(pstCient->login_status != LOGIN)
				{
				    break;
				}
				
			    strcpy(szDID, Msg->Body.RegDev.DID);

				TRY
				{
					Connection_execute(con, "lock table device write;");

					p = Connection_prepareStatement(con, "select DID from device where DID=?;");
					PreparedStatement_setString(p, 1, szDID);
					result = PreparedStatement_executeQuery(p);
					if(ResultSet_next(result) != false)
					{
						MsgResp.Body.RegDevResp.status = DEV_REGISTERED;                      //设备存在不需要注册
					}
					else
					{		
					    Connection_execute(con, "insert into device(UID, DID, Byname)values('%d', '%s', N'%s');", \
						                     0, szDID, Msg->Body.RegDev.Byname);  
						MsgResp.Body.RegDevResp.status = EXEC_SUCCESS; 
					}	
				}
				CATCH(SQLException)
				{
				    MsgResp.Body.RegDevResp.status = EXEC_FAIL;
					Log_Trace(LOG_LEVEL_DEBUG, "CMD = %d---SQLException -- %s\n", Cmd, Exception_frame.message);
				}
				FINALLY
				{
					Connection_execute(con, "unlock tables;");
				}
				END_TRY;

				memcpy(szTemp, &MsgResp.MsgHead, sizeof(stMsgHead));
				memcpy(szTemp + sizeof(stMsgHead), &MsgResp.Body.RegDevResp, sizeof(stRegDevResp));
				
				Msg_Send(sockfd, szTemp, sizeof(stMsgHead) + sizeof(stRegDevResp));
			}
			break;
		case CMD_ADD_DEV:
			{
				MsgResp.MsgHead.head = '#';
				MsgResp.MsgHead.s32Length = sizeof(stAddDevResp);
				MsgResp.MsgHead.s32CmdId = CMD_ADD_DEV_RESP;

				if(pstCient->login_status != LOGIN)
				{
				    break;
				}
				
				strcpy(szDID, Msg->Body.AddDev.DID);                
				
				TRY
				{
					Connection_execute(con, "lock table device write;");

					p = Connection_prepareStatement(con, "select DID from device where DID=?;");
					PreparedStatement_setString(p, 1, szDID);
					result = PreparedStatement_executeQuery(p);
					if(false != ResultSet_next(result))                                           //设备存在更新
					{
						Connection_execute(con, "update device set UID=%d where DID='%s';", Msg->Body.AddDev.uid, szDID); 
					}
					else
					{
						Connection_execute(con, "insert into device(UID, DID)values('%d', '%s');", Msg->Body.AddDev.uid, szDID);
					}			    
					 
					MsgResp.Body.AddDevResp.status = EXEC_SUCCESS;
				}
				CATCH(SQLException)
				{
				    MsgResp.Body.AddDevResp.status = EXEC_FAIL;
					Log_Trace(LOG_LEVEL_DEBUG, "CMD = %d---SQLException -- %s\n", Cmd, Exception_frame.message);
				}
				FINALLY
				{
					Connection_execute(con, "unlock tables;");
				}
				END_TRY;

				memcpy(szTemp, &MsgResp.MsgHead, sizeof(stMsgHead));
				memcpy(szTemp + sizeof(stMsgHead), &MsgResp.Body.AddDevResp, sizeof(stAddDevResp));
				
				Msg_Send(sockfd, szTemp, sizeof(stMsgHead) + sizeof(stAddDevResp));
			}
			break;
		case CMD_DEL_DEV:
			{
				MsgResp.MsgHead.head = '#';
				MsgResp.MsgHead.s32Length = sizeof(stDelDevResp);
				MsgResp.MsgHead.s32CmdId = CMD_DEL_DEV_RESP;

				if(pstCient->login_status != LOGIN)
				{
				    break;
				}
				
				strcpy(szDID, Msg->Body.DelDev.DID);               
				
				TRY
				{
					Connection_execute(con, "lock table device write;");

					p = Connection_prepareStatement(con, "select DID from device where DID=?;");
					PreparedStatement_setString(p, 1, szDID);
					result = PreparedStatement_executeQuery(p);
					if(false != ResultSet_next(result))                                           //设备存在更新
					{
					    Connection_execute(con, "delete from device where DID='%s';", szDID); 
						MsgResp.Body.AddDevResp.status = EXEC_SUCCESS;
					}
					else
					{
					    MsgResp.Body.AddDevResp.status = DEV_UNREG;
					}			    			
				}
				CATCH(SQLException)
				{
				    MsgResp.Body.AddDevResp.status = EXEC_FAIL;
					Log_Trace(LOG_LEVEL_DEBUG, "CMD = %d---SQLException -- %s\n", Cmd, Exception_frame.message);
				}
				FINALLY
				{
					Connection_execute(con, "unlock tables;");
				}
				END_TRY;

				memcpy(szTemp, &MsgResp.MsgHead, sizeof(stMsgHead));
				memcpy(szTemp + sizeof(stMsgHead), &MsgResp.Body.DelDevResp, sizeof(stDelDevResp));
				
				Msg_Send(sockfd, szTemp, sizeof(stMsgHead) + sizeof(stDelDevResp));
			}
			break;
		case CMD_LIST_DEV:
			{
				int i = 1;
				const char *str = NULL;
				stDevInfo *pTemp = NULL;
				stDevInfo *apDevInfo = NULL;	

				if(pstCient->login_status != LOGIN)
				{
				    break;
				}
				
				TRY
				{
					Connection_execute(con, "lock table device read;");

					p = Connection_prepareStatement(con, "select DID from device where UID=?;");
					PreparedStatement_setInt(p, 1, Msg->Body.ListDev.uid);
					result = PreparedStatement_executeQuery(p);	
					
					while (ResultSet_next(result))
					{
					    if(NULL == apDevInfo)
				    	{
				    	    apDevInfo = (stDevInfo *)malloc(sizeof(stDevInfo));
							if(NULL == apDevInfo)
							{
							    i = 0;
							    Log_Trace(LOG_LEVEL_ERR, "Memory malloc fail err = %d\n", errno);
								break;
							}
				    	}
						else
						{
						    i++;
							pTemp = (stDevInfo *)realloc(apDevInfo, i * sizeof(stDevInfo));
							if(NULL == pTemp)
							{
							    i = 0;
							    free(apDevInfo);
								apDevInfo = NULL;
								break;
							}

							apDevInfo = pTemp;
						}
	
					    str = ResultSet_getString(result, 1);
						if(NULL == str)
						{
						    free(apDevInfo);
							apDevInfo = NULL;
						    break;
						}
						memset(apDevInfo[i-1].DID, 0 , DID_LEN);
					    memcpy(apDevInfo[i-1].DID, str, ResultSet_getColumnSize(result, 1));	
					}
				}
				CATCH(SQLException)
				{
				    if(NULL != apDevInfo)
			    	{
			    	    free(apDevInfo);
			    	}
				    Log_Trace(LOG_LEVEL_DEBUG, "CMD = %d---SQLException -- %s\n", Cmd, Exception_frame.message);
				}
				FINALLY
				{
					Connection_execute(con, "unlock tables;");
				}
				END_TRY;

                if(i != 0)
            	{
					MsgResp.MsgHead.head = '#';
					MsgResp.MsgHead.s32Length = i * sizeof(stDevInfo);
					MsgResp.MsgHead.s32CmdId = CMD_LIST_DEV_RESP;

					memcpy(szTemp, (char *)&MsgResp.MsgHead, sizeof(stMsgHead));
					memcpy(szTemp + sizeof(stMsgHead), (char *)apDevInfo, MsgResp.MsgHead.s32Length);
            	}
				else
				{
				    MsgResp.MsgHead.head = '#';
					MsgResp.MsgHead.s32Length = 0;
					MsgResp.MsgHead.s32CmdId = CMD_LIST_DEV_RESP;
				}

                Msg_Send(sockfd, szTemp, sizeof(stMsgHead) + MsgResp.MsgHead.s32Length);
				
				free(apDevInfo);
			}
			break;
		case CMD_BYE:
			{
				SendByeMessage( sockfd );
				SessionMng_CloseASession(idx, sockfd);
			}
			break;
		case CMD_LINK_CHECK:
			{
				ret = SendCheckLink( sockfd, pstCient );
				
				if((ret == LINK_TIMEOUT) || ret < 0 )
				{
				    SessionMng_CloseASession(idx, sockfd);
				}
			}
		    break;
		default:
			break;
	}

	Connection_close(con);

	return 0;
}

int SendByeMessage(int sock)
{
    int ret = 0;
	stMsgHead MsgHeadResp;

	memset(&MsgHeadResp, 0, sizeof(MsgHeadResp));

	MsgHeadResp.head = '#';
	MsgHeadResp.s32Length = 0;
	MsgHeadResp.s32CmdId = CMD_BYE_RESP;

	if((ret = Msg_Send(sock, &MsgHeadResp, sizeof(stMsgHead))) < 0)
	{
		Log_Trace(LOG_LEVEL_DEBUG, "Send Bye fail, err = %d \n", errno);
	}

	return ret;
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */


