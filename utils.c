/**********************************************************
 *Filename: utils.c
 *Author:   星云鹏
 *Date:     2008-05-15
 *
 *与协议无关的若干函数
 *********************************************************/


#include "coms.h"
#include "utils.h"
#include <errno.h>

//////////////////////////////////////////////
//读写
ssize_t readn(int fd, void *buf, size_t count)
{
  char *strtmp;
  ssize_t reval, realcount=0;
  strtmp = (char *)buf;
  
  while (count>0)
  {
    reval = read(fd, strtmp, count);
    if (reval<0)
      if (errno == EINTR)
        continue;
      else return -1;
    else if (reval>0)
    {
      count -= reval;
      strtmp += reval;
      realcount += reval;
      continue;
    }
    else break;
  }
  
  return realcount;
}

ssize_t readDelimiter(int fd, void *buf, ssize_t count, char ch)
{
  char *strtmp, *buftmp=buf;
  ssize_t reval, realcount=0;
  strtmp = (char *)buf;
  
  while (realcount<count)
  {
    reval = read(fd, strtmp, 1);
    if (reval<0)
      if (errno == EINTR)
        continue;
      else return -1;
    else if (reval>0)
    {
      realcount++;
      if (*strtmp++ == ch)
        break;
      
      continue;
    }
    else break;
  }
  
  if (strtmp>buftmp)
    *(strtmp--) = '\0';
  else *buftmp = '\0';
  
  return realcount; 
}

ssize_t writen(int fd, const void *buf, size_t count)
{
  char *strtmp;
  ssize_t reval, realcount=count;
  strtmp = (char *)buf;

  while(count>0)
  {
    reval = write(fd, strtmp, count);
    if (reval < 0)
      if (errno == EINTR)
        continue;
      else return -1;
      
    count -= reval;
    strtmp += reval;
  }

  return realcount;
}
ssize_t readline(int fd, void *buf, int size)
{
  char *strtmp;
  ssize_t reval, realcount=0;
  strtmp = (char *)buf;
 
  while(size>1)
  {
    reval = read(fd, strtmp, 1);
    if (reval<0)
      if (errno == EINTR)
        continue;
      else return -1;
    else if (reval == 0)
      break;
    else
    {
      realcount++;
      size--;
      if (*strtmp++ =='\n')
        break;
    }
  }
  *strtmp='\0';
  return realcount;
}

//////////////////////////////////////////////
//处理:
void delColon(char* dest, int size) //:: ---> :
{
  char buf[FILENAME];
  char *pd=dest, *pb=buf;
  int flag=0;

  while (*pb++ = *pd++) // = not ==
    if ((*pd == ':') && (*(pd-1) == ':'))
    {
      pd++;
      flag = 1;
    }

  if (flag)
    strncpy(dest, buf, size);
  
}

void addColon(char* dest, int size) //: ---> ::
{
  char buf[FILENAME], *pb, *pd;
  int flag=0;
  unsigned int tmp=0;
  pb = buf;
  pd = dest;
  while ((*pd != '\0')&&(tmp<sizeof(buf)))
  {
    if ((*pb++ = *pd) == ':') //=是故意的
    {
      *pb++ = ':';
      tmp++;
      flag = 1;
    }
    pd++;
    tmp++;
  }

  buf[tmp]='\0';

  if (flag)
    strncpy(dest, buf, size);
  
}

///////////////////////////////////////////////
//用户选择
int inputNo(int min, int max, int def, char *prom)
{
  char which[30];
  int tmp, index;

  if (min>max)
    return -1;
  
  if ((def<min)||(def>max))
    def = min;

  while (1)
  {
    printf(prom);
    if (fgets(which, sizeof(which), stdin)==NULL)
      return -1;

    transfStr(which, 0);
    
    if (which[0]=='\0')
    {
      index = def;
      break;
    }

    tmp = strlen(which);
    
    if (which[tmp-1]=='\n')
      which[--tmp]='\0';
    
    index = atoi(which);
    
    if (index<min || index>max)
      printf("Invalid input. Please input again.\n");
    else break;
  }
  return index;
}

/////////////////////////////////////////////
//
int getFileName(char* dest, const char* fullpath, int size)//get filename from fullpath
{
  char strtmp[128], *head;
  const char *tail;
  int tmp;
  tmp = strlen(fullpath);
  tail = fullpath + tmp - 1;
  while (*tail == '/') //去掉结尾所有的/
    tail--;
  head = strtmp;
  while (tail>=fullpath && *tail!='/') //得到filename的倒序
    *head++ = *tail--;
  
  head--;
  while (size>0 && head>=strtmp)
  {
    *dest++ = *head--;
    size--;
  }
  if (size>0)
  {
    *dest = '\0';
    return 0; //
  }
  else return -1; //文件名太长
}


int getParentPath(char *dest, int size)
{
  char *last, *ptr;

  if (size<2)
    return -1;
  
  if ((last = strrchr(dest, '/'))==NULL)
    strncpy(dest, "./", size);
  else if (*(last+1)!='\0')
    *(last+1) = '\0';
  else
  {
    ptr = last - 1;
    
    while (ptr>=dest)
    {
      if (*ptr=='/' && *(ptr+1)!='/')
      {
        *(ptr+1) = '\0';
        break;
      }
      ptr--;
    }

    if (ptr<dest)
      if (*dest!='/')
        strncpy(dest, "./", size);
      else dest[1] = '\0';
  }
  
  return 0;
}

//////////////////////////////////////////////////
//字符串处理，去掉开头结尾空白字符，变成全小写(flag==1)

void transfStr(char *dest, int flag)
{
  char *ptr;
  int len;
  
  ptr = dest;
  
  while (isspace(*ptr))
    ptr++;

  len = strlen(ptr);
  if (ptr > dest)
    memmove(dest, ptr, len+1);
  
  ptr = dest+len-1;

  while (isspace(*ptr))
    ptr--;

  *(ptr+1) = '\0';

  ptr = dest;

  if (flag == 1)
    while (*ptr!='\0')
    {
      *ptr = tolower(*ptr);
      ptr++;
    }
}

