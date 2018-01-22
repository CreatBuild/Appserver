#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/epoll.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "DatabaseProc.h"
#include "SessionMng.h"
#include "Socket.h"
#include "MsgProc.h"
#include "erron.h"
#include "log.h"

#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif /* __cplusplus */


#define BUFFER_SIZE 1024
#define MAX_EVENT_NUMBER 1024
#define LISTEN_NUM  1024 * 16
#define MAX_CLIENT_NUM   65535

//pipe command
#define NEW_CONN_CMD            1
#define CONN_BYE_CMD            2

struct process_in_pool
{
    pid_t pid;
    int pipefd[2];
};

struct client_data
{
    struct sockaddr_in address;
	int conn_fd;
	int sessionID;
    int csseq;
};

int sig_pipefd[2];
int epollfd;
int listenfd;
struct process_in_pool sub_process[ PROCESS_COUNT ];
bool stop_child = false;
static int SessionID_Num = 0;
static int child_pindex;
static int fd_instance;
const char path[] = "/tmp/ins";


int setnonblocking( int fd )
{
    int old_option = fcntl( fd, F_GETFL );
    int new_option = old_option | O_NONBLOCK;
    fcntl( fd, F_SETFL, new_option );
    return old_option;
}

void addfd( int epollfd, int fd )
{
    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    epoll_ctl( epollfd, EPOLL_CTL_ADD, fd, &event );
    setnonblocking( fd );
}

void sig_handler( int sig )
{
    int save_errno = errno;
    int msg = sig;
    send( sig_pipefd[1], ( char* )&msg, 1, 0 );
    errno = save_errno;
}

void addsig( int sig, void(*handler)(int), bool restart )
{
    struct sigaction sa;
		
    memset( &sa, '\0', sizeof( sa ) );
    sa.sa_handler = handler;
    if( restart )
    {
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset( &sa.sa_mask );
    assert( sigaction( sig, &sa, NULL ) != -1 );
}

void del_resource()
{
    close( sig_pipefd[0] );
    close( sig_pipefd[1] );
    close( listenfd );
    close( epollfd );
}

void child_term_handler( int sig )
{
    stop_child = true;
}

void child_child_handler( int sig )
{
    pid_t pid;
    int stat;

    while ( ( pid = waitpid( -1, &stat, WNOHANG ) ) > 0 )
    {
        continue;
    }
}

void child_timer_handler( int sig )
{
    int i;
    Client_Data_S *pUserHead = NULL;
	Client_Data_S *pTemp = NULL;
	Client_Date_Mng_S *pstClient_Mng;

	pstClient_Mng = (Client_Date_Mng_S *)SessionMng_GetShBufHead();
	pUserHead = SessionMng_GetUserBufHead( child_pindex );

	Log_Trace(LOG_LEVEL_DEBUG, "Process (%d) check link!----Session count = %d, totlal = %d\n", getpid(), pstClient_Mng->childcount[child_pindex], pstClient_Mng->totalcount);

	for(i = 0; (i < USER_PER_PROCESS && pstClient_Mng->childcount[child_pindex] > 0); i++)
	{
	    if(pstClient_Mng->user_buf_stata[child_pindex][i] == BUF_STATE_BUSY)
    	{
		    pTemp = &pUserHead[i];
			if( (time(NULL) - pTemp->last_link_time) > LINK_TIMEVAL)
			{
			    pTemp->lost_link_count++;
				if(pTemp->lost_link_count > LINK_LOST_COUNT)
				{
				    //SessionMng_CloseASession(child_pindex, pTemp->conn_fd);
				    epoll_ctl( pstClient_Mng->child_epollfd[child_pindex], EPOLL_CTL_DEL, pTemp->conn_fd, 0 );
				    close(pTemp->conn_fd);

					memset(pTemp, 0, sizeof(Client_Data_S));

				    pthread_mutex_lock( &(pstClient_Mng->lock ));
					pstClient_Mng->totalcount--;
					pstClient_Mng->childcount[child_pindex]--;
					pstClient_Mng->user_buf_stata[child_pindex][i] = BUF_STATE_FREE;
					pthread_mutex_unlock(&(pstClient_Mng->lock ));
				}
			}
    	}
		
	}
}


int GetSessionID()
{
    if(SessionID_Num < MAX_CLIENT_NUM)
	{
	    return ++SessionID_Num;
	}
	else
	{
	    return -1;
	}
}

int run_child( int idx )
{
    int i = 0, j = 0;
	Client_Data_S *pUser = NULL;
	Client_Date_Mng_S *pClienMng = NULL;
	struct itimerval tick;
		
    struct epoll_event events[ MAX_EVENT_NUMBER ];
    int child_epollfd = epoll_create( LISTEN_NUM );
    assert( child_epollfd != -1 );
    int pipefd = sub_process[idx].pipefd[1];
    addfd( child_epollfd, pipefd );
    int ret;

	child_pindex = idx;
	
    addsig( SIGTERM, child_term_handler, true );
    addsig( SIGCHLD, child_child_handler, true );
	addsig( SIGALRM, child_timer_handler, true );
	addsig( SIGILL, child_term_handler, true );
	addsig( SIGPIPE, SIG_IGN, true );

	if(SessionMng_open() < 0)
	{
	    Log_Trace(LOG_LEVEL_ERR, "SessionMng_open fail!\n");
		goto exit;
	}

	memset(&tick, 0, sizeof(tick));

    tick.it_value.tv_sec = LINK_TIMEVAL;
    tick.it_value.tv_usec = 0;

    tick.it_interval.tv_sec = LINK_TIMEVAL;
    tick.it_interval.tv_usec = 0;

	if( setitimer(ITIMER_REAL, &tick, NULL) < 0 )
	{
	    Log_Trace(LOG_LEVEL_ERR, "Create timer fail!\n");
		goto exit;
	}

    while( !stop_child )
    {
        int number = epoll_wait( child_epollfd, events, MAX_EVENT_NUMBER, -1 );
		
        if ( ( number < 0 ) && ( errno != EINTR ) )
        {
			Log_Trace(LOG_LEVEL_ERR, "epoll failure in %d \n" ,idx);
            break;
        }

        for ( i = 0; i < number; i++ )
        {
            int fd_count;
            int sockfd = events[i].data.fd;

            if( ( sockfd == pipefd ) && ( events[i].events & EPOLLIN ) )
            {
                int client = 0;
                ret = recv( sockfd, ( char* )&client, sizeof( client ), 0 );
                if( ret < 0 )
                {
                    if( errno != EAGAIN )
                    {
                        Log_Trace(LOG_LEVEL_ERR,  "pipe recv errno is: %d\n", errno );
                        stop_child = true;
                    }
                }
                else if( ret == 0 )
                {
                    Log_Trace(LOG_LEVEL_ERR,  "pipe recv errno is: %d\n", errno );
                    stop_child = true;
                }
                else if( NEW_CONN_CMD == client )
                {
                    struct sockaddr_in client_address;
                    socklen_t client_addrlength = sizeof( client_address );

					pClienMng = (Client_Date_Mng_S *)SessionMng_GetShBufHead();
					
                    int connfd = accept( listenfd, ( struct sockaddr* )&client_address, &client_addrlength );
                    if ( connfd < 0 )
                    {
						Log_Trace(LOG_LEVEL_ERR,  "errno is: %d\n", errno);
                        continue;
                    }

					Log_Trace(LOG_LEVEL_INFO, "New connect coming, ip:%s, fd = %d, process index = %d\n", inet_ntoa(client_address.sin_addr), connfd, idx);
			
					if(SessionMng_Alloc(idx, connfd, &pUser) < 0)
					{
					    Log_Trace(LOG_LEVEL_INFO, "SessionMng_Alloc fail!\n");
						continue;
					}

                    //pUser->address = client_address;
					pUser->sessionID = GetSessionID();
					pUser->conn_fd = connfd;
					pClienMng->child_epollfd[idx] = child_epollfd;
                    addfd( child_epollfd, connfd );
                }
				else if( CONN_BYE_CMD == client )
				{
                    //SessionMng_CloseAll(idx, child_epollfd);
				}
            }
            else if( events[i].events & EPOLLIN )
            {
                char szBuff[BUFFER_SIZE];
				int ret;

				ret = Msg_Recv(sockfd, szBuff, BUFFER_SIZE);
				if(ret < 0)
				{
					SessionMng_CloseASession(idx, sockfd);
					Log_Trace(LOG_LEVEL_INFO, "Msg_Recv fail err = %d, ret = %d, fd = %d\n", errno, ret, sockfd);
					continue;
				}

			    MsgProc(idx, sockfd, (stMsg *)szBuff);
            }
            else
            {
                continue;
            }
        }
    }

    SessionMng_CloseAll(idx, child_epollfd);

	Log_Trace(LOG_LEVEL_INFO, "Sub process exit\n");
exit:
    close( pipefd );
    close( child_epollfd );
	DeInit_Log();
    return 0;
}

bool daemonize()
{
    pid_t pid = fork();
	if(pid < 0)
	{
	    return false;
	}
	else if(pid > 0)
	{
	    DeInit_Log();
	    exit(0);
	}

	umask(0);

	pid_t sid = setsid();
	if(sid < 0)
	{
	    return false;
	}

	if(chdir("/") < 0)
	{
	    return false;
	}

	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	open("/dev/null/", O_RDONLY);
	open("/dev/null/", O_RDWR);
	open("/dev/null/", O_RDWR);


	return true;
}

int Init_Instance()
{
	if(access(path, F_OK) < 0)
	{
	    if((fd_instance = open(path, O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO)) < 0)
    	{
    	    return FAIL;
    	}
	}
	else
	{
	    return FAIL;
	}

	return SUCCESS;
}

int DeInit_Instance()
{
    remove(path);
	close(fd_instance);
}


int main( int argc, char* argv[] )
{
    int i = 0;

    if(Init_Instance() < 0)
	{
	   printf("Appsrver has been started, or deleted /tmp/ins restart!\n");
	   exit(1);
	}

	if(daemonize() < 0)
	{
		goto exit_ins;
	}

	if(Init_Log() < 0)
	{
	    goto exit_ins;
	}
	
	if(InitDatabase() < 0)
	{
		Log_Trace(LOG_LEVEL_ERR, "Init database fail!\n");
		goto exit_log;
	}

    Log_Trace(LOG_LEVEL_INFO, "Start Server!\n");
	
    int port = 43200;//atoi( argv[2] );
    int socket_opt_value = 1;
    int ret = 0;

	if(SessionMng_Create() < 0)
	{
	    Log_Trace(LOG_LEVEL_ERR, "SessionMng_Create fail!\n");
		goto exit_database;
	}

	memset(&sub_process, 0, sizeof(struct process_in_pool) * PROCESS_COUNT);
	
    struct sockaddr_in address;
    bzero( &address, sizeof( address ) );
    address.sin_family = AF_INET;
    //inet_pton( AF_INET, ip, &address.sin_addr );
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons( port );

    listenfd = socket( PF_INET, SOCK_STREAM, 0 );
    assert( listenfd >= 0 );

    if (setsockopt(listenfd ,SOL_SOCKET,SO_REUSEADDR,(char *)&socket_opt_value,sizeof(int)) == -1) 
    {
        Log_Trace(LOG_LEVEL_ERR, "SO_REUSEADDR fail!\n");
    }

    ret = bind( listenfd, ( struct sockaddr* )&address, sizeof( address ) );
    if(ret == -1)
	{
	    printf("bind fail err = %d\n", errno);
		close(listenfd);
		goto exit_fd;
	}

    ret = listen( listenfd, LISTEN_NUM );
    assert( ret != -1 );

    for( i = 0; i < PROCESS_COUNT; ++i )
    {
        ret = socketpair( PF_UNIX, SOCK_STREAM, 0, sub_process[i].pipefd );
        assert( ret != -1 );
        sub_process[i].pid = fork();
        if( sub_process[i].pid < 0 )
        {
            continue;
        }
        else if( sub_process[i].pid > 0 )
        {
            close( sub_process[i].pipefd[1] );
            setnonblocking( sub_process[i].pipefd[0] );
            continue;
        }
        else
        {
            Log_Trace(LOG_LEVEL_INFO, "Create sub process %d(pid = %d) success !\n", i, getpid());
			
            close( sub_process[i].pipefd[0] );
            setnonblocking( sub_process[i].pipefd[1] );
            run_child( i );
            exit( 0 );
        }
    }

    struct epoll_event events[ MAX_EVENT_NUMBER ];
    epollfd = epoll_create( LISTEN_NUM );
    assert( epollfd != -1 );
    addfd( epollfd, listenfd );

    ret = socketpair( PF_UNIX, SOCK_STREAM, 0, sig_pipefd );
    assert( ret != -1 );
    setnonblocking( sig_pipefd[1] );
    addfd( epollfd, sig_pipefd[0] );

    addsig( SIGCHLD, sig_handler, true );
    addsig( SIGTERM, sig_handler, true );
    addsig( SIGINT, sig_handler, true );
	addsig( SIGILL, sig_handler, true );
    addsig( SIGPIPE, SIG_IGN, true );
    bool stop_server = false;
    int sub_process_counter = 0;

    while( !stop_server )
    {
        int i = 0, j = 0, g = 0, h = 0, z = 0;
        int number = epoll_wait( epollfd, events, MAX_EVENT_NUMBER, -1 );
        if ( ( number < 0 ) && ( errno != EINTR ) )
        {
            printf( "epoll failure\n" );
            break;
        }

        for ( i = 0; i < number; i++ )
        {
            int sockfd = events[i].data.fd;
            if( sockfd == listenfd )
            {
                int new_conn = NEW_CONN_CMD;
                send( sub_process[sub_process_counter++].pipefd[0], ( char* )&new_conn, sizeof( new_conn ), 0 );
				Log_Trace(LOG_LEVEL_INFO, "send request to child %d\n", sub_process_counter-1);
                sub_process_counter %= PROCESS_COUNT;
            }
            else if( ( sockfd == sig_pipefd[0] ) && ( events[i].events & EPOLLIN ) )
            {
                int sig;
                char signals[1024];
                ret = recv( sig_pipefd[0], signals, sizeof( signals ), 0 );
                if( ret == -1 )
                {
                    continue;
                }
                else if( ret == 0 )
                {
                    continue;
                }
                else
                {
                    for( j = 0; j < ret; ++j )
                    {
                        switch( signals[j] )
                        {
                            case SIGCHLD:
                            {
		                        pid_t pid;
		                        int stat;
		                        while ( ( pid = waitpid( -1, &stat, WNOHANG ) ) > 0 )
                                {
                                    for( g = 0; g < PROCESS_COUNT; ++g )
                                    {
                                        if( sub_process[g].pid == pid )
                                        {
                                            Log_Trace(LOG_LEVEL_INFO, "Sub process(pid = %d) success exit\n", sub_process[g].pid);
                                            close( sub_process[g].pipefd[0] );
                                            sub_process[g].pid = -1;
                                        }
                                    }
                                }
                
                                for( h = 0; h < PROCESS_COUNT; ++h )
                                {
                                    if( sub_process[h].pid == -1 )
                                    {
                                        stop_server = true;
                                    }
                                }
                                break;
                            }
                            case SIGTERM:
                            case SIGINT:
							case SIGILL:
                            {
								Log_Trace(LOG_LEVEL_INFO, "kill all the clild now\n");
                                for( z = 0; z < PROCESS_COUNT; ++z )
                                {
                                    int pid = sub_process[z].pid;
									int pipecmd = CONN_BYE_CMD;
									int ret = 0;

									ret = send( sub_process[z].pipefd[0], ( char* )&pipecmd, sizeof( pipecmd ), 0 );
                                    kill( pid, SIGTERM );
                                }
                                break;
                            }
                            default:
                            {
                                break;
                            }
                        }
                    }
                }
            }
            else 
            {
                continue;
            }
        }
    }

	Log_Trace(LOG_LEVEL_INFO, "End Server!\n");

exit_fd:
    del_resource();
	SessionMng_Destory();
exit_database:
    DeInitDatabase();
exit_log:
	DeInit_Log();
exit_ins:
	DeInit_Instance();

    return 0;
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */

