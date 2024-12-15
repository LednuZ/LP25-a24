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
        {"backup", no_argument, &backup_flag, 'b'},
        {"restore", no_argument, &restore_flag, 'r'},
        {"list-backups", no_argument, &list_flag, 'l'},
        {"dry-run", no_argument, &dry_run_flag, 'y'},
        {"d-server", required_argument, NULL, 'j'},
        {"d-port", required_argument, NULL, 'k'},
        {"s-server", required_argument, NULL, 'm'},
        {"s-port", required_argument, NULL, 'n'},
        {"dest", required_argument, NULL, 'd'},
        {"source", required_argument, NULL, 's'},
        {"verbose", no_argument, &verbose_flag, 'v'},
        {0, 0, 0, 0}
    };

    int option_index = 0;
    int opt;

    const char *source_dir = NULL, *dest_dir = NULL, *dest_server_ip = NULL, *src_server_ip = NULL;
    int dest_server_port = 0, src_server_port = 0;

    while ((opt = getopt_long(argc, argv, "b:r:l", long_options, &option_index)) != -1) {
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
            case 'v': // --verbose
                verbose_flag = 1;
                break;
            case '?': // Unknown option
                fprintf(stderr, "Option non valide.\n");
                return EXIT_FAILURE;
        }
    }

    if ((backup_flag + restore_flag + list_flag) != 1) {
        fprintf(stderr, "Erreur: Vous devez utiliser une seule option parmi : --backup, --restore, --list_backups.\n");
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
        } else {
            create_backup(source_dir, dest_dir);
        }
    }

    if (restore_flag) {
        if (!source_dir) {
            fprintf(stderr, "Erreur: Vous devez spécifier le dossier de sauvergarde avec l'option --source.\n");
            return EXIT_FAILURE;
        } else {
            if (!dest_dir) {
                dest_dir = "/";
            }
            restore_backup(source_dir, dest_dir);
        }
    }

    if (list_flag) {
        if (!source_dir) {
            fprintf(stderr, "Erreur: Vous devez spécifier le dossier de sauvergarde avec l'option --source.\n");
            return EXIT_FAILURE;
        } else {
            list_backups(source_dir);
        }
    }

    return EXIT_SUCCESS;
}
