/**********************************************************
 *Filename: utils.h
 *Author:   星云鹏
 *Date:     2008-05-15
 *
 *用户的处理
 *********************************************************/

#ifndef USERS_H
#define USERS_H

#include "coms.h"
typedef struct user
{
  struct sockaddr_in peer;
  char   name[NAMELEN];
  char   host[NAMELEN];
  char   nickname[NAMELEN];
  int    inUse;
  int    exit;
  struct user *next;
} user;

extern user userList;

extern int insertUser(user *uList, user *target);
extern void destroyUsers(user *uList);
extern int listUsers(user **pusers, user *uList, int size, int flag);
extern int unListUsers(user **pusers, int num);
extern int delUser(user *uList, command *peercom);

#endif
