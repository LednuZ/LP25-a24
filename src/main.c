#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "file_handler.h"
#include "deduplication.h"
#include "backup_manager.h"
#include "network.h"

int verbose_flag = 0;
int dry_run_flag = 0;
static int backup_flag = 0;
static int restore_flag = 0;
static int list_flag = 0;

int main(int argc, char *argv[]) {
    static struct option long_options[] = {
        {"backup", no_argument, NULL, 'b'},
        {"restore", no_argument, NULL, 'r'},
        {"list-backups", no_argument, NULL, 'l'},
        {"dry-run", no_argument, &dry_run_flag, 'y'},
        {"d-server", required_argument, NULL, 'j'},
        {"d-port", required_argument, NULL, 'k'},
        {"s-server", required_argument, NULL, 'm'},
        {"s-port", required_argument, NULL, 'n'},
        {"dest", required_argument, NULL, 'd'},
        {"source", required_argument, NULL, 's'},
        {"verbose", no_argument, &verbose_flag, 1},
        {0, 0, 0, 0}
    };

    int option_index = 0;
    int opt;
    int instance = -1;

    const char *source_dir = NULL, *dest_dir = NULL, *dest_server_ip = NULL, *src_server_ip = NULL;
    int dest_server_port = 0, src_server_port = 0;

    while ((opt = getopt_long(argc, argv, "brlyj:k:m:n:d:s:v", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'b': // --backup
                backup_flag = 1;
                break;
            case 'r': // --restore
                restore_flag = 1;
                break;
            case 'l': // --list-backups
                list_flag = 1;
                break;
            case 'y': // --dry-run
                dry_run_flag = 1;
                break;
            case 'j': // --d-server
                dest_server_ip = optarg;
                break;
            case 'k': // --d-port
                dest_server_port = atoi(optarg);
                break;
            case 'm': // --s-server
                src_server_ip = optarg;
                break;
            case 'n': // --s-port
                src_server_port = atoi(optarg);
                break;
            case 'd': // --dest
                dest_dir = optarg;
                break;
            case 's': // --source
                source_dir = optarg;
                break;
            case '?': // Unknown option
                fprintf(stderr, "Option non valide.\n");
                return EXIT_FAILURE;
                break;
            default:
                break;
        }
    }

    if (strcmp("127.0.0.1",dest_server_ip) == 0) { // instance serveur
        instance = 1;
    }

    if (strcmp("127.0.0.1",src_server_ip) == 0) { // instance client
        instance = 2;
    }    

    if ((backup_flag) + (restore_flag) + (list_flag) != 1) {
        fprintf(stderr, "Erreur: Vous devez utiliser une seule option parmi : --backup, --restore, --list-backups.\n\n");
        return EXIT_FAILURE;
    }

    if (verbose_flag) {
        printf("Mode verbose actif\n");
    }

    if (dry_run_flag) {
        printf("Mode dry-run actif\n");
    }

    if (backup_flag) {
        if (!source_dir || !dest_dir) {
            fprintf(stderr, "Erreur: Vous devez spécifier les dossiers source et destination.\n");
            return EXIT_FAILURE;
        }
        if (verbose_flag) {
            printf("Début du backup de '%s' à '%s'\n",source_dir, dest_dir);
        }
        if (instance > 0) {
            if (verbose_flag) {
                printf("[INFO] Backup en réseau\n");
            }
            if (instance == 1) { //instance serveur
                // receive_data(); //On récupère les data, si c'est BACKUP, on lance la procédure
                // send_data(); // on renvoie le fichier .backup_log
                // receive_data(); // on récupère les données à sauvegarder
                // receive_data(); // on récupère le signal EXIT

            } else {
                if (instance = 2) { //instance client
                    send_data(dest_server_ip, dest_server_port, "BACKUP", 8); // Envoi de BACKUP au serveur
                    // receive_data(); // On récupère le fichier .backup pour le comparer ensuite
                    // dédupliquer les données

                    //send_data(); // on renvoie les données à sauvegarder au serveur
                    // send_data(); // on renvoie EXIT pour signaler la fin
                }
            }
        } else {
            create_backup(source_dir, dest_dir);
        }
    }

    if (restore_flag) {
        char *backup_id;
        if (!source_dir) {
            fprintf(stderr, "Erreur: Vous devez spécifier le dossier de sauvergarde avec l'option --source.\n");
            return EXIT_FAILURE;
        }
        if (!dest_dir) {
            dest_dir = "/";
        }
        if (instance > 0) {
            if (verbose_flag) {
                printf("[INFO] Restore en réseau\n");
            }
            if (instance == 1) { // instance serveur
                // send_data(src_server_ip, serv_server_port, .backup_log, size); //envoie le fichier .backup_log
                //undeduplicate les fichiers
                //tant qu'il y a encore de fichiers faire :
                    // send_data(src_server_ip, serv_server_port, fichier, size) ); // envoie les fichiers
                // send_data(src_server_ip, serv_server_port, "EXIT", size) ); 

            } else {
                if (instance = 2) { //instance client
                    send_data(dest_server_ip, dest_server_port, strcat("RESTORE ", backup_id), 32); // Envoi de RESTORE backup_id au serveur
                    //receive_data(); mise en écoute
                    // pour chaque fichier le mettre dans le dossier de restore
                }
            }
        } else {
            restore_backup(source_dir, dest_dir);
        }
    }

    if (list_flag) {
        if (!source_dir) {
            fprintf(stderr, "Erreur: Vous devez spécifier le dossier de sauvergarde avec l'option --source.\n");
            return EXIT_FAILURE;
        }
        if (instance > 0) {
            if (verbose_flag) {
                printf("[INFO] Backup en réseau\n");
            }
            if (instance == 1) { //instance serveur
                // receive_data(); //lecture de "LIST-BACKUP"
                // construction de la liste de backups
                // pour chaque element de la liste faire :
                    // send_data(src_server_ip, src_server_port, backup, size); //envoi de la liste_de_backups
                // send_data(src_server_ip, src_server_port, "EXIT", size); //envoi de EXIT
            } else {
                if (instance = 2) { //instance client
                    send_data(dest_server_ip, dest_server_port, "LIST-BACKUP", 8); // Envoi de LIST-BACKUP au serveur
                    // tant que receive_data() ne vaut pas "EXIT"
                        // afficher les resultats sur le terminal
                }
            }
        } else {
            list_backups(source_dir);
        }
    }

    return EXIT_SUCCESS;
}
