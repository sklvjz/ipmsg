/**********************************************************
 *Filename: coms.c
 *Author:   星云鹏
 *Date:     2008-05-15
 *
 *主要的数据结构、全局变量和包的解析与生成
 *********************************************************/

#include "ipmsg.h"
#include "coms.h"
#include <semaphore.h>
#include <pthread.h>
#include <sys/types.h>
#include <pwd.h>
#include <sys/utsname.h>
#include <assert.h>

const char allHosts[] = "255.255.255.255"; //广播用地址
int msgSock; //消息
int tcpSock; //文件
struct passwd* pwd; 
struct utsname sysName;
char workDir[FILENAME];
int utf8;

gsNode sendFHead, getFHead; //发送和接收文件列表
msgList mList; //接受到的消息列表

pthread_mutex_t sendFMutex=PTHREAD_MUTEX_INITIALIZER; //发送文件
pthread_mutex_t getFMutex=PTHREAD_MUTEX_INITIALIZER;  //接收文件
pthread_mutex_t msgMutex=PTHREAD_MUTEX_INITIALIZER;   //消息队列
pthread_mutex_t usrMutex=PTHREAD_MUTEX_INITIALIZER;   //消息队列
sem_t waitNonEmpty, waitNonFull; //消息列表信号量

/*解析接受到的字符串*/
int msgParser(char *msg, int size, command* com)//, struct sockaddr_in *peer)
{
  char *start;
  char *pos;
  char outStr[COMLEN];
  int min;
  int index=0,colonCount, tmp;
  
  if ((msg==NULL)||(com==NULL))
    return -1;

  if (utf8)
  {
    g2u(msg, size, outStr, sizeof(outStr));
    start = outStr;
  }
  else start = msg;
  
  bzero(com, sizeof(command));
  while (index<5)
  {
    pos = strchr(start, ':');
    if (pos==NULL)
      break;
    
    colonCount=1;
    while(*(pos+1) == ':')
    {
      pos++;
      colonCount++;
    }
    if (colonCount % 2 == 0) //偶数个：表示不是分割符
    {
      start = pos+1;
      continue;
    }
      
    switch (index)
    {
    case 0:
      com->version = atoi(start);
      break;
    case 1:
      com->packetNo = atoi(start);
      break;
    case 2:
      min = sizeof(com->senderName)<(pos-start)?sizeof(com->senderName):(pos-start);
      memcpy(com->senderName, start, min); 
      com->senderName[min]='\0'; 
      break;
    case 3:
      min = sizeof(com->senderHost)<(pos-start)?sizeof(com->senderHost):(pos-start);
      memcpy(com->senderHost, start, min); 
      com->senderHost[min]='\0';
      break;
    case 4:
      com->commandNo = atoi(start);
      break;
    default:
      break;
    }
    start = pos+1;
    index++;
  }

  strncpy(com->additional, start, MSGLEN);
  if (com->commandNo & IPMSG_FILEATTACHOPT)
  {
    tmp = strlen(com->additional);
    com->fileList = getFilelist(start+tmp+1);
  }

  index++;
  
  return index;
  
    
}

/*生成要发送的message*/
int msgCreater(char* msg, command* com, size_t msgLen)
{
  int len, curLen, tmp, fileNo;
  char fileName[FILENAME], dest[COMLEN];
  filenode *curFile;
  
  if ((dest==NULL)||(com==NULL))
    return -1;
  
  len = snprintf(dest, sizeof(dest), "%d:%d:%s:%s:%d:%s",
                  com->version,
                  com->packetNo,
                  com->senderName,
                  com->senderHost,
                  com->commandNo,
                  com->additional);
  tmp = sizeof(dest) - len - 1;
  if (com->commandNo & IPMSG_FILEATTACHOPT)
  {
    fileNo = 0;
    curFile = com->fileList;
    while ((curFile != NULL) && (tmp>0))
    {
      if (getFileName(fileName, curFile->fileName, sizeof(fileName))<0)
      {
        printf("msgCreater: get fileName error.\n");
        return -1;
      }

      addColon(fileName, sizeof(fileName));
      
      
      if (curFile->fileType == 1)
        curLen = snprintf(dest+len+1, tmp, "%d:%s:%s:%s:1:\a",
                          fileNo, fileName, curFile->fileSize, curFile->mtime);
      else if (curFile->fileType == 2)
         curLen = snprintf(dest+len+1, tmp, "%d:%s:%s:%s:2:\a",
                          fileNo, fileName, curFile->fileSize, curFile->mtime);
      else
      {
        printf("Unsupported file type.\n");
        continue;
      }
      fileNo++;
      curFile = curFile->next;
      len += curLen;
      tmp -= curLen;
    }
    
  }

  if (utf8)
    u2g(dest, sizeof(dest), msg, msgLen);
  else strncpy(msg, dest, msgLen);
  
  return 0;
}

//分析待接收文件列表
/*comFiles格式为xx:xx:xx:\a*/
filenode* getFilelist(const char* comFiles)
{
  char eachFile[NAMELEN];
  const char *pos, *backsplash_a, *colon, *start;
  filenode *head=NULL, *tail=NULL;
  int index, colonCount=0, temp, min;
  
  assert((comFiles!=NULL));

  start = comFiles;

  while (1)
  {
    backsplash_a = strchr(start, '\a');
    if (backsplash_a==NULL)
      break;
    if (tail==NULL)
      head = tail = (filenode*)malloc(sizeof(filenode));
    else
    {
      tail->next = (filenode*)malloc(sizeof(filenode));
      tail = tail->next;
    }
    
    tail->next = NULL;

    index = 0;
    
    pos = start;
    while(1)
    {
      colon = memchr(pos, ':', backsplash_a - pos);
      if (colon == NULL)
        break;
      colonCount=1;
      while(*(colon+1) == ':')
      {
        colon++;
        colonCount++;
      }
      if (colonCount % 2 == 0) //偶数个：表示不是分割符
      {
        pos = colon+1;
        continue;
      }
      
      switch(index)
      {
      case 0:
        sscanf(start, "%x", &tail->fileNo); //sscanf遇到':'也会结束
        break;
      case 1:
        min = (sizeof(tail->fileName)-1)<(colon-start)?(sizeof(tail->fileName)-1):(colon-start);
        memcpy(tail->fileName, start, min);
        tail->fileName[min] = '\0';
        delColon(tail->fileName, sizeof(tail->fileName));//::  ----> :
        break;
      case 2:
        min = (sizeof(tail->fileSize)-1)<(colon-start)?(sizeof(tail->fileSize)-1):(colon-start);
        memcpy(tail->fileSize, start, min);
        tail->fileSize[min] = '\0';
        break;
      case 3:
        min = (sizeof(tail->mtime)-1)<(colon-start)?(sizeof(tail->mtime)-1):(colon-start);
        memcpy(tail->mtime, start, min);
        tail->mtime[min] = '\0';
        break;
      case 4:
        sscanf(start, "%x", &tail->fileType); //sscanf遇到':'也会结束
        break;
      case 5:
        min = (sizeof(tail->otherAttrs)-1)<(colon-start)?(sizeof(tail->otherAttrs)-1):(colon-start);
        memcpy(tail->otherAttrs, start, min);
        tail->otherAttrs[min] = '\0';
        break;
      default:
        break;
      }
      start = colon+1;
      pos = start;
      index++;
    }
    start = backsplash_a + 1;
  }
  return head;
}

//相关数据结构的初始化与清理
void initCommand(command *com, unsigned int flag)
{
  if (com == NULL)
    return;
  bzero(com, sizeof(command));
  com->version = 1;
  com->packetNo = (unsigned int)time(NULL);
  strncpy(com->senderName, pwd->pw_name, sizeof(com->senderName));
  strncpy(com->senderHost, sysName.nodename, sizeof(com->senderHost));
  com->commandNo = flag;
  return;
}

void deCommand(command *com) //delete fileList
{
  filenode *cur;
  cur = com->fileList;
  while (cur!=NULL)
  {
    com->fileList = cur->next;
    free(cur);
    cur = com->fileList;
  }
  com->fileList = NULL;
}

void initGsNode(gsNode *gs)
{
  if (gs == NULL)
    return;
  bzero(gs, sizeof(gsNode));
  gs->tcpSock = -1;
  gs->packetNo = -1;
}

void deGsNode(gsNode *gs) //delete fileList & tragetDir
{
  filenode *curFile;
  if (gs == NULL)
    return;
  while ((curFile=gs->fileList.next)!=NULL)
  {
    gs->fileList.next = curFile->next;
    free(curFile);
  }
  if (gs->targetDir != NULL)
    free(gs->targetDir);
}


