/**********************************************************
 *Filename: main.c
 *Author:   星云鹏
 *Date:     2008-05-15
 *
 *主程序、各个线程
 *********************************************************/


#include "users.h"
#include "ipmsg.h"
#include "send_receive.h"
#include "coms.h"
#include <langinfo.h>
#include <locale.h>
#include <pthread.h>

extern void destroyer();

//用户输入
void* interacter(void* option)
{
  char com[20], *fileName;
  int count;

  while(1)
  {
    printf("\n(ipmsg):");
    fgets(com, sizeof(com), stdin);

    transfStr(com, 1); //去掉开头结尾空白字符，变成全小写

    if (!strcmp(com, "list") ||
        !strcmp(com, "ls"))
    {
      pthread_mutex_lock(&usrMutex);
      count = listUsers(NULL, &userList, 2, 1);//只是列出
      pthread_mutex_unlock(&usrMutex);
    }
    else if (!strcmp(com, "quit") ||
             !strcmp(com, "q"))
    {
      logout();
      destroyer();
      exit(0);
    }
    else if (!strcmp(com, "refresh") ||
             !strcmp(com, "rf"))
    {
      login();
      pthread_mutex_lock(&usrMutex);
      count = listUsers(NULL, &userList, 2, 1);//只是列出
      pthread_mutex_unlock(&usrMutex);
    }
    else if (!strcmp(com, "talk") ||
             !strcmp(com, "tk"))
    {
      saySth();
    }
    else if (!strcmp(com, "sendfile") ||
             !strcmp(com, "sf"))
    {
      selectFiles();
    }
    else if (!strcmp(com, "getfile") ||
             !strcmp(com, "gf"))
    {
      recvFiles();
    }
    else if (!strcmp(com, "ceaseSend") ||
             !strcmp(com, "cs"))
    {
      ceaseSend();
    }
    else if (!strcmp(com, "help") ||
             !strcmp(com, "h"))
      printf(IMHELP);
  }

}

//接收udp包
void* receiver(void *option)
{
  command *peercom;
  struct sockaddr_in peer;
  int mSock = *(int*)option, len;
  char buf[COMLEN];
  
  while(1)
  {
    if (recvfrom(mSock, buf, sizeof(buf), 0, (struct sockaddr*)&peer, &len)<0)
      continue;
    peercom = (command*)malloc(sizeof(command));
    bzero(peercom, sizeof(command));
    msgParser(buf, sizeof(buf), peercom);
    memcpy(&peercom->peer, &peer, sizeof(peercom->peer));

    sem_wait(&waitNonFull);
    pthread_mutex_lock(&msgMutex);

    mList.comTail->next = peercom;
    mList.comTail = peercom;

    sem_post(&waitNonEmpty); 
    pthread_mutex_unlock(&msgMutex);
  }
}

//处理接收的udp包
void* processor(void *option)
{
  command *peercom, com;
  int comMode, comOpt, temp;
  int len;
  user *cur;
  filenode *head, *curFile;
  gsNode *preSend, *curSend, *curGet, *preGet;

  initCommand(&com, IPMSG_NOOPERATION);
  while(1)
  {
    sem_wait(&waitNonEmpty);
    pthread_mutex_lock(&msgMutex);
    
    peercom = mList.comHead.next;
    mList.comHead.next = mList.comHead.next->next;
    if (mList.comHead.next == NULL)
      mList.comTail = &mList.comHead;
    
    sem_post(&waitNonFull); 
    pthread_mutex_unlock(&msgMutex);
    
    memcpy(&com.peer, &peercom->peer, sizeof(com.peer));
    
    comMode = GET_MODE(peercom->commandNo);
    comOpt = GET_OPT(peercom->commandNo);

    if (comOpt & IPMSG_SENDCHECKOPT)
    {
      com.packetNo = (unsigned int)time(NULL);
      snprintf(com.additional, MSGLEN, "%d", peercom->packetNo);
      com.commandNo = IPMSG_RECVMSG;
      sendMsg(&com); //发送回应
    }
    
    switch (comMode)
    {
    case IPMSG_SENDMSG: //发送命令
      if (strlen(peercom->additional)>0)
      {
        printf("\nGet message from: %s(%s)\n", peercom->senderName, peercom->senderHost);
        puts(peercom->additional);
      }

      if (comOpt & IPMSG_FILEATTACHOPT)
      {
        printf("\nGet Files from: %s(%s).\nInput \"getfile(gf)\" to download.\n",
               peercom->senderName, peercom->senderHost);
        curGet = (gsNode*)malloc(sizeof(gsNode));
        initGsNode(curGet);
        memcpy(&curGet->peer, &peercom->peer, sizeof(curGet->peer));
        curGet->packetNo = peercom->packetNo;
        curGet->fileList.next = peercom->fileList;
        peercom->fileList = NULL; //
        
        preGet = &getFHead;
        pthread_mutex_lock(&getFMutex);
        while ((preGet->next!=NULL) &&
               (preGet->next->packetNo!=curGet->packetNo))
          preGet = preGet->next;
        
        if (preGet->next==NULL)
          preGet->next = curGet;
        
        pthread_mutex_unlock(&getFMutex);
      }
      
      break;
    case IPMSG_ANSENTRY: //
      cur = (user*)malloc(sizeof(user));
      memcpy(&cur->peer, &peercom->peer, sizeof(cur->peer));
      strncpy(cur->name, peercom->senderName, NAMELEN);
      strncpy(cur->host, peercom->senderHost, NAMELEN);
      strncpy(cur->nickname, peercom->additional, NAMELEN);
      cur->inUse = 0;
      cur->exit = 0;
      cur->next = NULL;
      pthread_mutex_lock(&usrMutex); //lock
      if (insertUser(&userList, cur)<0)
        free(cur);
      pthread_mutex_unlock(&usrMutex); //unlock
      break;
    case IPMSG_BR_ENTRY:
      com.packetNo = (unsigned int)time(NULL);
      com.commandNo = IPMSG_ANSENTRY;//
      strncpy(com.additional, pwd->pw_name, MSGLEN);
      sendMsg(&com);

      cur = (user*)malloc(sizeof(user));
      memcpy(&cur->peer, &peercom->peer, sizeof(cur->peer));
      strncpy(cur->name, peercom->senderName, NAMELEN);
      strncpy(cur->host, peercom->senderHost, NAMELEN);
      strncpy(cur->nickname, peercom->additional, NAMELEN);
      cur->inUse = 0;
      cur->exit = 0;
      cur->next = NULL;
      pthread_mutex_lock(&usrMutex);
      if (insertUser(&userList, cur)<0)
        free(cur);
      pthread_mutex_unlock(&usrMutex);
      break;
    case IPMSG_RECVMSG:
      //
      break;
    case IPMSG_RELEASEFILES: //应该验证一下peer
      preSend = &sendFHead;
      pthread_mutex_lock(&sendFMutex);
      curSend = sendFHead.next;
      while ((curSend!=NULL)&&(curSend->packetNo!=atoi(peercom->additional)))
      {
        preSend = preSend->next;
        curSend = curSend->next;
      }
      
      if (curSend!=NULL)
      {
        curSend->cancelled = 1;
        if (curSend->tcpSock>0)
          shutdown(curSend->tcpSock, SHUT_RDWR);
      }
      pthread_mutex_unlock(&sendFMutex);
      break;
    case IPMSG_BR_EXIT:
      pthread_mutex_lock(&usrMutex);
      delUser(&userList, peercom);
      pthread_mutex_unlock(&usrMutex);
      break;
    case IPMSG_NOOPERATION:
      //
      break;
    default:
      printf("\nno handle, %x\n", peercom->commandNo);
      break;
    }
    deCommand(peercom);
    free(peercom);
    peercom = NULL;
  }
  
}

//数据清理
void destroyer()
{
  gsNode *preSend, *curSend, *preGet, *curGet;
  filenode *curFile;
  user *curUsr, *preUsr;
  
  preSend = &sendFHead;
  pthread_mutex_lock(&sendFMutex);
  curSend = sendFHead.next;
  while (curSend!=NULL)
  {
    if ((curSend->cancelled == 1) && (curSend->transferring==0))
    {
      preSend->next = curSend->next;
      deGsNode(curSend);
      free(curSend);
    }
    else preSend = preSend->next;
    
    curSend = preSend->next;
  }
  pthread_mutex_unlock(&sendFMutex);
  
  
  preGet = &getFHead;
  pthread_mutex_lock(&getFMutex);
  curGet = getFHead.next;
  while (curGet!=NULL)
  {
    if ((curGet->cancelled==1) &&(curGet->transferring==0))
    {
      preGet->next = curGet->next;
      deGsNode(curGet);
      free(curGet);
    }
    else preGet = preGet->next;
    
    curGet = preGet->next;
  }
  pthread_mutex_unlock(&getFMutex);

  preUsr = &userList;
  pthread_mutex_lock(&usrMutex);
  curUsr = userList.next;
  while (curUsr!=NULL)
  {
    if ((curUsr->exit==1) && (curUsr->inUse==0))
    {
      preUsr->next = curUsr->next;
      free(curUsr);
    }
    else preUsr = preUsr->next;
    
    curUsr = preUsr->next;
    
  }
  pthread_mutex_unlock(&usrMutex);
}



void* cleaner(void *option)
{
  gsNode *preSend, *curSend, *preGet, *curGet;
  filenode *curFile;
  user *curUsr, *preUsr;
  
  while(1)
  {
    sleep(30); //半分钟一次
    destroyer();
  }
  
}

//初始化udp和tcp
int initSvr()
{
  struct sockaddr_in server;
  char targetHost[NAMELEN];
  const int on=1;

  msgSock = socket(AF_INET, SOCK_DGRAM, 0);  //UDP for Msg
  
  tcpSock = socket(AF_INET, SOCK_STREAM, 0); //TCP for File
  
  server.sin_family = AF_INET;
  server.sin_port = htons(IPMSG_DEFAULT_PORT);
  server.sin_addr.s_addr = htonl(INADDR_ANY);
  
  if (setsockopt(msgSock, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on))<0)
  {
    printf("initSvr: Socket options set error.\n");
    exit(1);
  }

  if (bind(msgSock, (struct sockaddr*)&server, sizeof(server))<0)
  {
    printf("initSvr: Udp socket bind error.\n");
    exit(1);
  }

  if (bind(tcpSock, (struct sockaddr*)&server, sizeof(server))<0)
  {
    printf("initSvr: Tcp socket bind error.\n");
    exit(1);
  }

  if (listen(tcpSock, 10)<0)
  {
    printf("initSvr: Tcp listen error.\n");
    exit(1);
  }
  
  //printf("Commands: list(ls) talk(tk) sendfile(sf)\n"
  //"Commands: getfile(gf) refresh(rf) ceaseSend(cs) quit(q)\n");
  printf(IMHELP);
}

int main (int argc, char *argv [])
{
  pthread_t procer, recver, iter, fler, cler;
  int *fSock;
  int tmp;
  pthread_attr_t attr;
  
  uname(&sysName);
  pwd = getpwuid(getuid());
  getcwd(workDir, sizeof(workDir));

  utf8 = 0;
  if (setlocale(LC_CTYPE, ""))
    if (!strcmp(nl_langinfo(CODESET), "UTF-8"))
      utf8 = 1;
  
  initGsNode(&sendFHead);
  initCommand(&mList.comHead, IPMSG_NOOPERATION);
  mList.comTail = &mList.comHead;
  userList.next = NULL;
  sem_init(&waitNonEmpty, 0, 0);
  sem_init(&waitNonFull, 0, MSGLIMIT);

  initSvr();
  pthread_create(&procer, NULL, &processor, &msgSock); 
  pthread_create(&recver, NULL, &receiver, &msgSock);
  pthread_create(&iter, NULL, &interacter, NULL);
  pthread_create(&cler, NULL, &cleaner, NULL);
  login();

  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  while(1)
  {
    if ((tmp=accept(tcpSock, NULL, NULL))<0)
      printf("Tcp accept error.\n");
    else
    {
      fSock = (int *)malloc(sizeof(int));
      *fSock = tmp;
      pthread_create(&fler, &attr, &sendData, fSock);
    }
    
  }

  pthread_join(procer, NULL);
  pthread_join(recver, NULL);
  pthread_join(iter, NULL);
  pthread_join(cler, NULL);
  return 0;
}

