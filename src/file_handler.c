#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>
#include "file_handler.h"
#include "deduplication.h"

#define BUFFER_SIZE 1024

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
        while(fgets(buffer, BUFFER_SIZE, f)) {
            char *log = malloc(strlen(elt->path) + strlen(elt->date) + strlen(elt->md5) + 3) ;
            strcpy(log, elt->path) ;
            strcat(log, ";") ;
            strcat(log, elt->date) ;
            strcat(log, ";") ;
            strcat(log, elt->md5) ;

            if (elt == NULL || strcmp(buffer, log) == 0) {
                fwrite(&buffer, BUFFER_SIZE, 1, temp) ;
            } else {
                fwrite(&log, strlen(log), 1, temp) ;
            }

            elt = elt->next ;
            free(log) ;
        }

        while (elt != NULL) {
            fwrite(&log, strlen(log), 1, f) ;
            elt = elt->next ;
        }

        fclose(f) ;
        fclose(temp) ;

        remove(logfile) ;
        rename("temp.txt", logfile) ;
    } else {
        if (f) fclose(f) ;
        if (temp) fclose(temp) ;
        printf("Erreur : ouverture du fichier %s ou création du fichier temp.txt", logfile) ;
        return EXIT_FAILURE ;
    }
}

void write_log_element(log_element *elt, FILE *logfile){
  /* Implémenter la logique pour écrire un élément log de la liste chaînée log_element dans le fichier .backup_log
   * @param: elt - un élément log à écrire sur une ligne
   *         logfile - le chemin du fichier .backup_log
   */
    FILE *f = fopen(logfile, "a") ;
    char buffer[BUFFER_SIZE] ;

    if (f) {
        fprintf(f, "%s;%s;%s", elt->path, elt->mtime, elt->md5) ;
        fclose(f) ;
    } else {
        printf("Erreur : ouverture du fichier %s", logfile) ;
        return EXIT_FAILURE ;
    }
}

void list_files(const char *path){
  /* Implémenter la logique pour lister les fichiers présents dans un répertoire
  */
    struct dirent *entry ;
    DIR *dir = opendir(path) ;

    if (dir) {
        printf("Contenu du répertoire %s :\n", path) ;

        while ((entry = readdir(dir)) != NULL) {
            // Ignorer les entrées spéciales "." et ".."
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
    
            // Construire le chemin complet
            char full_path[1024] ;
            snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name) ;
    
            // Vérifier si c'est un fichier ou un répertoire
            struct stat file_stat ;
            if (stat(full_path, &file_stat) == 0) {
                if (S_ISDIR(file_stat.st_mode)) {
                    printf("[Répertoire] %s\n", entry->d_name) ;
                } else if (S_ISREG(file_stat.st_mode)) {
                    printf("[Fichier] %s\n", entry->d_name) ;
                } else {
                    printf("[Autre] %s\n", entry->d_name) ;
                }
            } else {
                perror("Erreur lors de l'obtention des informations sur le fichier") ;
            }
        }
    
        closedir(dir) ;
    } else {
        perror("Erreur d'ouverture du répertoire") ;
        return EXIT_FAILURE ;
    }
}

void copy_file(const char *src, const char *dest){
    FILE *src_file = fopen(src, "rb") ; // Ouvre le fichier source en mode lecture binaire
    if (!src_file) {
        perror("Erreur d'ouverture du fichier source") ;
        return EXIT_FAILURE ;
    }

    FILE *dest_file = fopen(dest, "wb") ; // Ouvre le fichier destination en mode écriture binaire
    if (!dest_file) {
        perror("Erreur d'ouverture du fichier destination") ;
        fclose(src_file) ;
        return EXIT_FAILURE ;
    }

    char buffer[BUFFER_SIZE] ;
    size_t bytes_read ;

    // Lire et écrire par blocs
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, src_file)) > 0) {
        if (fwrite(buffer, 1, bytes_read, dest_file) != bytes_read) {
            perror("Erreur d'écriture dans le fichier destination") ;
            fclose(src_file) ;
            fclose(dest_file) ;
            return ;
        }
    }

    if (ferror(src_file)) {
        perror("Erreur de lecture du fichier source") ;
    }

    fclose(src_file) ;
    fclose(dest_file) ;
}
