/**********************************************************
 *Filename: send_receive.h
 *Author:   星云鹏
 *Date:     2008-05-15
 *
 *消息、文件（夹）的发送和接收
 *********************************************************/

#ifndef SEND_RECV_H
#define SEND_RECV_H

#include "coms.h"
#include <pwd.h>
#include <sys/utsname.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>

extern void* sendMsg(command* com);
extern int saySth();
extern int selectFiles();
extern void* sendData(void* option);
extern int sendDir(int fSock, const char* fullpath, int fileSize, int fileType);
extern int traverseDir(int fSock, char* fullpath, Mysnd snd);
extern int listSFiles(gsNode **list, gsNode *gs, int size);
extern int ceaseSend();
extern int recvFiles();
extern void* getData(void* option);
extern int getFile(void* option, gsNode *gList);
extern int parseHeader(filenode *pfn, char * recvs);
extern int getDir(void *option, gsNode *gList);
extern int listGFiles(gsNode **list, gsNode *gs, int size);
extern int login();
extern int logout();


#endif
