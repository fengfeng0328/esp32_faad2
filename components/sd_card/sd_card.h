#ifndef SD_CARD_H
#define SD_CARD_H

/* 初始化SD并挂载 */
void sd_init(void);

/* FileName	文件名字
 * FileSize 文件大小
 * 返回 0 	代表成功
 * 返回-1	代表失败
 * */
int CreateFile(char *FileName,int FileSize);

#endif
