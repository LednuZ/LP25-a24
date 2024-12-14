#include "backup_manager.h"
#include "deduplication.h"
#include "file_handler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <openssl/md5.h>


#define MAX_CHUNKS 10000
#define MAX_SIZE_PATH 2048

extern int dry_run_flag;

/**
 * @brief Vérifie si un fichier ou un répertoire existe.
 * @param path Chemin du fichier ou répertoire.
 * @return 1 si le fichier existe, 0 sinon.
 */
int file_exists(const char *path) {
    struct stat st;
    return (stat(path, &st) == 0);
}

/**
 * @brief Crée un répertoire si celui-ci n'existe pas déjà.
 * @param path Chemin du répertoire à créer.
 * @return 0 en cas de succès, -1 en cas d'erreur.
 */
int create_directory(const char *path) {
    if (mkdir(path, 0755) == -1 && errno != EEXIST) {
        return -1;
    }
    return 0;
}

/**
 * @brief Génère un horodatage du type YYYY-MM-DD-hh:mm:ss.mmm.
 * @param buffer Tampon où stocker l'horodatage.
 * @param size Taille du tampon buffer.
 */
void get_timestamp(char *buffer, size_t size) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm *tm_info = localtime(&tv.tv_sec);
    snprintf(buffer, size, "%04d-%02d-%02d-%02d:%02d:%02d.%03ld",
             tm_info->tm_year+1900, tm_info->tm_mon+1, tm_info->tm_mday,
             tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec, tv.tv_usec/1000);
}

/**
 * @brief Trouve la dernière sauvegarde (répertoire horodaté le plus récent) dans backup_dir.
 * @param backup_dir Répertoire contenant les sauvegardes.
 * @param last_backup_dir Tampon où stocker le chemin complet de la dernière sauvegarde.
 * @param size Taille du tampon last_backup_dir.
 */
void find_last_backup(const char *backup_dir, char *last_backup_dir, size_t size) {
    DIR *dir = opendir(backup_dir);
    if (!dir) return;
    struct dirent *entry;
    char latest[2048] = {0};
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        char path[2048];
        snprintf(path, sizeof(path), "%s/%s", backup_dir, entry->d_name);
        struct stat st;
        if (stat(path, &st)==0 && S_ISDIR(st.st_mode)) {
            if (strcmp(entry->d_name, ".backup_log") == 0) continue;
            if (latest[0] == '\0') {
                strncpy(latest, entry->d_name, sizeof(latest)-1);
            } else {
                if (strcmp(entry->d_name, latest) > 0) {
                    strncpy(latest, entry->d_name, sizeof(latest)-1);
                }
            }
        }
    }
    closedir(dir);
    if (latest[0] != '\0') {
        snprintf(last_backup_dir, size, "%s/%s", backup_dir, latest);
    }
}

/**
 * @brief Crée une nouvelle sauvegarde incrémentale.
 *
 * Cette fonction crée un répertoire horodaté pour la nouvelle sauvegarde. Si c'est
 * la première sauvegarde, elle déduplique tous les fichiers. Sinon, elle vérifie
 * s'ils ont changé depuis la dernière sauvegarde. Les fichiers inchangés sont liés
 * par lien physique depuis la précédente sauvegarde, tandis que les fichiers modifiés
 * sont redédupliqués.
 *
 * @param source_dir Chemin du répertoire source à sauvegarder.
 * @param backup_dir Chemin du répertoire de destination des sauvegardes.
 */
void create_backup(const char *source_dir, const char *backup_dir) {
    char backup_log_path[1024];
    snprintf(backup_log_path, sizeof(backup_log_path), "%s/.backup_log", backup_dir);
    int first_backup = !file_exists(backup_log_path);

    if (!file_exists(backup_dir)) {
        if (create_directory(backup_dir) != 0) {
            return;
        }
    }

    char timestamp[128];
    get_timestamp(timestamp, sizeof(timestamp));
    char new_backup_path[2048];
    snprintf(new_backup_path, sizeof(new_backup_path), "%s/%s", backup_dir, timestamp);
    if (create_directory(new_backup_path) != 0) {
        return;
    }

    log_t old_logs = {0};
    if (!first_backup) {
        old_logs = read_backup_log(backup_log_path);
    }

    char last_backup_dir[2048] = {0};
    if (!first_backup) {
        find_last_backup(backup_dir, last_backup_dir, sizeof(last_backup_dir));
    }

    DIR *dir = opendir(source_dir);
    if (!dir) {
        return;
    }

    log_t new_logs = {0};
    new_logs.head = NULL;
    new_logs.tail = NULL;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name,".")==0 || strcmp(entry->d_name,"..")==0) continue;

        char filepath[2048];
        snprintf(filepath, sizeof(filepath), "%s/%s", source_dir, entry->d_name);

        struct stat st;
        if (stat(filepath, &st) == 0 && S_ISREG(st.st_mode)) {
            unsigned char md5_sum[MD5_DIGEST_LENGTH];
            {
                FILE *fcheck = fopen(filepath, "rb");
                if (fcheck) {
                    unsigned char buffer[4096];
                    MD5_CTX ctx;
                    MD5_Init(&ctx);
                    size_t r;
                    while ((r=fread(buffer,1,sizeof(buffer),fcheck))>0) {
                        MD5_Update(&ctx, buffer, r);
                    }
                    MD5_Final(md5_sum, &ctx);
                    fclose(fcheck);
                } else {
                    memset(md5_sum,0,MD5_DIGEST_LENGTH);
                }
            }

            log_element *old_elt = NULL;
            if (!first_backup) {
                for (log_element *e = old_logs.head; e != NULL; e = e->next) {
                    if (strcmp(e->path, entry->d_name)==0) {
                        old_elt = e;
                        break;
                    }
                }
            }

            char dedup_filename[2048];
            snprintf(dedup_filename, sizeof(dedup_filename), "%s/%s.dedup", new_backup_path, entry->d_name);

            int file_unchanged = 0;
            if (!first_backup && old_elt != NULL && memcmp(old_elt->md5, md5_sum, MD5_DIGEST_LENGTH)==0) {
                char old_dedup_filename[2048];
                snprintf(old_dedup_filename, sizeof(old_dedup_filename), "%s/%s.dedup", last_backup_dir, entry->d_name);
                if (file_exists(old_dedup_filename)) {
                    if (link(old_dedup_filename, dedup_filename) == 0) {
                        file_unchanged = 1;
                    }
                }
            }

            if (!file_unchanged) {
                FILE *f = fopen(filepath, "rb");
                if (!f) continue;
                Chunk chunks[10000];
                Md5Entry hash_table[HASH_TABLE_SIZE];
                memset(chunks, 0, sizeof(chunks));
                memset(hash_table, 0, sizeof(hash_table));
                deduplicate_file(f, chunks, hash_table);
                fclose(f);

                int chunk_count = 0;
                for (int i=0; i<10000; i++) {
                    if (chunks[i].data == NULL) break;
                    chunk_count++;
                }

                write_backup_file(dedup_filename, chunks, chunk_count);
            }

            log_element *elt = (log_element*)malloc(sizeof(log_element));
            memset(elt,0,sizeof(log_element));
            elt->path = strdup(entry->d_name);

            char mod_time[128];
            struct tm *tm_info = localtime(&st.st_mtime);
            snprintf(mod_time, sizeof(mod_time), "%04d-%02d-%02d-%02d:%02d:%02d.%03d",
                     tm_info->tm_year+1900, tm_info->tm_mon+1, tm_info->tm_mday,
                     tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec, 0);
            elt->date = strdup(mod_time);
            memcpy(elt->md5, md5_sum, MD5_DIGEST_LENGTH);
            elt->next = NULL;
            elt->prev = new_logs.tail;
            if (new_logs.tail) {
                new_logs.tail->next = elt;
            } else {
                new_logs.head = elt;
            }
            new_logs.tail = elt;
        }
    }
    closedir(dir);

    update_backup_log(backup_log_path, &new_logs);
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

    char output_filename[MAX_SIZE_PATH];
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

/**
 * @brief Restaure une sauvegarde à partir d'un répertoire spécifié.
 *
 * Lit le .backup_log pour connaître la liste des fichiers, puis pour chaque
 * fichier dédupliqué, reconstruit le fichier original dans restore_dir.
 *
 * @param backup_id Chemin vers le répertoire de la sauvegarde.
 * @param restore_dir Répertoire de destination pour la restauration.
 */
void restore_backup(const char *backup_id, const char *restore_dir) {
    char backup_dir[MAX_SIZE_PATH];
    strcpy(backup_dir, backup_id);
    char *last_slash = strrchr(backup_dir, '/');
    if (last_slash) *last_slash = '\0';
    char backup_log_path[MAX_SIZE_PATH];
    snprintf(backup_log_path, sizeof(backup_log_path), "%s/.backup_log", backup_dir);
    if (!file_exists(backup_log_path)) {
        return;
    }

    log_t logs = read_backup_log(backup_log_path);
    create_directory(restore_dir);

    for (log_element *elt = logs.head; elt != NULL; elt = elt->next) {
        char dedup_file[MAX_SIZE_PATH];
        snprintf(dedup_file, sizeof(dedup_file), "%s/%s.dedup", backup_id, elt->path);
        if (!file_exists(dedup_file)) continue;
        FILE *fin = fopen(dedup_file, "rb");
        if (!fin) continue;
        Chunk *chunks = NULL;
        int chunk_count = 0;
        undeduplicate_file(fin, &chunks, &chunk_count);
        fclose(fin);

        char restored_file[MAX_SIZE_PATH];
        snprintf(restored_file, sizeof(restored_file), "%s/%s", restore_dir, elt->path);
        write_restored_file(restored_file, chunks, chunk_count);
        for (int i=0; i<chunk_count; i++) {
            free(chunks[i].data);
        }
        free(chunks);
    }
}

/**
 * @brief Liste les sauvegardes disponibles dans un répertoire.
 *
 * Affiche les noms de répertoires de sauvegarde (horodatés) présents dans backup_dir.
 *
 * @param backup_dir Répertoire contenant les différentes sauvegardes.
 */
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
        char path[MAX_SIZE_PATH];
        snprintf(path, sizeof(path), "%s/%s", backup_dir, entry->d_name);
        struct stat st;
        if (stat(path,&st) == 0 && S_ISDIR(st.st_mode)) {
            printf("%s\n", entry->d_name);
        }
    }
    closedir(dir);
}
