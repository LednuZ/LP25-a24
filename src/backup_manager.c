#include "backup_manager.h"
#include "deduplication.h"
#include "file_handler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <time.h>
#include <sys/stat.h>

#define MAX_CHUNKS 10000

// Fonction pour créer une nouvelle sauvegarde complète puis incrémentale
void create_backup(const char *source_dir, const char *backup_dir) {
    /* @param: source_dir est le chemin vers le répertoire à sauvegarder
    *          backup_dir est le chemin vers le répertoire de sauvegarde
    */
}

// Fonction permettant d'enregistrer dans fichier le tableau de chunk dédupliqué
void write_backup_file(const char *output_filename, Chunk *chunks, int chunk_count) {
    FILE *file = fopen(output_filename, "wb");
    if (file == NULL){
        perror("Erreur d'ouverture du fichier");
        return;
    }
    fwrite(&chunk_count, sizeof(int), 1, file);
    size_t chunk_size = CHUNK_SIZE;
    for (int i=0; i<chunk_count; i++) {
        fwrite(chunks[i].md5, MD5_DIGEST_LENGTH, 1, file);
        fwrite(&chunk_size, sizeof(size_t), 1, file);
        fwrite(chunks[i].data, 1, chunk_size, file);
    }
    fclose(file);
}


void backup_file(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (file == NULL){
        perror("Erreur d'ouverture du fichier");
        return;
    }
    Chunk chunks[MAX_CHUNKS];
    //memset(chunks, 0, sizeof(chunks));
    Md5Entry hash_table[HASH_TABLE_SIZE];
    //memset(hash_table, 0, sizeof(hash_table));
    deduplicate_file(file, chunks, hash_table);
    fclose(file);

    int chunk_count = 0;
    for (int i=0; i<MAX_CHUNKS; i++) {
        if (chunks[i].data == NULL){
            break;
        } 
        chunk_count++;
    }

    char output_filename[2048];
    snprintf(output_filename, sizeof(output_filename), "%s.dedup", filename);
    write_backup_file(output_filename, chunks, chunk_count);
}


void write_restored_file(const char *output_filename, Chunk *chunks, int chunk_count) {
    FILE *file = fopen(output_filename, "wb");
    if (!file) {
        perror("Erreur d'ouverture du fichier de destination pendant la restauration");
        return;
    }
    for (int i=0; i<chunk_count; i++) {
        fwrite(chunks[i].data, 1, CHUNK_SIZE, file);
    }
    fclose(file);
}

// Fonction pour restaurer une sauvegarde
void restore_backup(const char *backup_id, const char *restore_dir) {
    /* @param: backup_id est le chemin vers le répertoire de la sauvegarde que l'on veut restaurer
    *          restore_dir est le répertoire de destination de la restauration
    */
}

void list_backups(const char *backup_dir) {
    DIR *dir = opendir(backup_dir);
    if (dir == NULL){
        perror("Erreur pendant l'ouverture du dossier");
        return;       
    }
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name,".") == 0 || strcmp(entry->d_name,"..") == 0){
            continue;
        } 
        char path[2048];
        snprintf(path, sizeof(path), "%s/%s", backup_dir, entry->d_name);
        struct stat st;
        if (stat(path,&st) == 0 && S_ISDIR(st.st_mode)) {
            printf("%s\n", entry->d_name);
        }
    }
    closedir(dir);
}
