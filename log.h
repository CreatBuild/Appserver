#ifndef  __LOG_H__
#define  __LOG_H__

#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif /* __cplusplus */

typedef enum tagLog_Priority_E{
    LOG_LEVEL_EMERG,            /* system is unusable */
    LOG_LEVEL_ALERT,            /* action must be taken immediately */
    LOG_LEVEL_CRIT,             /* critical conditions */
    LOG_LEVEL_ERR,              /* error conditions */
    LOG_LEVEL_WARNING,          /* warning conditions */
    LOG_LEVEL_NOTICE,           /* normal but significant condition */
    LOG_LEVEL_INFO,             /* informational */
    LOG_LEVEL_DEBUG,            /* debug-level messages */
}Log_Priority_E;

int Init_Log();

#define Log_Trace(level, args ...) \
	Log_Print(level, __FUNCTION__, __LINE__, args)

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */

#endif //__LOG_H__
