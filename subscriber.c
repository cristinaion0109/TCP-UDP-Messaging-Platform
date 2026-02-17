#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <unistd.h>
#include "helpers.h"

#define ID_LEN 10
#define TOPIC_LEN 50
#define BUF_LEN 1600

// trimite mesaj catre server
// formatul mesajului este: tipul comenzii si topicul
void send_msg(int sockfd, uint8_t type, char *topic) {
    char buf[BUF_LEN];

    memset(buf, 0, sizeof(buf));
    sprintf(buf, "%d %s", type, topic);

    int rc = send(sockfd, buf, strlen(buf), 0);
    DIE(rc < 0, "Error send message to server\n");
}

int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    if (argc != 4) {
        fprintf(stderr, "Usage: %s <ID_CLIENT> <IP_SERVER> <PORT_SERVER>\n", argv[0]);
        exit(0);
    }

    char *id_client = argv[1];
    char *ip_server = argv[2];
    int port_server = atoi(argv[3]);
    int rc;

    if (strlen(id_client) > ID_LEN) {
        fprintf(stderr, "Subscriber id must not exceed 10 characters\n");
        exit(1);
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(sockfd < 0, "Error opening socket\n");

    int flag = 1;
    // dezactiveaza algoritmul Nagle
    rc = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int));
    DIE(rc < 0, "Error setsockopt\n");

    struct sockaddr_in serv_addr;
    socklen_t socket_len = sizeof(struct sockaddr_in);

    memset(&serv_addr, 0, socket_len);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port_server);

    // converteste ip_server din text in binar
    rc = inet_pton(AF_INET, ip_server, &serv_addr.sin_addr);
    DIE(rc <= 0, "Error inet_pton\n");

    // se face conectarea la server
    rc = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    DIE(rc < 0, "Error connect\n");

    // trimite id-ul clientului catre server
    rc = send(sockfd, id_client, strlen(id_client), 0);
    DIE(rc < 0, "Error send id_client\n");

    char buf[BUF_LEN];
    memset(buf, 0, BUF_LEN);

    struct pollfd poll_fds[2];

    // pentru stdin(comenzi de la tastatura)
    poll_fds[0].fd = STDIN_FILENO;
    poll_fds[0].events = POLLIN;

    // socket pentru mesajele de la server
    poll_fds[1].fd = sockfd;
    poll_fds[1].events = POLLIN;

    while (1) {
        int aux = poll(poll_fds, 2, -1);
        DIE(aux < 0, "Error in poll\n");

        // verifica daca s-a primit ceva de la tasatura
        if (poll_fds[0].revents & POLLIN) {
            memset(buf, 0, BUF_LEN);
            fgets(buf, BUF_LEN - 1, stdin);
            buf[strcspn(buf, "\n")] = 0;

            // trimite cerere de deconectare
            if (strcmp(buf, "exit") == 0) {
                // pt deconectare - type = 2
                send_msg(sockfd, 2, "");
                break;
            }
            // trimite mesaj de subscribe
            else if (strncmp(buf, "subscribe", 9) == 0) {
                char topic[TOPIC_LEN];

                int ret = sscanf(buf + 10, "%s", topic);
                DIE(ret != 1, "The topic is missing\n");

                // pt subscribe - type = 0
                send_msg(sockfd, 0, topic);
                printf("Subscribed to topic %s\n", topic);
            }
            // trimite mesaj de unsubscribe
            else if (strncmp(buf, "unsubscribe", 11) == 0) {
                char topic[TOPIC_LEN];

                int ret = sscanf(buf + 12, "%s", topic);
                DIE(ret != 1, "The topic is missing\n");

                // pt unsubscribe - type = 1
                send_msg(sockfd, 1, topic);
                printf("Unsubscribed from topic %s\n", topic);
            }
            else {
                fprintf(stderr, "Unknown command\n");
                close(sockfd);
                exit(0);
            }
        }
        // verifica daca s-au primit date de la server
        if (poll_fds[1].revents & POLLIN) {
            memset(buf, 0, BUF_LEN);

            int bytes_rcv = recv(sockfd, buf, sizeof(buf), 0);

            if (bytes_rcv == 0) {
                break;
            }
            // serverul a primit exit de la tastatura
            // se va inchide si clientul
            if (strncmp(buf, "STOP", 4) == 0) {
                break;
            }

            DIE(bytes_rcv < 0, "Error recv\n");
            // se afiseaza mesajul primit
            printf("%s", buf);
        }
    }
    close(sockfd);
    return 0;
}