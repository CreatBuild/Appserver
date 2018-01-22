#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

//#include <mysql/mysql.h>
#include <zdb.h>

#include "erron.h"
#include "log.h"

#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif /* __cplusplus */

const char szurl[] = "mysql://localhost/Gaozhi?user=root&password=Ab-456qaz&charset=utf8\n";
static URL_T url;
ConnectionPool_T pool;

int InitDatabase()
{

	Exception_init();

	if( (url = URL_new(szurl)) == NULL)
	{
	    Log_Trace(LOG_LEVEL_ERR, "URL_new fail!\n");
		return -1;
	}

	pool = ConnectionPool_new(url);
	ConnectionPool_start(pool);
}

int DeInitDatabase()
{
    ConnectionPool_free(&pool);   
	URL_free(&url);
}



#if 0
MYSQL *conn = NULL;


int InitDatabase()
{
    MYSQL_RES *res;
    MYSQL_ROW row;
	
    char server[] = "localhost";
    char user[] = "root";
	char db[] = "Gaozhi";
    char password[] = "qaz";
    char database[] = "mysql";

	conn = mysql_init(NULL);
    
    if (!mysql_real_connect(conn, server,user, password, database, 0, NULL, 0)) 
    {
        return ERR_CONN_DATA;
    }

	if(mysql_select_db(conn, db) == 1)
	{
	    return ERR_SELECT_DB;
	}

	return 0;
}

int DeInitDatabase()
{
    mysql_close(conn);

	return 0;
}
#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */

