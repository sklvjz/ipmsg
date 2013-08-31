/**********************************************************
 *Filename: send_receive.c
 *Author:   星云鹏
 *Date:     2008-05-15
 *
 *消息、文件（夹）的发送和接收
 *********************************************************/

#include "send_receive.h"
#include "ipmsg.h"
#include "users.h"
#include "coms.h"

#include <sys/utsname.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <pwd.h>
#include <sys/utsname.h>
#include <signal.h>
#include <pthread.h>


//////////////////////////////////////////////////
//发送udp包
void* sendMsg(command* com)
{
  char buf[COMLEN];
  int len, temp;
  
  msgCreater(buf, com, sizeof(buf));
  len = strlen(buf);
  if (com->commandNo & IPMSG_FILEATTACHOPT)
    {
      temp = strlen(buf+len+1);
      
      sendto(msgSock, buf, len+temp+2, 0,
             (struct sockaddr*)&com->peer, sizeof(com->peer));
     }
  else sendto(msgSock, buf, len+1, 0,
              (struct sockaddr*)&com->peer, sizeof(com->peer));
}

//////////////////////////////////////////////////
//聊天
int saySth()
{
  char buf[100]; //临时输入区
  char gMsg[MSGLEN];
  command com;
  user *cur=NULL;
  int remainder; //扩展区剩余空间
  int pos;       //扩展区的当前输入位置
  int who, temp, count;
  int sended;      //标记此包是否已经发过
  user *pusers[50];

  printf("\n*Talking mode\n"
         "*Continuous Enter to send msg,\n"
         "*Ctrl+D to quit conversation.\n");
  
  pthread_mutex_lock(&usrMutex);
  count = listUsers(pusers, &userList, sizeof(pusers)/sizeof(pusers[0]), 0);
  pthread_mutex_unlock(&usrMutex);
  
  who = inputNo(1, count, 1, "Please input user No.[1]:");
  
  if (who>0)
  {
    cur = pusers[who-1];
    
    initCommand(&com, IPMSG_SENDMSG|IPMSG_SENDCHECKOPT);
    memcpy(&com.peer, &cur->peer, sizeof(com.peer));
    
    remainder = MSGLEN;
    pos = 0;
    sended = 1;
    
    while(1)
    {
      printf("(talking with %s):", cur->name);
      if (fgets(buf, sizeof(buf), stdin)==NULL)
        break;
      if (buf[0] != '\n')
      {
        strncpy(com.additional+pos, buf, remainder);
        temp = strlen(com.additional+pos);
        pos += temp;
        remainder -= temp;
        sended = 0;
      }

      if ((buf[0]=='\n') || (remainder<=1))
      {
        if (!sended)
        {
          com.packetNo = (unsigned int)time(NULL);
          sendMsg(&com);
          sended = 1;
          printf("Message sent.\n");
        }
        remainder = sizeof(com.additional);
        pos = 0;
      }
      
    }
  }
  
    puts("\nEnd conversation.\n");
    
    pthread_mutex_lock(&usrMutex);
    unListUsers(pusers, count);
    pthread_mutex_unlock(&usrMutex);
  
}

/////////////////////////////////////////////////
//文件发送
//选择要传输的文件

int selectFiles()
{
  command com;
  user *cur=NULL;
  int  who, count, flag, fileType;
  unsigned int fileNo;
  char fileName[FILENAME];
  struct stat fileAttr;
  char *strtmp;
  filenode *fntmp, *head, *tail;
  gsNode *newTask;
  user *pusers[50];
  
  printf("\n*Sending mode\n"
         "*Continuous Enter to send file,\n"
         "*Ctrl+D to quit.\n");
  
  pthread_mutex_lock(&usrMutex);
  count = listUsers(pusers, &userList, sizeof(pusers)/sizeof(pusers[0]), 0);
  pthread_mutex_unlock(&usrMutex);

  who = inputNo(1, count, 1, "Please input user No.[1]:");

  if (who>0)
  {
    
    cur = pusers[who-1];
  
    initCommand(&com, IPMSG_SENDMSG|IPMSG_FILEATTACHOPT);
    memcpy(&com.peer, &cur->peer, sizeof(com.peer));
    
    printf("To send file to %s(%s).\nPlease select file to send:\n", cur->name, cur->host);
    
    newTask = (gsNode*)malloc(sizeof(gsNode));
    initGsNode(newTask);
    newTask->packetNo = com.packetNo;
    
    fileNo = 0;
    head = com.fileList;
    tail = com.fileList;
    
    while (1)
    {
      if (fgets(fileName, FILENAME, stdin) == NULL) //取消传输
      {
        free(newTask);
        newTask = NULL;
        while (head!=NULL)
        {
          tail = head;
          head = head->next;
          free(tail);
        }
        break;
      }

       transfStr(fileName, 0); //去除前后空白字符
       
      if (fileName[0]=='\0')
        break;
      
     if (lstat(fileName, &fileAttr)<0)
      {
        printf("Get file attributes error.\n");
        continue;
      }

      if (S_ISREG(fileAttr.st_mode))
        fileType = 1;
      else if (S_ISDIR(fileAttr.st_mode))
        fileType = 2;
      else
      {
        fileType = -1;
        printf("Unsupported file type.\n");
        continue;
      }
      
      if (tail == NULL)
        head = tail = (filenode*)malloc(sizeof(filenode));
      else
      {
        tail->next = (filenode*)malloc(sizeof(filenode));
        tail = tail->next;
      }
      
      tail->next = NULL;
      tail->fileNo = fileNo;
      strncpy(tail->fileName, fileName, sizeof(tail->fileName));
      snprintf(tail->fileSize, sizeof(tail->fileSize), "%x", fileAttr.st_size);
      snprintf(tail->mtime, sizeof(tail->mtime), "%x", fileAttr.st_mtime);
      tail->fileType = fileType;

      fileNo++;
    }
    
    if (head==NULL)
    {
      if (newTask!=NULL)
        free(newTask);
    }
    else 
    {
      newTask->fileList.next = com.fileList = head;
      pthread_mutex_lock(&sendFMutex); //lock
      newTask->next = sendFHead.next;
      sendFHead.next = newTask;
      pthread_mutex_unlock(&sendFMutex); //unlock

      if (newTask->fileList.next!=NULL)
      {
        sendMsg(&com); //可以放lock外面
        printf("\nWaiting to transfer.\n");
      }
    }
  }

  pthread_mutex_lock(&usrMutex);
  unListUsers(pusers, count);
  pthread_mutex_unlock(&usrMutex);
  
}

//文件或文件夹发送
void* sendData(void* option)
{
  int fSock = *(int *)option;
  char buf[RECFRG], fileName[FILENAME];
  int i, commandNo, realCount, offset, curErr;
  unsigned int packetNo, fileNo;
  filenode *preFile, *curFile;
  gsNode *preSend, *curSend;
  FILE* sfile;
  sigset_t mask, oldmask;

  free(option);
  
  sigemptyset(&mask);
  sigaddset(&mask, SIGPIPE);
  if (pthread_sigmask(SIG_BLOCK, &mask, &oldmask) != 0)
    printf("SIG_BLOCK error.\n");
  
  //以tcp的方式接受传输请求
  for (i=0;i<4;i++)
  {
    if (readDelimiter(fSock, buf, RECFRG, ':')<=0)
    {
      printf("Transfer cancelled.\n");
      shutdown(fSock, SHUT_RDWR);
      return NULL;
    }  
  }

  if (readDelimiter(fSock, buf, RECFRG, ':')<=0)
  {
    printf("Transfer cancelled.\n");
    shutdown(fSock, SHUT_RDWR);
    return NULL;
  }
  
  commandNo = atoi(buf);

  if (!(commandNo & IPMSG_GETFILEDATA))
  {
    printf("Invalid request.\n");
    shutdown(fSock, SHUT_RDWR);
    return NULL;
  }
  
  
  if (readDelimiter(fSock, buf, RECFRG, ':')<=0)
  {
    printf("Transfer cancelled.\n");
    shutdown(fSock, SHUT_RDWR);
    return NULL;
  }
  
  sscanf(buf, "%x", &packetNo);

  if (readDelimiter(fSock, buf, RECFRG, ':')<0)
  {
    printf("Transfer cancelled.\n");
    shutdown(fSock, SHUT_RDWR);
    return NULL;
  }
  sscanf(buf, "%x", &fileNo);

  pthread_mutex_lock(&sendFMutex);
  
  preSend = &sendFHead;
  curSend = sendFHead.next;

  while ((curSend != NULL) &&
         (curSend->packetNo!=packetNo || curSend->transferring==1 ||
         curSend->cancelled==1)){
    preSend = preSend->next;
    curSend = curSend->next;
  }
  
  if (curSend != NULL)
  {
    curSend->transferring = 1;
    curSend->tcpSock = fSock;
    pthread_mutex_unlock(&sendFMutex);
    
    curFile = curSend->fileList.next;
    preFile = &curSend->fileList;
    while ((curFile!=NULL) && (curFile->fileNo!=fileNo))
    {
      preFile = preFile->next;
      curFile = curFile->next;
    }
    
    if (curFile != NULL)
    {
      getFileName(fileName, curFile->fileName, sizeof(fileName));
      printf("\nStart transferring %s.\n", fileName);
      switch (curFile->fileType)
      {
      case 1: //发送文件
        if (readDelimiter(fSock, buf, RECFRG, ':')<=0) //offset似乎没用上
        {
          printf("Transfer cancelled.\n");
          shutdown(fSock, SHUT_RDWR);
          return NULL;
        }
        sscanf(buf, "%x", &offset);
        
        sfile = fopen(curFile->fileName, "r");
        
        while ((realCount = fread(buf, 1, RECFRG, sfile))>0)
          if (writen(fSock, buf, realCount)<0)
          {
            curErr = errno;
            break;
          }
        break;
      case 2: //发送文件夹
        curErr = traverseDir(fSock, curFile->fileName, sendDir); 
        break;
      default:
        break;
      }
    }
  }
  else pthread_mutex_unlock(&sendFMutex);

  pthread_mutex_lock(&sendFMutex);
  if ((curSend!=NULL) && (curSend->cancelled==1))
  {
    preSend->next = curSend->next;
    deGsNode(curSend);
    free(curSend);
    pthread_mutex_unlock(&sendFMutex);
    shutdown(fSock, SHUT_RDWR);
    printf("Transfer canceled.\n");
    return NULL;
  }
  
  if ((curErr<0) || (errno == ECONNRESET) || (errno == EPIPE)) //error or connection reset by peer
  {
    if (curFile!=NULL)
    {
      curSend->transferring = 0;
      curSend->tcpSock = -1;
    }
    pthread_mutex_unlock(&sendFMutex);
    shutdown(fSock, SHUT_RDWR);
    printf("Peer needs retransfer.\n");
    return NULL;
  }
  
  if (curFile!=NULL)
  {
    printf("\n%s is transferred.\n", fileName);
    preFile->next = curFile->next;
    free(curFile);
  }
  
  
  if (curSend!=NULL && curSend->fileList.next == NULL)
  {
    preSend->next = curSend->next;
    deGsNode(curSend);
    free(curSend);
  }
  else if (curSend!=NULL)
  {
    curSend->transferring = 0;
    curSend->tcpSock = -1;
  }
  pthread_mutex_unlock(&sendFMutex);
  
  shutdown(fSock, SHUT_RDWR);
}

//发送文件夹

int sendDir(int fSock, const char* fullpath, int fileSize, int fileType)
{
  char strtmp[FILENAME], strformat[50], strdata[RECFRG], fileName[FILENAME];
  int tmp, headLen, packetNo;
  FILE *sf;

  if (getFileName(strtmp, fullpath, sizeof(strtmp)) < 0)
  {
    printf("\nFilename is too long.\n");
    return -1;
  }

  addColon(strtmp, sizeof(strtmp));
  if (utf8)
    u2g(strtmp, sizeof(strtmp),
        fileName, sizeof(fileName));
  else strncpy(fileName, strtmp, sizeof(fileName));
  
  headLen = (strlen(fileName)+1) + (HL_HEADERSIZE+1) +
    (HL_FILETYPE+1) + (HL_FILESIZE+1) + 2*(HL_1416+1);
  packetNo = (unsigned int)time(NULL); //简化了，属性值并不准确
  snprintf(strformat, sizeof(strformat), "%%0%dx:%%s:%%0%dx:%%0%dx:14=%%0%dx:16=%%0%dx:",
           HL_HEADERSIZE, HL_FILESIZE, HL_FILETYPE, HL_1416-3, HL_1416-3);
  
  tmp = snprintf(strdata, sizeof(strdata), strformat,
           headLen, fileName, fileSize, fileType, packetNo, packetNo);
  
  switch (fileType)
  {
  case 1:
    if ((sf = fopen(fullpath, "r")) == NULL)
    {
      printf("file open error.\n");
      return -1;
    }
    if (writen(fSock, strdata, tmp)<0)
      return -1;
    while ((tmp = fread(strdata, 1, RECFRG, sf))>0)
    {
      if (writen(fSock, strdata, tmp)<0)
        return -1;
    }
    fclose(sf);
    break;
  case 2:
   
    //break;
  case 3:
    if (writen(fSock, strdata, tmp)<0)
      return -1;
    break;
  default:
    break;
  }
  return 0;
}

//遍历要发送的文件夹，利用上面的回掉函数发送
int traverseDir(int fSock, char* fullpath, Mysnd snd) //FILENAME指定了fullpath的容量
{
  struct stat fst;
  struct dirent *dirp;
  char *ptr;
  DIR  *dp;
  int tmp;
  

  if (lstat(fullpath, &fst)<0)
  {
    printf("\nDir: get attributes error.\n");
    return -1;
  }
  
  if (S_ISREG(fst.st_mode))
    return snd(fSock, fullpath, fst.st_size, 1);
  else if (S_ISDIR(fst.st_mode))
  {
    if (snd(fSock, fullpath, 0, 2)<0)
      return -1;
  }
  else return -1;

  tmp = strlen(fullpath);
  ptr = fullpath + tmp;
  
  *ptr++ = '/'; //tmp+1
  *ptr = '\0';

  if ((dp=opendir(fullpath)) == NULL)
  {
    printf("\nDir: open error.\n");
    return -1;
  }

  while ((dirp=readdir(dp)) != NULL)
  {
    if (strcmp(dirp->d_name, ".")==0 ||
        strcmp(dirp->d_name, "..")==0)
      continue;

    strncpy(ptr, dirp->d_name, FILENAME-tmp-1);
    if (traverseDir(fSock, fullpath, snd)<0)
      return -1;
  }

  ptr[-1] = '\0'; //还原fullname
  snd(fSock, ".", 0, 3);
  if (closedir(dp) < 0)
  {
    printf("\nDir: close error.\n");
    return -1;
  }
  return 0;
}

//列举待发送文件
int listSFiles(gsNode **list, gsNode *gs, int size)
{
  filenode *curFile;
  int tmp=0;

  while ((gs!=NULL)&&(tmp<size))
  {
    if ((gs->cancelled==0))
    {
      *list++ = gs;
      tmp++;
    }
    gs = gs->next;
  }
  
  return tmp;
  
}

//取消文件传输
int ceaseSend()
{
  gsNode *gsList[CAPACITY], *cur;
  int tmp, index, count;
  char buf[FILENAME];
  filenode *curFile;

  while(1)
  {
    pthread_mutex_lock(&sendFMutex);
    count = listSFiles(gsList, sendFHead.next, sizeof(gsList)/sizeof(gsList[0])-1);
    pthread_mutex_unlock(&sendFMutex);
    
    if (count == 0)
      return 0;
    else
    {
      printf("\n++++++++++++++++++++\n");
      printf("Files to send:\n");
      for(tmp=1;tmp<=count;tmp++)
      {
        printf("%d. ", tmp);
        curFile = (*(gsList+tmp-1))->fileList.next;
        while (curFile!=NULL)
        {
          printf("%s ", curFile->fileName);
          curFile = curFile->next;
        }
        printf("\n");
      }
      printf("++++++++++++++++++++\n");
    }

    index = inputNo(1, count, 1,
                    "Which file(s) to cancel?(Ctrl+D to quit)\nNumber[1]:");

    if (index<0)
      return -1;

    pthread_mutex_lock(&sendFMutex);
    (*(gsList+index-1))->cancelled = 1;
    if ((*(gsList+index-1))->tcpSock>=0)
      shutdown((*(gsList+index-1))->tcpSock, SHUT_RDWR);
    pthread_mutex_unlock(&sendFMutex);
  }
}


////////////////////////////////////////////////////
//文件接收
//选择要接收的文件
int recvFiles()
{
  gsNode *gsList[CAPACITY], *cur;
  int tmp, index, count;
  char buf[FILENAME];
  struct stat dirAttr;
  pthread_t gFile;
  filenode *curFile;
  pthread_attr_t attr;
  
  while(1)
  {
    pthread_mutex_lock(&getFMutex);
    count = listGFiles(gsList, getFHead.next, sizeof(gsList)/sizeof(gsList[0])-1);
    pthread_mutex_unlock(&getFMutex);
    
    if (count == 0)
      return -1;
    else
    {
      printf("\n++++++++++++++++++++\n");
      printf("Files to get:\n");
      for(tmp=1;tmp<=count;tmp++)
      {
        printf("%d. ", tmp);
        curFile = (*(gsList+tmp-1))->fileList.next;
        while (curFile!=NULL)
        {
          printf("%s ", curFile->fileName);
          curFile = curFile->next;
        }
        printf("\n");
      }
      printf("++++++++++++++++++++\n");
    }

    index = inputNo(1, count, 1,
                    "Which file(s) to get?(Ctrl+D to quit)\nNumber[1]:");

    if (index<0)
      return -1;
    
    while(1)
    {
      printf("Where do you want to save?(Ctrl+D to quit)\nTarget dir[.]:");
      if (fgets(buf, sizeof(buf), stdin)==NULL)
        return -1;

      transfStr(buf, 0); //去除前后空白字符
      
      if (buf[0]=='\0')
      {
        buf[0]='.';
        buf[1]='\0';
      }
      
      if ((stat(buf, &dirAttr)<0) ||
          !S_ISDIR(dirAttr.st_mode) ||
          (access(buf, W_OK)<0))
        printf("Invalid directory. Please input again.\n");
      else break;
    }
    
    cur = *(gsList+index-1);
    tmp = strlen(buf);
    pthread_mutex_lock(&getFMutex);
    if (cur->cancelled == 0)
    {
      cur->targetDir = (char*)malloc(tmp+1);
      strncpy(cur->targetDir, buf, tmp+1);
      cur->transferring = 1;
    }
    pthread_mutex_unlock(&getFMutex);
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&gFile, &attr, &getData, cur);
  }

}

//文件接收线程
void* getData(void* option)
{
  gsNode *gList = (gsNode *)option;
  filenode *head=&gList->fileList, *cur;
  int fileType, tmp;
  command com;

  printf("\nStart getting files.\n");
  while((cur=head->next) != NULL)
  {
    fileType = cur->fileType & 0x03;
    switch (fileType)
    {
    case 1:
      tmp=getFile(cur, gList);
      break;
    case 2:
      tmp=getDir(cur, gList);
      break;
    default:
      tmp = -1;
      printf("\nUnsupported file type.\n%s fails to be transfered.\n", cur->fileName);
      break;
    }
    
    if (tmp<0)
    {
      initCommand(&com, IPMSG_RELEASEFILES);
      snprintf(com.additional, sizeof(com.additional), "%d", gList->packetNo);
      memcpy(&com.peer, &gList->peer, sizeof(com.peer));
      sendMsg(&com);
      pthread_mutex_lock(&getFMutex);
      gList->cancelled = 1;
      gList->transferring = 0;
      pthread_mutex_unlock(&getFMutex);
      printf("\n%s fails to be transfered.\n", cur->fileName);
      return NULL;
    }
    
    printf("\n%s is transferred.\n", cur->fileName);
    head->next = cur->next;
    free(cur);
  }

  pthread_mutex_lock(&getFMutex);
  gList->cancelled = 1; //其实是已经完成
  gList->transferring = 0;
  pthread_mutex_unlock(&getFMutex);
  
  printf("File reception done.\n");
}

//接收文件
int getFile(void* option, gsNode *gList)
{
  int sockfd, readBytes;
  char buf[COMLEN];
  char recvs[RECFRG];
  filenode *head = (filenode*)option;
  command com;
  struct sockaddr_in peer;
  long fileSize, offset=0;
  FILE* recvFile;

  initCommand(&com, IPMSG_GETFILEDATA);  //Regular file
  snprintf(com.additional, MSGLEN, "%x:%x:%x", gList->packetNo, head->fileNo, offset);
  
  sockfd = socket(AF_INET, SOCK_STREAM, 0);

  bzero(&peer, sizeof(peer));
  peer.sin_family = AF_INET;
  peer.sin_port = htons(IPMSG_DEFAULT_PORT);
  memcpy(&peer.sin_addr, &gList->peer.sin_addr, sizeof(peer.sin_addr));
  
  if (connect(sockfd, (struct sockaddr*)&peer, sizeof(peer))<0)
  {
    printf("File connect error.\n");
    return -1;
  }
  
  msgCreater(buf, &com, sizeof(buf));

  if (writen(sockfd, buf, strlen(buf)+1)<0)
    return -1;

  sscanf(head->fileSize, "%x", &fileSize);

  if ((recvFile = fopen(head->fileName, "w+")) == NULL)
  {
    printf("getFile: file create error.\n");
    return -1;
  }

  while (fileSize>0)
  {
    readBytes = fileSize < RECFRG ? fileSize:RECFRG;
    if ((readBytes = readn(sockfd, recvs, readBytes))<0) //RECFRG
    {
      printf("getFile: data transfer error.\n");
      return -1;
    }
    
    fwrite(recvs, 1, readBytes, recvFile);
    fileSize -= readBytes;
  }
  fclose(recvFile);
  
  close(sockfd);
  return 0;
}

//接收文件夹时分析，头部
int parseHeader(filenode *pfn, char * recvs)
{
  char *strhead, *strtmp, gMsg[FILENAME];
  int i=0;
  
  strhead = recvs;
  while (i<3) //分析前3段
  {
    strtmp = strchr(strhead, ':');
    if (strtmp == NULL)
      return -1;
    *strtmp = '\0';

    switch (i)
    {
    case 0:
      
      strncpy(gMsg, strhead, sizeof(gMsg));
      delColon(gMsg, sizeof(gMsg));
      if (utf8)
        g2u(gMsg, sizeof(gMsg),
            pfn->fileName, sizeof(pfn->fileName));
      else strncpy(pfn->fileName, gMsg, sizeof(pfn->fileName));
      break;
    case 1:
      //sscanf(strhead, "%x", &pfn->fileSize);
      strncpy(pfn->fileSize, strhead, sizeof(pfn->fileSize));
      break;
    case 2:
      sscanf(strhead, "%x", &pfn->fileType);
      break;
      //case 3: //进行了简化
      // strncpy(pfh->h14, strhead+3, sizeof(pfh->h14));
      //break;
      //case 4:
      //strncpy(pfh->h16, strhead+3, sizeof(pfh->h16));
      //break;
    default:
      break;
    }

    strhead = strtmp+1;
    i++;
  }

  strncpy(pfn->otherAttrs, strhead, sizeof(pfn->otherAttrs));
  
  return 0;
}

//接收文件夹
int getDir(void *option, gsNode *gList)
{
  int sockfd, readBytes, headerSize, fileType, dirDepth=0, tmp;
  char buf[COMLEN], recvs[RECFRG], hSize[HSIZE], fullPath[2*FILENAME];
  filenode *head = (filenode*)option, *cur, fn;
  command com;
  struct sockaddr_in peer;
  long  offset=0, fileSize;
  FILE* recvFile;

  strncpy(fullPath, gList->targetDir, sizeof(fullPath));
  
  initCommand(&com, IPMSG_GETDIRFILES); //Direcotry
  
  snprintf(com.additional, MSGLEN, "%x:%x:%x", gList->packetNo, head->fileNo, offset);
  
  sockfd = socket(AF_INET, SOCK_STREAM, 0);

  bzero(&peer, sizeof(peer));
  peer.sin_family = AF_INET;
  peer.sin_port = htons(IPMSG_DEFAULT_PORT);
  memcpy(&peer.sin_addr, &gList->peer.sin_addr, sizeof(peer.sin_addr));
  
  if (connect(sockfd, (struct sockaddr*)&peer, sizeof(peer))<0)
  {
    printf("File connect error.\n");
    return -1;
  }

  msgCreater(buf, &com, sizeof(buf));
  
  if (writen(sockfd, buf, strlen(buf)+1)<0)
    return -1;
  
  do
  {
    tmp = strlen(fullPath);
    if (tmp+1>=sizeof(fullPath))
    {
      printf("getDir: target directory is too lang.\n");
      return -1;
    }
    if (fullPath[tmp-1]!='/')
    {
      fullPath[tmp] = '/';
      fullPath[tmp+1] = '\0';
    }
  
    readBytes = readDelimiter(sockfd, hSize, HSIZE, ':');
    sscanf(hSize, "%x", &headerSize);
    if (headerSize<=0 || headerSize<=readBytes || headerSize>RECFRG) //保护、防止溢出
      return -1;
    readn(sockfd, recvs, headerSize-readBytes);
    recvs[headerSize-readBytes]='\0';
    
    if (parseHeader(&fn, recvs)<0)
    {
      printf("getDir: Parse protocol failed.\n");
      return -1;
    }
    
    switch (fn.fileType & 0x03)
    {
    case IPMSG_FILE_REGULAR:

      strncat(fullPath, fn.fileName, sizeof(fullPath)-tmp-1);
      if ((recvFile = fopen(fullPath, "w+")) == NULL)
      {
        printf("Open error.\n");
        return -1;
      }
      
      sscanf(fn.fileSize, "%x", &fileSize);
      while (fileSize>0)
      {
        readBytes = fileSize < RECFRG ? fileSize:RECFRG;
        if ((readBytes = readn(sockfd, recvs, readBytes))<0) //RECFRG
        {
          printf("File read error.\n");
          return -1;
        }
        
        fwrite(recvs, 1, readBytes, recvFile);
        fileSize -= readBytes;
      }
      fclose(recvFile);
      getParentPath(fullPath, sizeof(fullPath));
      break;
      
    case IPMSG_FILE_DIR:
      strncat(fullPath, fn.fileName, sizeof(fullPath)-tmp-1);
      tmp = strlen(fullPath);
      if (tmp+1>=sizeof(fullPath))
      {
        printf("getDir: target directory is too lang.\n"
               "Filename(%s) has been truncated.\n", fn.fileName);
        fullPath[tmp-1] = '/';
      }
      else
      {
        fullPath[tmp] = '/';
        fullPath[tmp+1] = '\0';
      }
      
      if (mkdir(fullPath, S_IRUSR|S_IWUSR|S_IXUSR))
      {
        printf("getDir: mkdir failed.\n");
        return -1;
      }
      
      dirDepth++;
      break;
      
    case IPMSG_FILE_RETPARENT:
      getParentPath(fullPath, sizeof(fullPath));
      dirDepth--;
      break;
      
    default:
      printf("Unsupported file type.\n");
      break;
    }
    
  }while(dirDepth>0);

  //close(sockfd);
  shutdown(sockfd, SHUT_RDWR);
  return 0;
}

//列举待接收文件
int listGFiles(gsNode **list, gsNode *gs, int size)
{
  filenode *curFile;
  int tmp=0;

  while ((gs!=NULL)&&(tmp<size))
  {
    if ((gs->transferring==0)&&(gs->cancelled==0))
    {
      *list++ = gs;
      tmp++;
    }
    gs = gs->next;
  }
  
  return tmp;
  
}


/////////////////////////////////////////////////
//登录
int login()
{
  command com;

  initCommand(&com, IPMSG_BR_ENTRY);
  
  com.peer.sin_family = AF_INET;
  com.peer.sin_port = htons(IPMSG_DEFAULT_PORT);
  
  if (inet_pton(AF_INET, allHosts, &com.peer.sin_addr)<0)
    printf("login: Ip error.\n");
  
  strncpy(com.additional, pwd->pw_name, MSGLEN);
  
  sendMsg(&com);
  
}

//退出
int logout()
{
  command com;

  initCommand(&com, IPMSG_BR_EXIT);
  
  com.peer.sin_family = AF_INET;
  com.peer.sin_port = htons(IPMSG_DEFAULT_PORT);
  
  if (inet_pton(AF_INET, allHosts, &com.peer.sin_addr)<0)
    printf("logout: error\n");

  strncpy(com.additional, pwd->pw_name, MSGLEN);
  
  sendMsg(&com);
  printf("Bye!\n");
  
}
