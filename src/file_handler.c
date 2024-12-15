#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include "file_handler.h"
#include "deduplication.h"

#define BUFFER_SIZE 1024

// Récupère les valeurs de verbose_flag et dry_run_flag
extern int verbose_flag;
extern int dry_run_flag;

// Fonction permettant de créer une structure log_element
log_element *create_element(char *path, char *mtime, char *md5) {
 /* Crée une structure log_element
  * @param: path - Chemin vers le fichier
  *         mtime - Dernière date de modification du fichier
  *         md5 - Hachage md5 du fichier
  * @return: un pointeur vers une structure log_element
  */
    log_element *new_elt = malloc(sizeof(log_element)) ;
    new_elt->path = strdup(path) ;
    new_elt->date = strdup(mtime) ;
    new_elt->date = strdup(md5) ;
    new_elt->next = NULL ;
    new_elt->prev = NULL ;

    if (verbose_flag) {
        printf("[INFO] Création d'un nouvel élément log : %s, %s, %s\n", path, mtime, md5);
    }

    return new_elt ;
}

// Fonction permettant de lire un fichier .backup_log
log_t read_backup_log(const char *logfile){
 /* Lecture des lignes du fichier ".backup_log"
  * @param: logfile - le chemin vers le fichier .backup_log
  * @return: une structure log_t
  */
    log_t backup = {.head = NULL, .tail = NULL} ;
    char buffer[BUFFER_SIZE] ;
    FILE *f = fopen(logfile, "r") ;

    if (verbose_flag) {
        printf("[INFO] Lecture du fichier %s\n", logfile);
    }

    if (f) {
        while (fgets(buffer, BUFFER_SIZE, f)) {
            // Supprime le saut de ligne
            buffer[strcspn(buffer, "\n")] = '\0';
            
            // Découper la ligne en chemin, date et MD5
            char *path = strtok(buffer, ";") ;
            char *mtime = strtok(NULL, ";") ;
            char *md5 = strtok(NULL, ";") ;

            if (verbose_flag) {
                printf("[INFO] Lecture de %s : %s, %s, %s\n", logfile, path, mtime, md5);
            }

            // Crée un nouvel élément et l'ajoute à la liste chaînée
            log_element *ligne = create_element(path, mtime, md5) ;

            if (backup.head == NULL) {
                backup.head = ligne ;
                backup.tail = ligne ;
            } else {
                ligne->prev = backup.tail ;
                backup.tail->next = ligne ;
                backup.tail = ligne ;
            }
        }
        fclose(f) ;
        return backup ;
    } else {
        perror("Erreur : ouverture du fichier\n") ;
        return backup ;
    }
}

// Fonction permettant de mettre à jour le fichier .backup_log
void update_backup_log(const char *logfile, log_t *logs){
 /* Modification d'une ou plusieurs ligne(s) du fichier ".bakcup_log"
  * @param: logfile - le chemin vers le fichier .backup_log
  *         logs - qui est la liste de toutes les lignes du fichier .backup_log sauvegardée dans une structure log_t
  */
    char buffer[BUFFER_SIZE] ;
    FILE *f = fopen(logfile, "r") ;
    FILE *temp = fopen("temp.txt", "w") ;

    if (verbose_flag) {
        printf("[INFO] Mise à jour du fichier %s\n", logfile);
    }

    if (f && temp) {
        log_element *elt = logs->head ;

        // Parcourt chaque ligne et effectue la mise à jour si nécessaire
        while(fgets(buffer, BUFFER_SIZE, f)) {
            char *log = malloc(strlen(elt->path) + strlen(elt->date) + strlen(elt->md5) + 3) ;
            strcpy(log, elt->path) ;
            strcat(log, ";") ;
            strcat(log, elt->date) ;
            strcat(log, ";") ;
            strcat(log, elt->md5) ;

            if (elt == NULL || strcmp(buffer, log) == 0) {
                if (verbose_flag) {
                    printf("[INFO] Conservation de la ligne %s\n", buffer);
                }
                fwrite(&buffer, BUFFER_SIZE, 1, temp) ;
            } else {
                if (verbose_flag) {
                    printf("[INFO] Mise à jour de la ligne %s en %s\n", buffer, log);
                }
                fwrite(log, strlen(log), 1, temp) ;
            }

            elt = elt->next ;
            free(log) ;
        }

        // Ajoute les nouvelles entrées restantes
        while (elt != NULL) {
            fprintf(temp, "%s;%s;%s", elt->path, elt->date, elt->md5);
            elt = elt->next ;
        }

        fclose(f) ;
        fclose(temp) ;

        // Remplace le fichier original par le fichier temporaire
        if (dry_run_flag) {
            if (verbose_flag) {
                printf("[DRY-RUN] Remplacement du fichier %s par temp.txt\n", logfile);
            }
        } else {
            remove(logfile) ;
            rename("temp.txt", logfile) ;
            if (verbose_flag) {
                printf("[INFO] Mise à jour du fichier %s effectuée\n", logfile);
            }
        }
    } else {
        if (f) fclose(f) ;
        if (temp) fclose(temp) ;
        perror("Erreur : Echec de l'ouverture du fichier en paramètre ou de la création du fichier temp.txt\n") ;
        return ;
    }
}

// Ecrit un élément log dans le fichier .backup_log
void write_log_element(log_element *elt, FILE *logfile){
 /* Ecrire un élément log de la liste chaînée log_element dans le fichier .backup_log
   * @param: elt - un élément log à écrire sur une ligne
   *         logfile - le chemin du fichier .backup_log
   */
    if (logfile) {
        if (dry_run_flag) {
            fprintf(logfile, "%s;%s;%s", elt->path, elt->date, elt->md5) ;
        }
        if (verbose_flag) {
            printf("[INFO] Écriture de l'élément log %s, %s, %s\n", elt->path, elt->date, elt->md5);
        }
        fclose(logfile) ;
    } else {
        printf("Erreur : échec ouverture du fichier\n") ;
        return ;
    }
}

// Liste les fichiers présents dans un répertoire
void list_files(const char *path){
 /* Implémenter la logique pour lister les fichiers présents dans un répertoire
  * @param: path - Chemin vers le répertoire
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
                perror("Erreur : échec obtention des informations sur le fichier\n") ;
            }
        }
    
        closedir(dir) ;
    } else {
        perror("Erreur : échec ouverture du répertoire\n") ;
        return ;
    }
}

// Copie un fichier depuis une source vers une destination
void copy_file(const char *src, const char *dest){
 /* Copie un fichier depuis une source vers une destination 
  * @param: src - Chemin du fichier source
  *         dest - Chemin du fichier de destination
  */
    FILE *src_file = fopen(src, "rb") ; // Ouvre le fichier source en mode lecture binaire
    if (!src_file) {
        perror("Erreur : échec ouverture du fichier source\n") ;
        return ;
    }

    FILE *dest_file = fopen(dest, "wb") ; // Ouvre le fichier destination en mode écriture binaire
    if (!dest_file) {
        perror("Erreur : échec ouverture du fichier destination\n") ;
        fclose(src_file) ;
        return ;
    }

    char buffer[BUFFER_SIZE] ;
    size_t bytes_read ;

    if (verbose_flag) {
        printf("[INFO] Copie du fichier %s dans le fichier %s\n", src, dest);
    }

    // Lire et écrire par blocs
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, src_file)) > 0) {
        if (fwrite(buffer, 1, bytes_read, dest_file) != bytes_read) {
            perror("Erreur d'écriture dans le fichier destination\n") ;
            fclose(src_file) ;
            fclose(dest_file) ;
            return ;
        }
    }

    if (ferror(src_file)) {
        perror("Erreur de lecture du fichier source\n") ;
    }

    fclose(src_file) ;
    fclose(dest_file) ;
}
