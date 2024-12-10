#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "file_handler.h"
#include "deduplication.h"
#include "backup_manager.h"
#include "network.h"

int main(int argc, char *argv[]) {
    // Analyse des arguments de la ligne de commande

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
    return EXIT_SUCCESS;
}

