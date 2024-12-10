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
        {.name="--backup",.has_arg=0,.flag=0,.val='b'},  //à compléter
        {.name="--restore",.has_arg=0,.flag=0,.val=''}, //à compléter
        {.name="--list-backups",.has_arg=0,.flag=0,.val=''}, //à compléter
        {.name="--dry-run",.has_arg=0,.flag=0,.val=''}, //à compléter
        {.name="--d-server",.has_arg=0,.flag=0,.val=''}, //à compléter
        {.name="--d-port",.has_arg=0,.flag=0,.val=''}, //à compléter
        {.name="--s-server",.has_arg=0,.flag=0,.val=''}, //à compléter
        {.name="--s-port",.has_arg=0,.flag=0,.val=''}, //à compléter
        {.name="--dest",.has_arg=0,.flag=0,.val=''}, //à compléter
        {.name="--source",.has_arg=0,.flag=0,.val=''}, //à compléter
        {.name="--verbose",.has_arg=0,.flag=0,.val=''} //à compléter
    }

    while ((opt = getopt(argc, argv, ))!= -1){
        switch (opt){ //à compléter
            case 'b': //à compléter
                break; //à compléter
        } 
    }    
    return EXIT_SUCCESS;
}

