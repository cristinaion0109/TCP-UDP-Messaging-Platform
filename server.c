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
#include <math.h>
#include "helpers.h"

#define ID_LEN 10
#define TOPIC_LEN 50
#define BUF_LEN 1600
#define INITIAL_CLIENTS 100

typedef struct {
    int sockfd;
    char id[ID_LEN + 1];
    int status;
    // retine topicuri primite de la tastatura
    char subscribed_topics[51][51];
    // retine topicuri in forma completa
    // fara + sau *
    char subscribed_topics_complete[51][51];
    int topic_count;
    int complete_count;
} client;

// cauta un client dupa socket
int search_client(int sockfd, client *clients, int clients_count) {
    int index = -1;
    for (int i = 0; i < clients_count; i++) {
        if (clients[i].sockfd == sockfd) {
            index = i;
            break;
        }
    }
    return index;
}

// numara cate nivele sunt intr-un subiect
// pe baza separatorului /
int levels_count(char *topic, char levels[51][51]) {
    int count = 0;
    char *start = topic;
    char *p = strchr(start, '/');

    while(p) {
        int aux = p - start;
        strncpy(levels[count], start, aux);
        levels[count][aux] = '\0';
        count++;
        start = p + 1;
        p = strchr(start, '/');
    }
    strcpy(levels[count], start);
    count++;
    return count;
}

// realizeaza potriviri intre nivele
int search_match_aux(char subscr_levels[51][51], char topic_levels[51][51], int subscr_count, 
        int topic_count, int subscr_idx, int topic_idx) {
    // am ajuns la finalul ambelor liste de cuvinte
    // adica exista o potrivire completa
    if ((subscr_count == subscr_idx) && (topic_count == topic_idx)) {
        return 1;
    }
    // lista de cuvinte care contine wildcarduri
    // s-a terminat inaintea listei de cuvinte complete
    // adica in final nu exista o potrivire
    if ((subscr_idx == subscr_count) && (topic_count > topic_idx)) {
        return 0;
    }
    // nivelele curente sunt la identice
    if (strcmp(subscr_levels[subscr_idx], topic_levels[topic_idx]) == 0) {
        return search_match_aux(subscr_levels, topic_levels, subscr_count, topic_count, subscr_idx + 1, topic_idx+1);
    }
    if (strcmp(subscr_levels[subscr_idx], "+") == 0) {
        if (topic_count > topic_idx) {
            // trecem la urmatorul nivel
            return search_match_aux(subscr_levels, topic_levels, subscr_count, topic_count, subscr_idx + 1, topic_idx+1);
        }
        return 0;
    }
    if (strcmp(subscr_levels[subscr_idx], "*") == 0) {
        for (int i = topic_idx; i < topic_count; i++) {
            if (search_match_aux(subscr_levels, topic_levels, subscr_count, topic_count, subscr_idx + 1, i+1)) {
                return 1;
            }
        }
        return 0;
    }
    return 0;
}

// cauta o potrivire intre un topic primit de la tastatura
// si un topic complet
int search_match(char *subscr, char *topic) {
    char subsc_levels[51][51], topic_level[51][51];
    int subscr_count = levels_count(subscr, subsc_levels);
    int topic_count = levels_count(topic, topic_level);
    return search_match_aux(subsc_levels, topic_level, subscr_count, topic_count, 0, 0);
}

// verifica daca un client este abonat la un topic
int is_subscribed2(client c, char topic[51]) {
    for (int i = 0; i < c.complete_count; i++) {
        if (search_match(c.subscribed_topics_complete[i], topic)) {
            return 1;
        }
    }
    return 0;
}

// verifica daca un client este deja abonat la un
// anumit topic
int is_topic(client c, char topic[51]) {
    for (int i = 0; i < c.complete_count; i++) {
        if (strcmp(c.subscribed_topics_complete[i], topic) == 0) {
            return 1;
        }
    }
    return 0;
}

int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <PORT>\n", argv[0]);
        exit(0);
    }

    int port = atoi(argv[1]);
    int rc;

    int udp_sockfd, tcp_sockfd;

    udp_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    DIE(udp_sockfd < 0, "UDP socket error\n");

    tcp_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(tcp_sockfd < 0, "Socket error\n");

    int flag = 1;
    // dezactiveaza algoritmul Nagle
    rc = setsockopt(tcp_sockfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int));
    DIE(rc < 0, "Error setsockopt\n");

    struct sockaddr_in serv_addr, client_addr;
    socklen_t socket_len = sizeof(struct sockaddr_in);

    memset(&serv_addr, 0, socket_len);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    memset(&client_addr, 0, socket_len);
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(port);
    client_addr.sin_addr.s_addr = INADDR_ANY;

    rc = bind(tcp_sockfd, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr));
    DIE(rc < 0, "Error bind\n");

    rc = bind(udp_sockfd, (struct sockaddr *)&client_addr, sizeof(struct sockaddr));
    DIE(rc < 0, "Error bind\n");

    rc = listen(tcp_sockfd, INITIAL_CLIENTS);
    DIE(rc < 0, "Error listen\n");

    struct pollfd poll_fds[3 + INITIAL_CLIENTS];

    poll_fds[0].fd = STDIN_FILENO;
    poll_fds[0].events = POLLIN;

    poll_fds[1].fd = tcp_sockfd;
    poll_fds[1].events = POLLIN;

    poll_fds[2].fd = udp_sockfd;
    poll_fds[2].events = POLLIN;

    client *clients = malloc(INITIAL_CLIENTS * sizeof(client));
    DIE(clients == NULL, "Error malloc clients\n");

    int client_count = 0;
    int start_index = 3;
    int fd_count = 3;

    while (1) {
        int aux = poll(poll_fds, start_index, -1);
        DIE(aux < 0, "Error poll\n");

        for (int i = 0; i < start_index; i++) {
            if (poll_fds[i].fd == -1)
                continue;
            // verifica evenimentele pentru fiecare socket
            if (poll_fds[i].revents & POLLIN) {
                // comanda de la tastura
                if (poll_fds[i].fd  == STDIN_FILENO) {
                    char buf[BUF_LEN];
                    fgets(buf, BUF_LEN - 1, stdin);
                    buf[strcspn(buf, "\n")] = 0;

                    // daca s-a primit exit, se vor inchide
                    // atat serverul, cat si clientii
                    if (strncmp(buf, "exit", 4) == 0) {
                        for (int j = 0; j < client_count; j++) {
                            if(clients[j].status == 1) {
                                printf("Client %s disconnected.\n", clients[j].id);
                                send(clients[j].sockfd, "STOP", 4, 0);
                                close(clients[j].sockfd);
                            }
                        }
                        close(tcp_sockfd);
                        close(udp_sockfd);
                        exit(0);
                    }
                }
                else if (poll_fds[i].fd == tcp_sockfd) {
                    int client_sockfd = accept(tcp_sockfd, (struct sockaddr *)&client_addr, &socket_len);
                    DIE(client_sockfd < 0, "Error accept\n");

                    rc = setsockopt(client_sockfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int));
                    DIE(rc < 0, "Error setsockopt\n");

                    if (client_count >= INITIAL_CLIENTS) {
                        clients = realloc(clients, (client_count * 2) * sizeof(client));
                        DIE(clients == NULL, "Error realloc\n");
                    }

                    char id[ID_LEN + 1];
                    // primeste id-ul clientului
                    int bytes_rcv = recv(client_sockfd, id, sizeof(id), 0);
                    DIE(bytes_rcv <= 0, "Error recv\n");

                    id[bytes_rcv] = '\0';

                    int flag = -1;

                    for (int j = 0; j < client_count; j++) {
                        if (strcmp(clients[j].id, id) == 0) {
                            flag = j;
                            break;
                        }
                    }

                    // daca clientul nu este in lista de clienti
                    // il adaug
                    if (flag == -1) {
                        fd_count++;

                        clients[client_count].sockfd = client_sockfd;
                        clients[client_count].status = 1;
                        clients[client_count].topic_count = 0;
                        clients[client_count].complete_count = 0;

                        poll_fds[start_index].fd = client_sockfd;
                        poll_fds[start_index].events = POLLIN;
                        
                        strcpy(clients[client_count].id, id);

                        int client_port = ntohs(client_addr.sin_port);
                        char client_ip[INET_ADDRSTRLEN];

                        strcpy(client_ip, inet_ntoa(client_addr.sin_addr));

                        printf("New client %s connected from %s:%d.\n", id, client_ip, client_port);

                        client_count++;
                        start_index++;
                    }
                    // daca clientul este in lista de clienti, dar e deconectat
                    // il reconectez
                    else if (flag != -1 && (clients[flag].status == 0)) {
                        clients[flag].sockfd = client_sockfd;
                        clients[flag].status = 1;
                        poll_fds[flag + 3].fd = client_sockfd;

                        int client_port = ntohs(client_addr.sin_port);
                        char client_ip[INET_ADDRSTRLEN];

                        strcpy(client_ip, inet_ntoa(client_addr.sin_addr));
                        printf("New client %s connected from %s:%d\n", id, client_ip, client_port);
                    }
                    // daca clientul este deja conectat
                    // se inchide conexiunea
                    else {
                        printf("Client %s already connected.\n", id);
                        send(client_sockfd, "STOP", 4, 0);
                        close(client_sockfd);
                    }
                }
                else if (poll_fds[i].fd == udp_sockfd) {
                    char buf[BUF_LEN];
                    struct sockaddr_in udp_client_addr;
                    socklen_t udp_len = sizeof(udp_client_addr);

                    memset(buf, 0, sizeof(buf));
                    memset(&udp_client_addr, 0, sizeof(udp_client_addr));

                    int bytes_recv = recvfrom(udp_sockfd, buf, sizeof(buf), 0, (struct sockaddr *)&udp_client_addr, &udp_len);
                    DIE(bytes_recv < 0, "Error recv udp\n");

                    char topic[TOPIC_LEN + 1];
                    strncpy(topic, buf, TOPIC_LEN);
                    topic[TOPIC_LEN] = '\0';

                    int type = (int)buf[50];
                    char value[1500];
                    char type_data[20];

                    // procesam mesajele in functie de tip
                    if (type == 0) {
                        uint8_t sign = buf[51];
                        uint32_t nr;
                        int final_nr;

                        memcpy(&nr, buf + 52, sizeof(uint32_t));
                        nr = ntohl(nr);

                        if(sign == 1) {
                            final_nr = -((int)nr);
                        } else {
                            final_nr = (int)nr;
                        }
                        sprintf(value, "%d", final_nr);
                        strcpy(type_data, "INT");
                    }
                    else if (type == 1) {
                        uint16_t nr;

                        memcpy(&nr, buf + 51, sizeof(uint16_t));
                        nr = ntohs(nr);

                        double final_nr = nr / 100.0;
                        sprintf(value, "%.2f", final_nr);
                        strcpy(type_data, "SHORT_REAL");
                    }
                    else if (type == 2) {
                        uint8_t sign = buf[51];
                        uint32_t nr;

                        memcpy(&nr, buf + 52, sizeof(uint32_t));
                        nr = ntohl(nr);

                        uint8_t power = buf[56];

                        double final_nr = (double)nr / pow(10, power);

                        if (sign == 1) {
                            final_nr = -final_nr;
                        }

                        sprintf(value, "%.4f", final_nr);
                        strcpy(type_data, "FLOAT");
                    }
                    else if (type == 3) {
                        strcpy(value, (char *)(buf + 51));
                        strcpy(type_data, "STRING");
                    }
                    char message[BUF_LEN];

                    // se construieste mesajul ce va fi trimit catre
                    // clientii tcp
                    snprintf(message, sizeof(message), "%s - %s - %s\n", topic, type_data, value);

                    //se face update la lista de topicuri complete
                    for (int j = 0; j < client_count; j++) {
                        for(int t = 0; t < clients[j].topic_count; t++) {
                            if(search_match(clients[j].subscribed_topics[t], topic)) {
                                if (is_topic(clients[j], topic) == 0) {
                                    strcpy(clients[j].subscribed_topics_complete[clients[j].complete_count], topic);
                                    clients[j].complete_count++;
                                }
                            }
                        }

                        // se trimite mesajul la clientii abonati la
                        // topicul respectiv
                        for(int k = 0; k < clients[j].complete_count; k++) {
                            if ((strcmp(clients[j].subscribed_topics_complete[k], topic) == 0) && (clients[j].status == 1)) {
                                rc = send(clients[j].sockfd, message, strlen(message), 0);
                                DIE(rc < 0, "Error send to TCP client\n");
                            }
                        }
                    }
                }
                // se primesc mesaje de la clientii tcp existenti
                else {
                    char buffer[BUF_LEN];
                    memset(buffer, 0, BUF_LEN);

                    int bytes_rcv = recv(poll_fds[i].fd, buffer, BUF_LEN - 1, 0);
                    DIE(bytes_rcv < 0, "Error recv from client TCP\n");

                    if (bytes_rcv == 0) {
                        int client_index = search_client(poll_fds[i].fd, clients, client_count);

                        if (client_index != -1) {
                            printf("Client %s disconnected.\n", clients[client_index].id);
                            close(clients[client_index].sockfd);
                            close(poll_fds[3+client_index].fd);
                            poll_fds[3 + client_index].fd = -1;
                            clients[client_index].status = 0;
                        }

                        poll_fds[i].fd = -1;
                        continue;
                    }
                    int type;
                    char topic[TOPIC_LEN + 1];

                    int ret = sscanf(buffer, "%d %s", &type, topic);

                    if (ret > 2) {
                        printf("Invalid message format\n");
                        continue;
                    }

                    int index = search_client(poll_fds[i].fd, clients, client_count);

                    if (index == -1) {
                        continue;
                    }
                    // clientul face subscribe la un topic
                    if (type == 0) {
                        strncpy(clients[index].subscribed_topics[clients[index].topic_count], topic, TOPIC_LEN);
                        clients[index].subscribed_topics[clients[index].topic_count][TOPIC_LEN] = '\0';
                        clients[index].topic_count++;
                    }
                    // clientul face unsusbcribe la un topic
                    else if (type == 1) {
                        for (int t = 0; t < clients[index].complete_count; t++) {
                            if (search_match(topic, clients[index].subscribed_topics_complete[t])) {
                                for (int t1 = 0; t1 < clients[index].topic_count; t1++) {
                                    //if (strcmp(clients[index].subscribed_topics[t], topic) == 0) {
                                    if (search_match(clients[index].subscribed_topics[t1], clients[index].subscribed_topics_complete[t])) {
                                        for (int k = t1; k < clients[index].topic_count - 1; k++) {
                                            strcpy(clients[index].subscribed_topics[k], clients[index].subscribed_topics[k + 1]);
                                        }
                                        clients[index].topic_count--;
                                        t1--;
                                    }
                                }
                                for (int k = t; k < clients[index].complete_count - 1; k++) {
                                    strcpy(clients[index].subscribed_topics_complete[k], clients[index].subscribed_topics_complete[k + 1]);
                                }
                                clients[index].complete_count--;
                                t--;
                            }
                        }
                    }

                    // clientul primeste exit de la tastatura
                    // se va deconecta clientul respectiv
                    else if (type == 2) {
                        printf("Client %s disconnected.\n", clients[index].id);
                        close(clients[index].sockfd);
                        close(poll_fds[i].fd);
                        poll_fds[i].fd = -1;
                        clients[index].status = 0;
                    }
                    else {
                        printf("Unknown command: %c\n", type);
                    }
                }
            }
        }
    }
    for (int j = 0; j < client_count; j++) {   
        close(clients[j].sockfd);
    }
    close(tcp_sockfd);
    close(udp_sockfd);
    return 0;
}
