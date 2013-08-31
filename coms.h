/**********************************************************
 *Filename: coms.h
 *Author:   星云鹏
 *Date:     2008-05-15
 *
 *主要的数据结构、全局变量和包的解析与生成
 *********************************************************/


#ifndef COMMAND_H
#define COMMAND_H

#include <unistd.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <semaphore.h>

#define NAMELEN 50
#define MSGLEN  1000
#define COMLEN  1500
#define RECFRG  1448
#define HSIZE   10
#define FILENAME 128
#define MSGLIMIT 100
#define HL_HEADERSIZE	4
#define HL_FILESIZE	9
#define HL_FILETYPE	1
#define HL_1416		11
#define CAPACITY     50

#define IMHELP    \
  "Commands: help(h) list(ls) talk(tk) sendfile(sf)\n"\
  "Commands: getfile(gf) refresh(rf) ceaseSend(cs) quit(q)\n"

typedef int (*Mysnd)(int, const char*, int, int);

typedef struct filenode filenode;

typedef struct command
{
  unsigned int version;
  unsigned int packetNo;
  char         senderName[NAMELEN];
  char         senderHost[NAMELEN];
  unsigned int commandNo;
  char         additional[MSGLEN];
  struct sockaddr_in peer;
  filenode *   fileList;
  struct command *next;
}command;

struct filenode //文件序号:文件名:大小(单位:字节):最后修改时间:文件属性 [: 附加属性=val1[,val2…][:附加信息=…]]:\a文件序号…
{
  int    tcpSock;
  unsigned int    fileNo;
  char   fileName[FILENAME];
  char   fileSize[NAMELEN];
  char   mtime[NAMELEN];
  int    fileType;
  char   otherAttrs[2*NAMELEN];
  struct filenode* next;
};

typedef struct gsNode
{
  int tcpSock;
  struct sockaddr_in peer;
  unsigned int packetNo;
  int	 transferring;
  int    cancelled;
  char  *targetDir;
  filenode fileList;
  struct gsNode *next;
} gsNode;

typedef struct msgList
{
  command comHead;
  command *comTail;
} msgList;


extern const char allHosts[]; //广播用地址
extern int msgSock; //消息
extern int tcpSock; //文件
extern struct passwd* pwd; 
extern struct utsname sysName;
extern char workDir[FILENAME];
extern int utf8; //系统的LC_CTYPE

extern gsNode sendFHead, getFHead; //发送和接收文件列表
extern msgList mList; //接受到的消息列表

extern pthread_mutex_t sendFMutex; //发送文件
extern pthread_mutex_t getFMutex;  //接收文件
extern pthread_mutex_t msgMutex;   //消息队列
extern pthread_mutex_t usrMutex;   //消息队列
extern sem_t waitNonEmpty, waitNonFull; //消息列表信号量

extern int msgParser(char *msg, int size, command* com);
extern int msgCreater(char* msg, command* com, size_t msgLen);
extern filenode* getFilelist(const char* comFiles);
extern void initCommand(command *com, unsigned int flag);
extern void deCommand(command *com);
extern void initGsNode(gsNode *gs);
extern void deGsNode(gsNode *gs);

#endif
