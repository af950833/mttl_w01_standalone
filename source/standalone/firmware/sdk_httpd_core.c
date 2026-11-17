/* Local SDK copy: single-thread cleanup acknowledgement; no SDK source edits. */
#include "FreeRTOS.h"
#include "task.h"
#include "platform_stdlib.h"
#include "lwip/sockets.h"
#include "httpd.h"
#include "httpd_util.h"
#include "sdk_sse.h"
#include "sdk_worker.h"

extern void local_api_relay_handler(struct httpd_conn *conn);

// setting
#define HTTPD_DEFAULT_PORT             80
#define HTTPD_DEFAULT_SECURE_PORT      443
#define HTTPD_DEFAULT_MAX_CONN         1
#define HTTPD_DEFAULT_STACK_BYTES      4096
#define HTTPD_LISTEN_LEN               3
#define HTTPD_SELECT_TIMEOUT           1
#define HTTPD_RECV_TIMEOUT             2
#define HTTPD_PRIORITY                 (tskIDLE_PRIORITY + 1)

static uint16_t httpd_port = HTTPD_DEFAULT_PORT;
static uint32_t httpd_stack_bytes = HTTPD_DEFAULT_STACK_BYTES;
static uint8_t httpd_thread_mode = HTTPD_THREAD_SINGLE;
static uint8_t httpd_secure = HTTPD_SECURE_NONE;
uint8_t httpd_debug = HTTPD_DEBUG_ON;
uint8_t httpd_max_conn = HTTPD_DEFAULT_MAX_CONN;

// run-time
static uint8_t httpd_running = 0;
static volatile uint8_t mttl_httpd_active = 0;
int mttl_httpd_stopped(void) { return !mttl_httpd_active; }
static int httpd_sock = -1;
static int httpd_max_sock = -1;
struct httpd_page *httpd_page_database = NULL;
struct httpd_conn *httpd_connections = NULL;
static char *httpd_user_password = NULL;

static int httpd_conn_verify(struct httpd_conn *conn)
{
	int ret = 0;
	char *auth = NULL;
	char *auth_basic = NULL;
	char *auth_base64 = NULL;

	if(httpd_request_get_header_field(conn, "Authorization", &auth) != 0) {
		ret = -1;
		goto exit;
	}

	auth_basic = strstr(auth, "Basic ");
	if(auth_basic == NULL) { ret = -1; goto exit; }
	auth_base64 = auth_basic + strlen("Basic ");

	if((auth_basic == NULL) || (strcmp(auth_base64, httpd_user_password) != 0)) {
		ret = -1;
		goto exit;
	}

exit:
	if(auth)
		httpd_free(auth);

	return ret;
}

static void httpd_conn_handler(struct httpd_conn *conn)
{
    extern void mttl_http_begin(struct httpd_conn *);
    extern void mttl_http_end(void);
	struct httpd_page *page;
	int recv_timeout, recv_timeout_backup = 0;
	size_t recv_timeout_len;
    mttl_http_begin(conn);

	recv_timeout_len = sizeof(recv_timeout_backup);

	if(getsockopt(conn->sock, SOL_SOCKET, SO_RCVTIMEO, &recv_timeout_backup, &recv_timeout_len) != 0)
		httpd_log("ERROR: SO_RCVTIMEO");

	recv_timeout = HTTPD_RECV_TIMEOUT * 1000;

	if(setsockopt(conn->sock, SOL_SOCKET, SO_RCVTIMEO, &recv_timeout, sizeof(recv_timeout)) != 0)
		httpd_log("ERROR: SO_RCVTIMEO");

	if(httpd_request_read_header(conn) == -1) {
		httpd_response_bad_request(conn, NULL);
		goto exit;
	}

	if(httpd_user_password) {
		if(httpd_conn_verify(conn) == -1) {
			httpd_response_unauthorized(conn, NULL);
			goto exit;
		}
	}

	if(setsockopt(conn->sock, SOL_SOCKET, SO_RCVTIMEO, &recv_timeout_backup, sizeof(recv_timeout_backup)) != 0)
		httpd_log("ERROR: SO_RCVTIMEO");

	// call page callback
	page = httpd_page_database;

	while(page) {
		if((strlen(page->path) == conn->request.path_len) &&
		   (memcmp(page->path, conn->request.path, conn->request.path_len) == 0)) {

			(page->callback)(conn);
			break;
		}

		page = page->next;
	}

	if(page == NULL) {
		httpd_response_not_found(conn, NULL);
		goto exit;
	}

    mttl_http_end();
	return;

exit:
	httpd_conn_close(conn);
    mttl_http_end();
}

static void httpd_conn_thread(void *param)
{
	struct httpd_conn *conn = (struct httpd_conn *) param;

	httpd_log_verbose("%s started", __FUNCTION__);

	while(conn->sock >= 0) {
		fd_set read_fds;
		struct timeval timeout;

		timeout.tv_sec = HTTPD_SELECT_TIMEOUT;
		timeout.tv_usec = 0;
		FD_ZERO(&read_fds);
		FD_SET(conn->sock, &read_fds);

		if(select(conn->sock + 1, &read_fds, NULL, NULL, &timeout)) {
			if(FD_ISSET(conn->sock, &read_fds))
				httpd_conn_handler(conn);
		}
	}

	httpd_log_verbose("%s stopped", __FUNCTION__);
	vTaskDelete(NULL);
}

static void httpd_server_thread(void *param)
{
	httpd_log("%s started", __FUNCTION__);
	httpd_running = 1;

	while(httpd_running) {
		mttl_sdk_watchdog_http(1);
		fd_set read_fds;
		struct timeval timeout;

		mttl_sdk_sse_pump();
		timeout.tv_sec = mttl_sdk_sse_active()?0:HTTPD_SELECT_TIMEOUT;
		timeout.tv_usec = mttl_sdk_sse_active()?20000:0;
		FD_ZERO(&read_fds);
		FD_SET(httpd_sock, &read_fds);

		if(httpd_thread_mode == HTTPD_THREAD_SINGLE) {
			int i;

			for(i = 0; i < httpd_max_conn; i ++) {
				if(httpd_connections[i].sock >= 0) {
					FD_SET(httpd_connections[i].sock, &read_fds);
				}
			}
		}

		if(select(httpd_max_sock + 1, &read_fds, NULL, NULL, &timeout)) {
			if(FD_ISSET(httpd_sock, &read_fds)) {
				struct sockaddr_in client_addr;
				unsigned int client_addr_size = sizeof(client_addr);
				int client_sock;
				struct httpd_conn *conn = NULL;

				if((client_sock = accept(httpd_sock, (struct sockaddr *) &client_addr, &client_addr_size)) >= 0) {
					httpd_log_verbose("accept(%d)", client_sock);
					conn = httpd_conn_add(client_sock);

					if(conn == NULL) {
						struct httpd_conn tmp_conn;

						httpd_log("ERROR: httpd_conn_add");
						memset(&tmp_conn, 0, sizeof(struct httpd_conn));
						tmp_conn.sock = client_sock;

						if(httpd_secure) {
							tmp_conn.tls = httpd_tls_new_handshake(&tmp_conn.sock, httpd_secure);

							if(tmp_conn.tls == NULL) {
								httpd_log("ERROR: httpd_tls_new_handshake");
								close(client_sock);
								continue;
							}
						}

						httpd_response_too_many_requests(&tmp_conn, NULL);

						if(tmp_conn.tls) {
							httpd_tls_close(tmp_conn.tls);
							httpd_tls_free(tmp_conn.tls);
						}

						close(client_sock);
					}
					else {
						/* Small header/body writes must not wait for Nagle/delayed ACK. */
						int no_delay = 1;
						setsockopt(conn->sock, IPPROTO_TCP, TCP_NODELAY, &no_delay, sizeof(no_delay));
						// enable socket keepalive with keepalive timeout = idle(3) + interval(5) * count(3) = 18 seconds
						int keepalive = 1, keepalive_idle = 3, keepalive_interval = 5, keepalive_count = 3;

						if(setsockopt(conn->sock, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive)) != 0)
							httpd_log("ERROR: SO_KEEPALIVE");
						if(setsockopt(conn->sock, IPPROTO_TCP, TCP_KEEPIDLE, &keepalive_idle, sizeof(keepalive_idle)) != 0)
							httpd_log("ERROR: TCP_KEEPIDLE");
						if(setsockopt(conn->sock, IPPROTO_TCP, TCP_KEEPINTVL, &keepalive_interval, sizeof(keepalive_interval)) != 0)
							httpd_log("ERROR: TCP_KEEPINTVL");
						if(setsockopt(conn->sock, IPPROTO_TCP, TCP_KEEPCNT, &keepalive_count, sizeof(keepalive_count)) != 0)
							httpd_log("ERROR: TCP_KEEPCNT");

						// set conn ssl
						if(httpd_secure) {
							conn->tls = httpd_tls_new_handshake(&conn->sock, httpd_secure);

							if(conn->tls == NULL) {
								httpd_log("ERROR: httpd_tls_new_handshake");
								httpd_conn_remove(conn);
								continue;
							}
						}

						// update max sock for select
						if(client_sock > httpd_max_sock)
							httpd_max_sock = client_sock;

						// create thread for multi-thread mode
						if(httpd_thread_mode == HTTPD_THREAD_MULTIPLE) {
							if(xTaskCreate(httpd_conn_thread, ((const char*) "httpd_conn"), httpd_stack_bytes / 4, conn, HTTPD_PRIORITY, NULL) != pdPASS) {
								httpd_log("ERROR: xTaskCreate httpd_conn");
								httpd_conn_remove(conn);
							}
						}
					}
				}
				else {
					httpd_log("ERROR: accept");
					goto exit;
				}
			}
			else if(httpd_thread_mode == HTTPD_THREAD_SINGLE) {
				int i;

				for(i = 0; i < httpd_max_conn; i ++) {
					if((httpd_connections[i].sock >= 0) && (FD_ISSET(httpd_connections[i].sock, &read_fds))) {
						httpd_conn_handler(&httpd_connections[i]);
					}
				}
			}
		}
		else {
			httpd_log_verbose("select");
		}
	}

exit:
	// free httpd resource
	mttl_sdk_sse_close();
	/* Release the listening socket as well as accepted connections. Otherwise
	 * AP -> STA restart cannot bind port 80 and recovery loops forever. */
	if(httpd_sock != -1) {
		close(httpd_sock);
		httpd_sock = -1;
	}
	httpd_max_sock = -1;
	httpd_page_clear();
	httpd_conn_clear();
	httpd_free(httpd_connections);
	httpd_connections = NULL;

	if(httpd_secure)
		httpd_tls_setup_free();

	if(httpd_user_password) {
		httpd_free(httpd_user_password);
		httpd_user_password = NULL;
	}

	httpd_log("%s stopped", __FUNCTION__);
	httpd_running = 0;
	mttl_httpd_active = 0;
	mttl_sdk_watchdog_http(0);
	vTaskDelete(NULL);
}

static int httpd_init(void)
{
	int ret = 0;

	httpd_connections = (struct httpd_conn *) httpd_malloc(sizeof(struct httpd_conn) * httpd_max_conn);

	if(httpd_connections) {
		int i;

		memset(httpd_connections, 0, sizeof(struct httpd_conn) * httpd_max_conn);

		for(i = 0; i < httpd_max_conn; i ++) {
			httpd_connections[i].sock = -1;
		}
	}
	else {
		ret = -1;
		goto exit;
	}

	if((httpd_sock = socket(AF_INET, SOCK_STREAM, 0)) >= 0) {
		struct sockaddr_in httpd_addr;
		int reuse = 1;
		/* AP submission connections can remain in TIME_WAIT after closing.
		 * Permit the same service to bind again during AP/STA transitions. */
		if(setsockopt(httpd_sock, SOL_SOCKET, SO_REUSEADDR,
		              &reuse, sizeof(reuse)) != 0) {
			ret = -1;
			goto exit;
		}
		memset(&httpd_addr, 0, sizeof(httpd_addr));

		httpd_addr.sin_family = AF_INET;
		httpd_addr.sin_port = htons(httpd_port);
		httpd_addr.sin_addr.s_addr = INADDR_ANY;

		if((ret = bind(httpd_sock, (struct sockaddr *) &httpd_addr, sizeof(httpd_addr))) != 0) {
			httpd_log("ERROR: bind");
			goto exit;
		}

		if((ret = listen(httpd_sock, HTTPD_LISTEN_LEN)) != 0) {
			httpd_log("ERROR: listen");
			goto exit;
		}

		httpd_max_sock = httpd_sock;
	}
	else {
		ret = -1;
		goto exit;
	}

exit:
	if(ret) {
		if(httpd_sock != -1) {
			close(httpd_sock);
			httpd_sock = -1;
		}

		if(httpd_connections) {
			httpd_free(httpd_connections);
			httpd_connections = NULL;
		}
	}

	return ret;
}

/* Interface */
int httpd_start(uint16_t port, uint8_t max_conn, uint32_t stack_bytes, uint8_t thread_mode, uint8_t secure)
{
#if (HTTPD_USE_TLS != HTTPD_TLS_POLARSSL) && (HTTPD_USE_TLS != HTTPD_TLS_MBEDTLS)
    /* Reject unsupported HTTPS before mutating server state. */
    if(secure) return -1;
#endif
	if(port > 0)
		httpd_port = port;
	else if(secure)
		httpd_port = HTTPD_DEFAULT_SECURE_PORT;

	if(max_conn > 0)
		httpd_max_conn = max_conn;

	if(stack_bytes > 0)
		httpd_stack_bytes = stack_bytes;

	httpd_thread_mode = thread_mode;
	httpd_secure = secure;

	if(httpd_init()) {
		httpd_log("ERROR: httpd_init");
		return -1;
	}

	httpd_reg_page_callback("/api/relay", local_api_relay_handler);

	mttl_httpd_active = 1;
	if(xTaskCreate(httpd_server_thread, ((const char*) "httpd_server"), httpd_stack_bytes / 4, NULL, HTTPD_PRIORITY, NULL) != pdPASS) {
		mttl_httpd_active = 0;
		if(httpd_sock != -1) { close(httpd_sock); httpd_sock = -1; }
		httpd_max_sock = -1;
		httpd_conn_clear(); httpd_free(httpd_connections); httpd_connections = NULL;
		httpd_log("ERROR: xTaskCreate httpd_server");
		return -1;
	}

	return 0;
}

void httpd_stop(void)
{
	httpd_running = 0;
}

int httpd_reg_page_callback(char *path, void (*callback)(struct httpd_conn *conn))
{
	return httpd_page_add(path, callback);
}

void httpd_clear_page_callbacks(void)
{
	httpd_page_clear();
}

void httpd_setup_debug(uint8_t debug)
{
	httpd_debug = debug;
}

int httpd_setup_cert(char *server_cert, char *server_key, char *ca_certs)
{
	return httpd_tls_setup_init(server_cert, server_key, ca_certs);
}

int httpd_setup_user_password(char *user, char *password)
{
	int ret = 0;
	size_t auth_len = strlen(user) + 1 + strlen(password);
	size_t base64_len = (auth_len + 2) / 3 * 4 + 1;
	uint8_t *auth = NULL;

	auth = (uint8_t *) httpd_malloc(auth_len);

	if(auth) {
		memcpy(auth, user, strlen(user));
		memcpy(auth + strlen(user), ":", 1);
		memcpy(auth + strlen(user) + 1, password, strlen(password));
		httpd_user_password = (char *) httpd_malloc(base64_len);

		if(httpd_user_password) {
			memset(httpd_user_password, 0, base64_len);
			ret = httpd_base64_encode(auth, auth_len, httpd_user_password, base64_len);

			if(ret) {
				httpd_log("ERROR: httpd_base64_encode");
				ret = -1;
				goto exit;
			}

			httpd_log_verbose("setup httpd_user_password[%s]", httpd_user_password);
		}
		else {
			httpd_log("ERROR: httpd_malloc");
			ret = -1;
			goto exit;
		}
	}
	else {
		httpd_log("ERROR: httpd_malloc");
		ret = -1;
		goto exit;
	}

exit:
	if(auth)
		httpd_free(auth);

	if(ret) {
		httpd_free(httpd_user_password);
		httpd_user_password = NULL;
	}

	return ret;
}
