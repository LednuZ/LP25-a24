#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "file_handler.h"
#include "deduplication.h"
#include "backup_manager.h"
#include "network.h"


<<<<<<< Updated upstream
    // Implémentation de la logique de sauvegarde et restauration
    // Exemples : gestion des options --backup, --restore, etc.
    int opt = 0;
    struct option my_opts[] = {
        {.name="--backup",.has_arg=0,.flag=NULL,.val='b'},  //à compléter
        {.name="--restore",.has_arg=0,.flag=NULL,.val='r'}, //à compléter
        {.name="--list-backups",.has_arg=0,.flag=NULL,.val='l'}, //à compléter
        {.name="--dry-run",.has_arg=0,.flag=NULL,.val='c'}, //à compléter
        {.name="--d-server",.has_arg=0,.flag=NULL,.val='i'}, //à compléter
        {.name="--d-port",.has_arg=0,.flag=NULL,.val='j'}, //à compléter
        {.name="--s-server",.has_arg=0,.flag=NULL,.val='k'}, //à compléter
        {.name="--s-port",.has_arg=0,.flag=NULL,.val='m'}, //à compléter
        {.name="--dest",.has_arg=0,.flag=NULL,.val='d'}, //à compléter
        {.name="--source",.has_arg=0,.flag=NULL,.val='s'}, //à compléter
        {.name="--verbose",.has_arg=0,.flag=NULL,.val='v'} //à compléter
    }

    while ((opt = getopt_long(argc, argv, ))!= -1){
        switch (opt){ //à compléter
            case 'b': // --backup
                break; 
            case 'r': // --restore
                break;
            case 'l': // --list-backups
                break;
            case 'c': // --dry-run
                break;
            case 'i': // --d-server
                break;
            case 'j': // --d-port
                break;
            case 'k': // --s-server
                break;
            case 'm': // --s-port
                break;
            case 'd': // --dest
                break;
            case 's': // --source
                break;
            case 'v': // --verbose
                break;
        } 
    }    
=======
int main(int argc, char *argv[]) {
    static int verbose_flag = 0;
    static int dry_run_flag = 0;

    static struct option long_options[] = {
        {"backup", no_argument, NULL, 'b'},
        {"restore", required_argument, NULL, 'r'},
        {"list-backups", no_argument, NULL, 'l'},
        {"d-server", required_argument, NULL, 'd'},
        {"d-port", required_argument, NULL, 'p'},
        {"s-server", required_argument, NULL, 's'},
        {"s-port", required_argument, NULL, 't'},
        {"dest", required_argument, NULL, 'e'},
        {"source", required_argument, NULL, 'f'},
        {"verbose", no_argument, &verbose_flag, 1},
        {0, 0, 0, 0}
    };

    int option_index = 0;
    int opt;

    const char *source_dir = NULL, *backup_dir = NULL, *server_ip = NULL, *dest_dir = NULL;
    int server_port = 0;

    while ((opt = getopt_long(argc, argv, "b:r:l", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'b': // --backup
                source_dir = optarg;
                break;
            case 'r': // --restore
                backup_dir = optarg;
                break;
            case 'l': // --list-backups
                break;
            case 'd': // --d-server
                server_ip = optarg;
                break;
            case 'p': // --d-port
                server_port = atoi(optarg);
                break;
            case 's': // --s-server
                server_ip = optarg;
                break;
            case 't': // --s-port
                server_port = atoi(optarg);
                break;
            case 'e': // --dest
                dest_dir = optarg;
                break;
            case 'f': // --source
                source_dir = optarg;
                break;
            case 'v': // --verbose
                verbose_flag = 1;
                break;
            case '?': // Unknown option
                fprintf(stderr, "Unknown option. Use --help for usage instructions.\n");
                return EXIT_FAILURE;
        }
    }

    if (!source_dir && !backup_dir && !dest_dir) {
        fprintf(stderr, "Error: Missing required options. Use --help for usage instructions.\n");
        return EXIT_FAILURE;
    }

    if (verbose_flag) {
        printf("Verbose mode enabled\n");
    }

    if (backup_dir) {
        if (dry_run_flag) {
            printf("[DRY-RUN] Would restore backup from %s to %s\n", backup_dir, dest_dir);
        } else {
            restore(backup_dir, dest_dir, server_ip, server_port, verbose_flag);
        }
    } else if (source_dir && dest_dir) {
        if (dry_run_flag) {
            printf("[DRY-RUN] Would create backup from %s to %s\n", source_dir, dest_dir);
        } else {
            create_backup(source_dir, dest_dir, server_ip, server_port, verbose_flag);
        }
    } else {
        fprintf(stderr, "Error: Invalid combination of options. Use --help for usage instructions.\n");
        return EXIT_FAILURE;
    }

>>>>>>> Stashed changes
    return EXIT_SUCCESS;
}
