#ifndef FILE_HANDLER_H
#define FILE_HANDLER_H

#include <stdio.h>
#include <openssl/md5.h>

// Structure pour une ligne du fichier log
typedef struct log_element{
    const char *path; // Chemin du fichier/dossier
    unsigned char md5[MD5_DIGEST_LENGTH]; // MD5 du fichier dédupliqué
    char *date; // Date de dernière modification
    struct log_element *next;
    struct log_element *prev;
} log_element;

// Structure pour une liste de log représentant le contenu du fichier backup_log
typedef struct {
    log_element *head; // Début de la liste de log 
    log_element *tail; // Fin de la liste de log
} log_t;


// Fonction permettant de créer une structure log_element
log_element *create_element(char *path, char *mtime, char *md5);
// Fonction permettant de lire un fichier .backup_log
log_t read_backup_log(const char *logfile);
// Fonction permettant de mettre à jour le fichier .backup_log
void update_backup_log(const char *logfile, log_t *logs);
// Ecrit un élément log dans le fichier .backup_log
void write_log_element(log_element *elt, FILE *logfile);
// Liste les fichiers présents dans un répertoire
void list_files(const char *path);
// Copie un fichier depuis une source vers une destination
void copy_file(const char *src, const char *dest);

#endif // FILE_HANDLER_H


