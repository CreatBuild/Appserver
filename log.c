#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif /* __cplusplus */
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <stdarg.h>
#include <pthread.h>
#include <errno.h>


#include "log.h"


#define INFO_PATH             "/var/log/Appserver/" 
#define DEBUG_PATH            "/tmp/Appserver/" 
#define INFO_FILENAME         "Appserver_info"
#define DEBUG_FILENAME        "Appserver_debug"
#define EXTERNTION            ".log"

static int info_fd = -1;
static int debug_fd = -1;
static FILE *fp_info = NULL;
static FILE *fp_debug = NULL;

const char* LevelStr[] = {
	"EMERG",
	"ALERT",
	"CRIT",
	"ERR",
	"WARNING",
	"NOTICE",
	"INFO",
	"DEBUG"
};

static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;


int Init_Log()
{
    struct tm *pstTime = NULL;
	time_t      times;
	
    char path[256];
	char szDate[32];

    times = time(NULL);
    pstTime = localtime(&times);
	snprintf(szDate, sizeof(szDate), "-%04d-%02d-%02d", pstTime->tm_year+1900,\
        pstTime->tm_mon+1, pstTime->tm_mday);
	
    if(access(INFO_PATH, F_OK) < 0)
	{
		if(mkdir(INFO_PATH, S_IRWXU | S_IRWXG | S_IRWXO) < 0)
		{
		    return -1;
		}
	}

	strcpy(path, INFO_PATH);
	strcat(path, INFO_FILENAME);
	strcat(path, szDate);
	strcat(path, EXTERNTION);
	if((info_fd = open(path, O_RDWR | O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO)) < 0)
	{
	    return -1;
	}

	if(access(DEBUG_PATH, F_OK) < 0)
	{
		if(mkdir(DEBUG_PATH, S_IRWXU  | S_IRWXG | S_IRWXO) < 0)
		{
		    return -1;
		}
	}

	strcpy(path, DEBUG_PATH);
	strcat(path, DEBUG_FILENAME);
	strcat(path, szDate);
	strcat(path, EXTERNTION);
	if((debug_fd = open(path, O_RDWR | O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO)) < 0)
	{
	    return -1;
	}

	fp_info = fdopen(info_fd, "a+");
	if(fp_info == NULL)
	{
	    return -1;
	}

	fp_debug = fdopen(debug_fd, "a+");
	if(fp_debug == NULL)
	{
	    return -1;
	}

	return 0;
}


int ReInit_Log()
{
    if(DeInit_Log() < 0)
	{
	    return -1;
	}

	if(Init_Log() < 0)
	{
	    return -1;
	}

	return 0;
}


int Log_Print(int level, const char *pszfunc, const int line, const char *pszFmt, ...)
{
	struct tm *pstTime = NULL;
	static int curr_day = 0;
	time_t      times;
	char szDate[32];
	char szPID[16];
	char szTemp[512];
	char szMsg[640];
	va_list vaArgs;
	FILE *fp;

	pthread_mutex_lock(&log_mutex);

	times = time(NULL);
    pstTime = localtime(&times);
    snprintf(szDate, sizeof(szDate), "%04d-%02d-%02d %02d:%02d:%02d", pstTime->tm_year+1900,\
        pstTime->tm_mon+1, pstTime->tm_mday,  pstTime->tm_hour, pstTime->tm_min, pstTime->tm_sec);

	if(0 == curr_day)
	{
	    curr_day = pstTime->tm_mday;
	}

	if(curr_day != pstTime->tm_mday)
	{
	    if(ReInit_Log())
    	{
    	
    	}
	}

	sprintf(szPID, "PID:%d", getpid());

	va_start( vaArgs, pszFmt );
	vsprintf(szTemp, pszFmt, vaArgs);
	va_end( vaArgs );

    sprintf(szMsg, "[%s][%s][%s:%d][%s]:%s", szDate, szPID, pszfunc, line, LevelStr[level], szTemp);

    if(level < LOG_LEVEL_DEBUG)
	{
		fputs(szMsg, fp_info);
		fflush(fp_info);
	}
	fputs(szMsg, fp_debug);
	fflush(fp_debug);

	pthread_mutex_unlock(&log_mutex);

	return 0;
}

int DeInit_Log()
{
    close(info_fd);
	close(debug_fd);

	return 0;
}


#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */


