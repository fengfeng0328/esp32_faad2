#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "errno.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include "spiram_fifo.h"
#include "spiram.h"

#include "url_parser.h"
#include "http.h"

#include "mp4event.h"

static const char *TAG="http";

#define RECV_MAX_LEN_T 4096

struct resp_header{
	int status_code;//HTTP/1.1 '200' OK
    char content_type[128];//Content-Type: application/gzip
	long content_length;//Content-Length: 11683079
};

static void get_resp_header(const char *response,struct resp_header *resp){
	/*获取响应头的信息*/
	char *pos = strstr(response, "HTTP/");
	if (pos)
		sscanf(pos, "%*s %d", &resp->status_code);//返回状态码
		pos = strstr(response, "Content-Type:");//返回内容类型
		if (pos)
			sscanf(pos, "%*s %s", resp->content_type);
		pos = strstr(response, "Content-Length:");//内容的长度(字节)
		if (pos)
			sscanf(pos, "%*s %ld", &resp->content_length);
}

/**
 * @brief simple http_get
 * see https://github.com/nodejs/http-parser for callback usage
 */
int http_client_get(char *uri, int fdtype, int Rlen_sta, int Rlen_end, int mode, int RecvDelay)// mode 1:use Range  mode 0:no use Range
{
	SET_HTTPFLAGS(HTTPOPEN);	// HTTP OPEN

	if (GET_MP4REQUEST() == MP4REQUEST_FIRST) {
		pHeadCnt = Rlen_sta - 1;	// FILE ADDR
	}

	if (GET_MP4REQUEST() == MP4REQUEST_THIRD) {
		pMdatCnt = Rlen_sta - 1;
	}

	url_t *url = url_parse(uri);

	const struct addrinfo hints = { .ai_family = AF_INET, .ai_socktype =
			SOCK_STREAM, };
	struct addrinfo *res;
	struct in_addr *addr;
	char port_str[6]; // stack allocated
	snprintf(port_str, 6, "%d", url->port);

	int err = getaddrinfo(url->host, port_str, &hints, &res);
	if (err != ESP_OK || res == NULL) {
		ESP_LOGE(TAG, "DNS lookup failed err=%d res=%p", err, res);
		return err;
	}

	// print resolved IP
	addr = &((struct sockaddr_in *) res->ai_addr)->sin_addr;
	ESP_LOGI(TAG, "DNS lookup succeeded. IP=%s", inet_ntoa(*addr));

	// allocate socket
	int sock = socket(res->ai_family, res->ai_socktype, 0);
	if (sock < 0) {
		ESP_LOGE(TAG, "... Failed to allocate socket.");
		freeaddrinfo(res);
	}
	ESP_LOGI(TAG, "... allocated socket");

	// connect, retrying a few times
	char retries = 0;
	while (connect(sock, res->ai_addr, res->ai_addrlen) != 0) {
		retries++;
		ESP_LOGE(TAG, "... socket connect attempt %d failed, errno=%d", retries,
				errno);

		if (retries > 5) {
			ESP_LOGE(TAG, "giving up");
			close(sock);
			freeaddrinfo(res);
			return ESP_FAIL;
		}
	}

	ESP_LOGI(TAG, "... connected");
	freeaddrinfo(res);

	char *request;
	if (mode == 0) {	// 非断点请求
		// write http request
		// char *request;
		if (asprintf(&request, "GET %s HTTP/1.0\r\n"
				"Host: %s:%d\r\n"
				"User-Agent: ESP32\r\n"
				"Accept: */*\r\n"
				"\r\n", url->path, url->host, url->port) < 0) {
			return ESP_FAIL;
		}
	} else if ((mode != 0) && (Rlen_end != -1)) {
		// write http request
		// char *request;
		if (asprintf(&request, "GET %s HTTP/1.0\r\n"
				"Host: %s:%d\r\n"
				"User-Agent: ESP32\r\n"
				"Accept: */*\r\n"
				"Range: bytes=%d-%d\r\n"
				"\r\n", url->path, url->host, url->port, Rlen_sta, Rlen_end)
				< 0) {
			return ESP_FAIL;
		}
	} else if ((mode != 0) && (Rlen_end == -1)) {
		// write http request
		// char *request;
		if (asprintf(&request, "GET %s HTTP/1.0\r\n"
				"Host: %s:%d\r\n"
				"User-Agent: ESP32\r\n"
				"Accept: */*\r\n"
				"Range: bytes=%d-\r\n"
				"\r\n", url->path, url->host, url->port, Rlen_sta)
				< 0) {
			return ESP_FAIL;
		}
	}

	ESP_LOGI(TAG, "requesting %s", request);

	if (write(sock, request, strlen(request)) < 0) {
		ESP_LOGE(TAG, "... socket send failed");
		close(sock);
	}

	free(request);
	ESP_LOGI(TAG, "... socket send success");

	/****** 读取响应头 ******/
	char response[4096];
	memset(response, 0, sizeof(response));
	int length = 0, mem_size = 4096;
	struct resp_header resp;
	int ret = 0;
	while (1) {
		ret = recv(sock, response + length, 1, 0);
		if (ret <= 0)
			break;
		//找到响应头的头部信息, 两个"\r\n"为分割点
		int flag = 0;
		int i;
		for (i = strlen(response) - 1;
				response[i] == '\n' || response[i] == '\r'; i--, flag++)
			;
		if (flag == 4)
			break;
		length += ret;
		if (length >= mem_size - 1) {
			break;
		}
	}
	get_resp_header(response, &resp);
	printf("origin content_length = %ld status_code = %d\n",
			resp.content_length, resp.status_code);	// 长度和状态保留开发者使用
	if (((resp.status_code != 200) && (resp.status_code != 206))
			|| resp.content_length == 0) {
		return;
	}

	FILE *FileCache = NULL;
	if (fdtype == 1) {		// w+
		FILE *hMP4FileCache;
		hMP4FileCache = fopen("/sdcard/Cache.m4a", "rb+");	// 第一次打开文件
		if (!hMP4FileCache) {
			printf("Error opening file\n");
			return -1;
		}
		FileCache = hMP4FileCache;
		fseek(FileCache, Rlen_sta, SEEK_SET);
	} else if (fdtype == 2) {
		FILE *hMP4FileCache;
		hMP4FileCache = fopen("/sdcard/Cache.m4a", "rb+");	// 非第一次打开
		if (!hMP4FileCache) {
			printf("Error opening file\n");
			return -1;
		}
		FileCache = hMP4FileCache;
		fseek(FileCache, Rlen_sta, SEEK_SET);
	}




	/* 读取主体部分 */
//	char recv_buf[2048];	// 注意TCP最大长度和字节对齐效率，该长度未发现问题
	char *recv_buf = (char*) malloc(RECV_MAX_LEN_T);
	bzero(recv_buf, RECV_MAX_LEN_T);
	ssize_t recved;

	length = 0;
	do {
		recved = read(sock, recv_buf, RECV_MAX_LEN_T - 1);
		printf("recved:%d\n", recved);
//		spiRamFifoWrite(recv_buf, recved);
		length = length + recved;

		if (fdtype != 0) {
			if (fwrite(recv_buf, 1, recved, FileCache) != recved) {
				printf("fwrite Error\n");
				return -1;
			}

			if (GET_MP4REQUEST() == MP4REQUEST_FIRST) {
				pHeadCnt = pHeadCnt + recved;
			}
			if (GET_MP4REQUEST() == MP4REQUEST_THIRD) {
				pMdatCnt = pMdatCnt + recved;
			}
			if (RecvDelay > 0) {
				vTaskDelay(RecvDelay / portTICK_PERIOD_MS);
			}
		}

		if (length == resp.content_length)
			break;
//		if (GET_EVENT_BTN() == DOWN) {
//			break;
//		}
		if (GET_HTTPFLAGS() == HTTPCLOSE) {		// HTTP关闭请求
			break;
		}
	} while (recved > 0);	// 一定要主动断开，注意实时性
	printf("length = %d\n", length);

	free(recv_buf);
	free(url);

	ESP_LOGI(TAG, "... done reading from socket. Last read return=%d errno=%d",
			recved, errno);
	close(sock);
	ESP_LOGI(TAG, "socket closed");

	if ((fdtype != 0) && (FileCache != NULL))
		fclose(FileCache);

	SET_HTTPFLAGS(HTTPEXIT);	// HTTP 状态设置为退出，进行新HTTP请求前需要判断是否处于退出状态
	return 0;
}

void http_client_get_task(void *pvParameters) {
	http_client_get(((Prvdata_T*) pvParameters)->uri,
			((Prvdata_T*) pvParameters)->fdtype,
			((Prvdata_T*) pvParameters)->Rlen_sta,
			((Prvdata_T*) pvParameters)->Rlen_end,
			((Prvdata_T*) pvParameters)->mode,
			((Prvdata_T*) pvParameters)->RecvDelay);

	if (pvParameters != NULL)
		free(pvParameters);		// 任务完成，释放内存

	vTaskDelete(NULL);
}

int CreateHttpGet_Task(char *uri, int fdtype, int Rlen_sta, int Rlen_end,
		int mode, int RecvDelay, int usStackDepth, int uxPriority) {
	Prvdata_T *requestInfo = NULL;
	requestInfo = (Prvdata_T *) malloc(sizeof(Prvdata_T));
	if (NULL == requestInfo) {
		printf("malloc Error\n");
		return -1;
	}
	requestInfo->uri = uri;
	requestInfo->fdtype = fdtype;
	requestInfo->Rlen_sta = Rlen_sta;
	requestInfo->Rlen_end = Rlen_end;
	requestInfo->mode = mode;
	requestInfo->RecvDelay = RecvDelay;

	xTaskCreate(&http_client_get_task, "http_client_get_task", usStackDepth,
			(void*) requestInfo, uxPriority, NULL);

	return 0;
}
