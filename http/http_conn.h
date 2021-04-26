#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <map>

#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"
#include "../timer/lst_timer.h"
#include "../log/log.h"

class http_conn
{
public:
    static const int FILENAME_LEN = 200;  //文件长度
    static const int READ_BUFFER_SIZE = 2048;   //读取缓冲区大小
    static const int WRITE_BUFFER_SIZE = 1024;  //写缓冲区大小
    //请求方法
    enum METHOD
    {
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATH
    };
    //主状态机的状态
    enum CHECK_STATE
    {
        CHECK_STATE_REQUESTLINE = 0,    //连接请求
        CHECK_STATE_HEADER,   //连接头部
        CHECK_STATE_CONTENT  //连接内容
    };
    //报文的解析结果
    enum HTTP_CODE
    {
        NO_REQUEST,   //默认0
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURCE,
        FORBIDDEN_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION
    };

    //从状态机的状态
    enum LINE_STATUS
    {
        LINE_OK = 0,
        LINE_BAD,
        LINE_OPEN
    };

public:
    http_conn() {}
    ~http_conn() {}

public:
//初始化连接
    void init(int sockfd, const sockaddr_in &addr, char *, int, int, string user, string passwd, string sqlname);
    //关闭连接
    void close_conn(bool real_close = true);
    void process();
    //读取浏览器发来的全部数据
    bool read_once();
    //响应报文写入
    bool write();
    sockaddr_in *get_address()  //获取连接的地址
    {
        return &m_address;
    }
    //同步线程初始化数据库读取表
    void initmysql_result(connection_pool *connPool);
    int timer_flag;
    int improv;

private:
    void init();
    //从读取缓冲区读入，并处理请求报文
    HTTP_CODE process_read();  
    //向写缓冲区写入响应报文
    bool process_write(HTTP_CODE ret);
    //请求行
    HTTP_CODE parse_request_line(char *text);
    //请求头
    HTTP_CODE parse_headers(char *text);
    //内容
    HTTP_CODE parse_content(char *text);
    //响应请求
    HTTP_CODE do_request();
    //指向未处理的字符
    char *get_line() { return m_read_buf + m_start_line; };
    //读取行
    LINE_STATUS parse_line();
    //取消映射
    void unmap();
    //根据响应报文格式，生成各部分
    bool add_response(const char *format, ...);
    bool add_content(const char *content);
    bool add_status_line(int status, const char *title);
    bool add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();

public:
    static int m_epollfd;
    static int m_user_count;
    MYSQL *mysql;
    int m_state;  //读为0, 写为1

private:
    int m_sockfd;  //连接的页面
    sockaddr_in m_address;
    char m_read_buf[READ_BUFFER_SIZE];  //读缓冲
    int m_read_idx;   //缓冲区中最后一个字节位置
    int m_checked_idx;  //读取位置
    int m_start_line;
    char m_write_buf[WRITE_BUFFER_SIZE];  //写缓冲
    int m_write_idx;
    CHECK_STATE m_check_state;  //检查的状态
    METHOD m_method;  //处理位置
    char m_real_file[FILENAME_LEN];  //读取文件的名称
    char *m_url;
    char *m_version;
    char *m_host;
    int m_content_length;
    bool m_linger;  //保持连接标志，长连接
    char *m_file_address;    //服务器上的文件地址
    struct stat m_file_stat;
    struct iovec m_iv[2];  //两个缓冲区
    int m_iv_count;
    int cgi;        //是否启用的POST
    char *m_string; //存储请求头数据
    int bytes_to_send;  //待发送
    int bytes_have_send;  //已发送
    char *doc_root;   //根目录

    map<string, string> m_users;  //用户账户密码
    int m_TRIGMode;  //ET/LT
    int m_close_log;  //关闭日志

    char sql_user[100];
    char sql_passwd[100];
    char sql_name[100];
};

#endif
