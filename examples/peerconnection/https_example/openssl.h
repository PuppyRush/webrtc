//#define WIN32_LEAN_AND_MEAN
//#define _CRT_SECURE_NO_WARNINGS
//#define _WINSOCK_DEPRECATED_NO_WARNINGS
//
//
//#include <openssl/bio.h> /* BasicInput/Output streams */
//#include <openssl/err.h> /* errors */
//#include <openssl/ssl.h> /* core library */
//#include <stdio.h>
//#include <stdlib.h>
//
//#include "openssl2.h"
//#define BuffSize 1024
//
//
//void report_and_exit(const char* msg) {
//  perror(msg);
//  ERR_print_errors_fp(stderr);
//  exit(-1);
//}
//
//void init_ssl() {
//  SSL_load_error_strings();
//  SSL_library_init();
//}
//
//void cleanup(SSL_CTX* ctx, BIO* bio) {
//  SSL_CTX_free(ctx);
//  BIO_free_all(bio);
//}
//
//void secure_connect(const char* hostname) {
//  char name[BuffSize];
//  char request[BuffSize];
//  char response[BuffSize];
//  
//  auto ctx = TCPServerSSL2(_SSLCERT_SHA2);
//
//  // const SSL_METHOD* method = TLS_method();
//  //if (NULL == method)
//  //  report_and_exit("TLSv1_2_client_method...");
//
//  //SSL_CTX* ctx = SSL_CTX_new(method);
//
//  if (NULL == ctx)
//    report_and_exit("SSL_CTX_new...");
//
//  BIO* bio = BIO_new_ssl_connect(ctx);
//  if (NULL == bio)
//    report_and_exit("BIO_new_ssl_connect...");
//
//  SSL* ssl = NULL;
//
//  /* link bio channel, SSL session, and server endpoint */
//
//  sprintf(name, "%s:%s", hostname, "9559");
//  BIO_get_ssl(bio, &ssl);                 /* session */
//  SSL_set_mode(ssl, SSL_MODE_AUTO_RETRY); /* robustness */
//  BIO_set_conn_hostname(bio, name);       /* prepare to connect */
//
//  /* try to connect */
//  if (BIO_do_connect(bio) <= 0) {
//    cleanup(ctx, bio);
//    report_and_exit("BIO_do_connect...");
//  }
//
//  /* verify truststore, check cert */
//  if (!SSL_CTX_load_verify_locations(
//          ctx, "C:\\webrtc\\src\\examples\\peerconnection\\js\\fake-keys\\certificate.pem", /* truststore */
//          "/etc/ssl/certs/"))                        /* more truststore */
//    report_and_exit("SSL_CTX_load_verify_locations...");
//
//  long verify_flag = SSL_get_verify_result(ssl);
//  if (verify_flag != X509_V_OK)
//    fprintf(stderr,
//            "##### Certificate verification error (%i) but continuing...\n",
//            (int)verify_flag);
//
//  /* now fetch the homepage as sample data */
//  sprintf(request,
//          "GET / HTTP/1.1\x0D\x0AHost: %s\x0D\x0A\x43onnection: "
//          "Close\x0D\x0A\x0D\x0A",
//          hostname);
//  BIO_puts(bio, request);
//
//  /* read HTTP response from server and print to stdout */
//  while (1) {
//    memset(response, '\0', sizeof(response));
//    int n = BIO_read(bio, response, BuffSize);
//    if (n <= 0)
//      break; /* 0 is end-of-stream, < 0 is an error */
//    puts(response);
//  }
//
//  cleanup(ctx, bio);
//}
//
//int openssl_main() {
//  init_ssl();
//
//  const char* hostname = "10.10.45.51";
//  fprintf(stderr, "Trying an HTTPS connection to %s...\n", hostname);
//  secure_connect(hostname);
//
//  return 0;
//}