#ifndef _SETTINGS_H
#define _SETTINGS_H

#define CONNECT_TIMEOUT 15 // seconds
#define MAXCONNECTS 50 // for curl's CURLOPT_MAXCONNECTS
#define LOW_SPEED_LIMIT 1024 // one kilobyte
#define LOW_SPEED_TIME 15 // ten seconds the speed is low - timeout

#define RETRY_SLEEP_TIME 5 // second, sleep time increment between failed retries to upload
#define RETRY_FAIL_AFTER 3 // how many tries to perform on failed upload

#define UPLOAD_THREADS 4

#define THREAD_STACK_SIZE 1024*512

// I need to use curl error reporting for internal failures of upload functions
#define UPLOAD_FILE_FUNCTION_FAILED CURLE_OBSOLETE44


#define HTTP_SHOULD_RETRY_ON(res) (res==CURLE_COULDNT_RESOLVE_PROXY || res==CURLE_COULDNT_RESOLVE_HOST || res==CURLE_COULDNT_CONNECT || \
	res==CURLE_HTTP_RETURNED_ERROR || res==CURLE_WRITE_ERROR || res==CURLE_UPLOAD_FAILED || res==CURLE_OPERATION_TIMEDOUT || \
	res==CURLE_SSL_CONNECT_ERROR || res==CURLE_PEER_FAILED_VERIFICATION || res==CURLE_SEND_ERROR || res==CURLE_CHUNK_FAILED)

#endif
