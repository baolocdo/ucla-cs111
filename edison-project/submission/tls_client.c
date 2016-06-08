/*    
 *    Demonstration TLS client
 *
 *       Compile with
 *
 *       gcc -Wall -o tls_client tls_client.c -L/usr/lib -lssl -lcrypto
 *
 *       Execute with
 *
 *       ./tls_client <server_INET_address> <port> <client message string>
 *
 *       Generate certificate with 
 *
 *       openssl req -x509 -nodes -days 365 -newkey rsa:1024 -keyout tls_demonstration_cert.pem -out tls_demonstration_cert.pem
 *
 *     Developed for Intel Edison IoT Curriculum by UCLA WHI
 */
#include "tls_header.h"
#include "pthread.h"

#define LOG_FILE_NAME "tls.log"

int rate;
int log_fd;

void *thread_read(void *param)
{
    char buf[BUFSIZE];
    char log_buf[BUFSIZE];
    SSL *ssl = (SSL *)param;
    
    int receive_length, buf_size;

    while(1) {
        memset(buf,0,sizeof(buf));
        receive_length = SSL_read(ssl, buf, sizeof(buf));
        if(strstr(buf, "new_rate: ") != NULL) {
            sscanf(buf, "Heart rate of patient %*s is %*f new_rate: %d", &rate);
            rate = rate;
            printf("New rate %d received from server.\n", rate);

            memset(log_buf, 0, sizeof(log_buf));
            buf_size = sprintf(log_buf, "New rate %d received from server.\n", rate);
            write(log_fd, log_buf, buf_size);
        }

        printf("Received message '%s' from server.\n\n", buf);

        memset(log_buf, 0, sizeof(log_buf));
        buf_size = sprintf(log_buf, "Received message '%s' from server.\n\n", buf);
        write(log_fd, log_buf, buf_size);
    }
}

int main(int args, char *argv[])
{
    int port, range;
    int server;
    int receive_length, line_length;
    int buf_size;
    char ip_addr[BUFSIZE];
    char *my_ip_addr;
    char *line = NULL;
    
    char buf[BUFSIZE];
    char log_buf[BUFSIZE];

    double heart_rate;
    FILE *fp = NULL;
    SSL *ssl;
    SSL_CTX *ctx;

    // We overwrite previous log on each run
    log_fd = creat(LOG_FILE_NAME, 0666);
    if (log_fd < 0) {
        fprintf(stderr, "Error opening log file. Exiting.\n");  
        exit(2);
    }
    
    my_ip_addr = get_ip_addr();
    printf("My ip addr is: %s\n", my_ip_addr);

    /* READ INPUT FILE */
    fp = fopen("config_file", "r");
    if(fp == NULL){
        fprintf(stderr, "Error opening config file with name 'config_file'. Exiting.\n");
        exit(1);
    }
    printf("Reading input file...\n");
    while(getline(&line, &line_length, fp) > 0){
        if(strstr(line, "host_ip") != NULL){
            sscanf(line, "host_ip: %s\n", ip_addr);
        }
        else if(strstr(line, "port") != NULL){
            sscanf(line, "port: %d\n", &port);
        }
        else if(strstr(line, "range") != NULL){
            sscanf(line, "range: %d\n", &range);
        }
        else if(strstr(line, "rate") != NULL){
            sscanf(line, "rate: %d\n", &rate);
        }
        else{
            fprintf(stderr, "Unrecognized line found: %s. Ignoring.\n", line);
        }
    }
    fclose(fp);
    /* FINISH READING INPUT FILE */

    printf("Connecting to: %s:%d\n", ip_addr, port);
    buf_size = sprintf(log_buf, "Connecting to: %s:%d\n", ip_addr, port);
    write(log_fd, log_buf, buf_size);
    memset(log_buf, 0, sizeof(log_buf));

    /* SET UP TLS COMMUNICATION */
    SSL_library_init();
    ctx = initialize_client_CTX();
    server = open_port(ip_addr, port);
    ssl = SSL_new(ctx);
    SSL_set_fd(ssl, server);
    /* FINISH SETUP OF TLS COMMUNICATION */

    /* SEND HEART RATE TO SERVER */
    if (SSL_connect(ssl) == -1){ //make sure connection is valid
        fprintf(stderr, "Error. TLS connection failure. Aborting.\n");
        ERR_print_errors_fp(stderr);
        buf_size = sprintf(log_buf, "Error. TLS connection failure. Aborting.\n");
        write(log_fd, log_buf, buf_size);
        close(log_fd);
        exit(1);
    } else {
        memset(log_buf, 0, sizeof(log_buf));
        buf_size = sprintf(log_buf, "Client-Server connection complete with %s encryption\n", SSL_get_cipher(ssl));
        printf("%s", log_buf);
        write(log_fd, log_buf, buf_size);
        display_server_certificate(ssl);
    }
    
    pthread_t child_input_thread;
    pthread_create(&child_input_thread, NULL, thread_read, (void *)ssl);

    while(1) {
        printf("Current settings: rate: %d, range: %d\n", rate, range);
        memset(log_buf, 0, sizeof(log_buf));
        buf_size = sprintf(log_buf, "Current settings: rate: %d, range: %d\n", rate, range);
        write(log_fd, log_buf, buf_size);

        heart_rate = generate_random_number(AVERAGE_HEART_RATE-(double)range, AVERAGE_HEART_RATE+(double)range);

        //populate the buffer with information about the ip address of the client and the heart rate
        sprintf(buf, "Heart rate of patient %s is %4.2f", my_ip_addr, heart_rate);
        printf("Sending message '%s' to server...\n", buf);

        memset(log_buf, 0, sizeof(log_buf));
        buf_size = sprintf(log_buf, "Sending message '%s' to server...\n", buf);
        write(log_fd, log_buf, buf_size);

        SSL_write(ssl, buf, strlen(buf));
        memset(buf,0,sizeof(buf)); //clear out the buffer

        sleep(rate);
    }
    /* FINISH HEART RATE TO SERVER */

    //clean up operations
    SSL_free(ssl);
    close(server);
    close(log_fd);
    SSL_CTX_free(ctx);
    return 0;
}
