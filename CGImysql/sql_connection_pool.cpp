#include <mysql/mysql.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <list>
#include <pthread.h>
#include <iostream>
#include "sql_connection_pool.h"

using namespace std;

connection_pool::connection_pool()
{
	m_CurConn = 0;
	m_FreeConn = 0;
}

//单例模式获取数据库连接池
connection_pool *connection_pool::GetInstance()  
{
	static connection_pool connPool;
	return &connPool; 
}

//构造初始化
void connection_pool::init(string url, string User, string PassWord, string DBName, int Port, int MaxConn, int close_log)//创建MaxConn个空闲mysql数据库
{
	m_url = url;   //资源定位器
	m_Port = Port;  //端口号
	m_User = User;   //用户名
	m_PassWord = PassWord;  //密码
	m_DatabaseName = DBName;  //库名
	m_close_log = close_log;  //关闭标志

	for (int i = 0; i < MaxConn; i++)  //将数据库全都创建出来
	{
		MYSQL *con = NULL;  
		con = mysql_init(con);  //系统分配一个内存存储mysql对象

		if (con == NULL)  //没分配到内存
		{
			LOG_ERROR("MySQL Error");
			exit(1);
		}
		con = mysql_real_connect(con, url.c_str(), User.c_str(), PassWord.c_str(), DBName.c_str(), Port, NULL, 0);
		//数据库，主机名，用户名，密码，数据库名，端口，套接字，协议

		if (con == NULL)
		{
			LOG_ERROR("MySQL Error");
			exit(1);
		}
		connList.push_back(con);   //连接池添加连接
		++m_FreeConn;
	}

	reserve = sem(m_FreeConn);  //创建信号量

	m_MaxConn = m_FreeConn;  //更新最大连接数为已连接数
}


//当有请求时，从数据库连接池中返回一个可用连接，更新使用和空闲连接数
MYSQL *connection_pool::GetConnection()
{
	MYSQL *con = NULL;

	if (0 == connList.size())
		return NULL;

	reserve.wait();  //信号量-1
	
	lock.lock();  //加锁

	con = connList.front();  //取出连接池的第一个
	connList.pop_front();

	--m_FreeConn;    //空闲-1，使用+1
	++m_CurConn;

	lock.unlock();
	return con;
}

//释放当前使用的连接
bool connection_pool::ReleaseConnection(MYSQL *con)
{
	if (NULL == con)
		return false;

	lock.lock();

	connList.push_back(con);   //退回一个连接
	++m_FreeConn;
	--m_CurConn;

	lock.unlock();

	reserve.post();   //信号量+1
	return true;
}

//销毁数据库连接池
void connection_pool::DestroyPool()
{

	lock.lock();
	if (connList.size() > 0)
	{
		list<MYSQL *>::iterator it;
		for (it = connList.begin(); it != connList.end(); ++it)
		{
			MYSQL *con = *it;
			mysql_close(con);   //销毁数据库
		}
		m_CurConn = 0;
		m_FreeConn = 0;
		connList.clear();
	}

	lock.unlock();
}

//当前空闲的连接数
int connection_pool::GetFreeConn()
{
	return this->m_FreeConn;
}

//销毁数据库
connection_pool::~connection_pool()
{
	DestroyPool();
}

connectionRAII::connectionRAII(MYSQL **SQL, connection_pool *connPool){
	*SQL = connPool->GetConnection();
	
	conRAII = *SQL;
	poolRAII = connPool;
}

connectionRAII::~connectionRAII(){
	poolRAII->ReleaseConnection(conRAII);
}