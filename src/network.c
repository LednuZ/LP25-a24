#include "network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

void send_data(const char *server_address, int port, const void *data, size_t size) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Erreur de création du socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, server_address, &server_addr.sin_addr) <= 0) {
        perror("Adresse serveur invalide");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Échec de la connexion au serveur");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    if (send(sockfd, data, size, 0) < 0) {
        perror("Erreur lors de l'envoi des données");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    close(sockfd);
}

void receive_data(int port, void **data, size_t *size) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Erreur de création du socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_addr, client_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Erreur lors de l'association au port");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    listen(sockfd, 5);

    socklen_t client_len = sizeof(client_addr);
    int client_sock = accept(sockfd, (struct sockaddr *)&client_addr, &client_len);
    if (client_sock < 0) {
        perror("Erreur lors de l'acceptation de la connexion client");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    size_t buffer_size = 1024;
    *data = malloc(buffer_size);
    if (!*data) {
        perror("Erreur d'allocation mémoire");
        close(client_sock);
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    ssize_t received_size = recv(client_sock, *data, buffer_size, 0);
    if (received_size < 0) {
        perror("Erreur lors de la réception des données");
        free(*data);
        close(client_sock);
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    *size = (size_t)received_size;
    close(client_sock);
    close(sockfd);
}
