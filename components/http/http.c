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

static const char *TAG="http";

#define RECV_MAX_LEN_T 2048

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
int http_client_get(char *uri , void *datalen)
{
    url_t *url = url_parse(uri);

    const struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *res;
    struct in_addr *addr;
    char port_str[6]; // stack allocated
    snprintf(port_str, 6, "%d", url->port);

    int err = getaddrinfo(url->host, port_str, &hints, &res);
    if(err != ESP_OK || res == NULL) {
        ESP_LOGE(TAG, "DNS lookup failed err=%d res=%p", err, res);
        return err;
    }

    // print resolved IP
    addr = &((struct sockaddr_in *)res->ai_addr)->sin_addr;
    ESP_LOGI(TAG, "DNS lookup succeeded. IP=%s", inet_ntoa(*addr));

    // allocate socket
    int sock = socket(res->ai_family, res->ai_socktype, 0);
    if(sock < 0) {
        ESP_LOGE(TAG, "... Failed to allocate socket.");
        freeaddrinfo(res);
    }
    ESP_LOGI(TAG, "... allocated socket");


    // connect, retrying a few times
    char retries = 0;
    while(connect(sock, res->ai_addr, res->ai_addrlen) != 0) {
        retries++;
        ESP_LOGE(TAG, "... socket connect attempt %d failed, errno=%d", retries, errno);

        if(retries > 5) {
            ESP_LOGE(TAG, "giving up");
            close(sock);
            freeaddrinfo(res);
            return ESP_FAIL;
        }
    }

    ESP_LOGI(TAG, "... connected");
    freeaddrinfo(res);

    // write http request
    char *request;
    if(asprintf(&request, "GET %s HTTP/1.0\r\nHost: %s:%d\r\nUser-Agent: ESP32\r\nAccept: */*\r\n\r\n", url->path, url->host, url->port) < 0)
    {
        return ESP_FAIL;
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
    int length = 0,mem_size=4096;
    struct resp_header resp;
    int ret=0;
    while (1)	{
		ret = recv(sock, response+length, 1,0);
		if(ret<=0)
			break;
		//找到响应头的头部信息, 两个"\r\n"为分割点
		int flag = 0;
		int i;
		for (i = strlen(response) - 1; response[i] == '\n' || response[i] == '\r'; i--, flag++);
		if (flag == 4)
			break;
		length += ret;
		if(length>=mem_size-1){
			break;
		}
	}
	get_resp_header(response,&resp);
    printf("origin content_length = %ld status_code = %d\n",resp.content_length,resp.status_code);	// 长度和状态保留开发者使用
	if(resp.status_code!=200||resp.content_length==0){
		return;
	}

    /* 读取主体部分 */
//	char recv_buf[2048];	// 注意TCP最大长度和字节对齐效率，该长度未发现问题
	char *recv_buf = (char*) malloc(RECV_MAX_LEN_T);
    bzero(recv_buf, RECV_MAX_LEN_T);
    ssize_t recved;

	length = 0;
	do {
		recved = read(sock, recv_buf, RECV_MAX_LEN_T - 1);
		printf("recved:%d\n",recved);
		spiRamFifoWrite(recv_buf, recved);
		length = length + recved;
		if (length == resp.content_length)
			break;
//		if (GET_EVENT_BTN() == DOWN) {
//			break;
//		}
	} while (recved > 0);	// 一定要主动断开，注意实时性
	printf("length = %d\n",length);

	free(recv_buf);
    free(url);

    ESP_LOGI(TAG, "... done reading from socket. Last read return=%d errno=%d", recved, errno);
    close(sock);
    ESP_LOGI(TAG, "socket closed");

    return 0;
}