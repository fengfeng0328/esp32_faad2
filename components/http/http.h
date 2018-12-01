#ifndef TL_HTTP_H
#define TL_HTTP_H

/*
 * uri:需要请求的http资源地址	(http://ai-thinker.oss-cn-shenzhen.aliyuncs.com/eCos%2Fm4atestfile.m4a)
 * fdtype:fopen打开文件的模式 	(0:接收的数据不写入SD卡,1:w+,2:rb+)
 * Rlen_sta:断点请求的起始地址	(与SD卡地址相对应)
 * Rlen_end:断点请求的结束地址 	(当Rlen_end为-1表示断点请求从Rlen_sta到文件结束)
 * mode:是否使用断点请求模式 	(1:使用,0:不使用)
 * */

int http_client_get(char *uri, int fdtype, int Rlen_sta, int Rlen_end, int mode);

#endif
