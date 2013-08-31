/**********************************************************
 *Filename: utils.h
 *Author:   星云鹏
 *Date:     2008-05-15
 *
 *与协议无关的若干函数
 *********************************************************/

#ifndef UTIL_H
#define UTIL_H

#include <unistd.h>

extern ssize_t readn(int fd, void *buf, size_t count);
extern ssize_t readDelimiter(int fd, void *buf, ssize_t count, char ch);
extern ssize_t writen(int fd, const void *buf, size_t count);
extern ssize_t readline(int fd, void *buf, int size);
extern void delColon(char* dest, int size);
extern void addColon(char* dest, int size);
extern int inputNo(int min, int max, int def, char *prom);
extern int getFileName(char* dest, const char* fullpath, int size);
extern int getParentPath(char *dest, int size);
extern void transfStr(char *dest, int flag);

#endif
