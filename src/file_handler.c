#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>
#include "file_handler.h"
#include "deduplication.h"

#define BUFFER_SIZE = 1024 ;

log_element create_element(char *path, char *mtime, char *md5) {
    log_element *new_elt = malloc(sizeof(log_element)) ;
    new_elt->path = strdup(path) ;
    new_elt->date = strdup(mtime) ;
    new_elt->date = strdup(md5) ;
    new_elt->next = NULL ;
    new_elt->prev = NULL ;

    return new_elt ;
}

// Fonction permettant de lire un élément du fichier .backup_log
log_t read_backup_log(const char *logfile){
    /* Implémenter la logique pour la lecture d'une ligne du fichier ".backup_log"
    * @param: logfile - le chemin vers le fichier .backup_log
    * @return: une structure log_t
    */
    log_t backup = {.head = NULL, .tail = NULL} ;
    char buffer[BUFFER_SIZE] ;
    FILE *f = fopen(logfile, "r") ;

    if (f) {
        while (fgets(buffer, BUFFER_SIZE, f) {
            buffer[strcspn(buffer, "\n")] = '\0';

            char *path = strtok(buffer, ";") ;
            char *mtime = strtok(NULL, ";") ;
            char *md5 = strtok(NULL, ";") ;

            log_element *ligne = create_element(path, mtime, md5) ;

            if (backup->head == NULL) {
                backup->head = ligne ;
                backup->tail = ligne ;
            } else {
                ligne->prev = backup->tail ;
                backup->tail->next = ligne ;
                backup->tail = ligne ;
            }
        }
        fclose(f) ;
        return backup ;
    } else {
        printf("Erreur : ouverture du fichier %s", logfile) ;
        return backup ;
    }
}

// Fonction permettant de mettre à jour une ligne du fichier .backup_log
void update_backup_log(const char *logfile, log_t *logs){
  /* Implémenter la logique de modification d'une ligne du fichier ".bakcup_log"
  * @param: logfile - le chemin vers le fichier .backup_log
  *         logs - qui est la liste de toutes les lignes du fichier .backup_log sauvegardée dans une structure log_t
  */
    char buffer[BUFFER_SIZE] ;
    FILE *f = fopen(logfile, "r") ;
    FILE *temp = fopen("temp.txt", "w") ;

    if (f && temp) {
        log_element *elt = logs->head ;
        while(fgets(buffer, BUFFER_SIZE, f) && elt != NULL) {
            char *log = malloc(strlen(elt->path) + strlen(elt->date) + strlen(elt->md5) + 3) ;
            strcpy(log, elt->path) ;
            strcat(log, ";") ;
            strcat(log, elt->date) ;
            strcat(log, ";") ;
            strcat(log, elt->md5) ;

            if (strcmp(buffer, log) == 0) {
                fwrite(&buffer, BUFFER_SIZE, 1, f) ;
            } else {
                fwrite(&log, strlen(log), 1, f) ;
            }

            elt = elt->next ;
            free(log) ;
        }

        if (elt == NULL) {
            while (fgets(buffer, BUFFER_SIZE, f)) {
                fwrite(&buffer, BUFFER_SIZE, 1, f) ;
            }
        } else if () {

        }
    }
}

void write_log_element(log_element *elt, FILE *logfile){
  /* Implémenter la logique pour écrire un élément log de la liste chaînée log_element dans le fichier .backup_log
   * @param: elt - un élément log à écrire sur une ligne
   *         logfile - le chemin du fichier .backup_log
   */
}

void list_files(const char *path){
  /* Implémenter la logique pour lister les fichiers présents dans un répertoire
  */
}

void copy_file(const char *src, const char *dest){

}
