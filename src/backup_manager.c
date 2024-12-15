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

extern int verbose_flag;
extern int dry_run_flag;

/**
 * @brief Teste l'existence d'un fichier ou répertoire.
 */
static int file_exists_local(const char *path)
{
    struct stat st;
    int exists = (stat(path, &st) == 0);

    if (verbose_flag) {
        if (exists) {
            printf("[INFO] Le fichier existe : %s\n", path);
        } else {
            printf("[INFO] Le fichier n'existe pas : %s\n", path);
        }
    }

    return exists;
}

/**
 * @brief Crée un répertoire s'il n'existe pas déjà.
 */
static int create_directory_local(const char *path)
{
    if (verbose_flag) {
        printf("[INFO] Vérification de l'existence du répertoire : %s\n", path);
    }

    if (dry_run_flag) {
        if (verbose_flag) {
            printf("[DRY-RUN] Le répertoire serait créé s'il n'existe pas : %s\n", path);
        }
        return 0;
    }

    if (mkdir(path, 0755) == -1 && errno != EEXIST) {
        if (verbose_flag) {
            perror("[ERROR] Échec de la création du répertoire");
        }
        return -1;
    }

    if (verbose_flag) {
        printf("[INFO] Le répertoire a été créé ou existe déjà : %s\n", path);
    }

    return 0;
}

/**
 * @brief Génère un horodatage sous forme YYYY-MM-DD-hh:mm:ss.sss.
 */
static void get_timestamp_local(char *buffer, size_t size)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm *tm_info = localtime(&tv.tv_sec);
    snprintf(
        buffer, size, "%04d-%02d-%02d-%02d:%02d:%02d.%03ld",
        tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
        tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec, tv.tv_usec / 1000
    );

    if (verbose_flag) {
        printf("[INFO] Horodatage généré : %s\n", buffer);
    }
}

/**
 * @brief Trouve le dernier répertoire de sauvegarde (le plus récent).
 */
static void find_last_backup_local(const char *backup_dir, char *last_backup_dir, size_t size)
{
    DIR *dir = opendir(backup_dir);
    if (!dir) {
        if (verbose_flag) {
            perror("[ERROR] Échec de l'ouverture du répertoire de sauvegarde");
        }
        last_backup_dir[0] = '\0';
        return;
    }

    struct dirent *entry;
    char latest[2048] = {0};

    if (verbose_flag) {
        printf("[INFO] Scan du répertoire : %s\n", backup_dir);
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        if (strcmp(entry->d_name, ".backup_log") == 0) {
            continue;
        }

        char path[2048];
        snprintf(path, sizeof(path), "%s/%s", backup_dir, entry->d_name);

        struct stat st;
        if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
            if (latest[0] == '\0') {
                strncpy(latest, entry->d_name, sizeof(latest) - 1);
            } else {
                if (strcmp(entry->d_name, latest) > 0) {
                    strncpy(latest, entry->d_name, sizeof(latest) - 1);
                }
            }
        }
    }
    closedir(dir);

    if (latest[0] != '\0') {
        snprintf(last_backup_dir, size, "%s/%s", backup_dir, latest);
        if (verbose_flag) {
            printf("[INFO] Le dernier répertoire de sauvegarde a été trouvé : %s\n", last_backup_dir);
        }
    } else {
        last_backup_dir[0] = '\0';
        if (verbose_flag) {
            printf("[INFO] Aucun répertoire de backup trouvé dans : %s\n", backup_dir);
        }
    }
}

/**
 * @brief Écrit dans un fichier de backup dédupliqué le tableau de chunks.
 * Si dry_run_flag est activé, n'écrit pas réellement, se contente d'afficher ce qui serait fait.
 */
void write_backup_file(const char *output_filename, Chunk *chunks, int chunk_count)
{
    if (dry_run_flag) {
        if (verbose_flag) {
            printf("[DRY-RUN] Écriture du fichier dédupliqué : %s avec %d chunks (non réalisée)\n", output_filename, chunk_count);
        }
        return; // Pas d'écriture réelle
    }

    FILE *file = fopen(output_filename, "wb");
    if (file == NULL) {
        perror("Erreur d'ouverture du fichier");
        return;
    }

    fwrite(&chunk_count, sizeof(int), 1, file);
    for (int i = 0; i < chunk_count; i++) {
        size_t chunk_size = CHUNK_SIZE; // Taille fixe du chunk
        fwrite(chunks[i].md5, MD5_DIGEST_LENGTH, 1, file);
        fwrite(&chunk_size, sizeof(size_t), 1, file);
        fwrite(chunks[i].data, 1, chunk_size, file);
    }
    fclose(file);

    if (verbose_flag) {
        printf("[INFO] Fichier dédupliqué écrit : %s avec %d chunks\n", output_filename, chunk_count);
    }
}

/**
 * @brief Met à jour le fichier .backup_log.
 * Si dry_run_flag est activé, n'écrit pas réellement, se contente d'afficher ce qui serait fait.
 */
static void update_backup_log_if_needed(const char *backup_log_path, log_t *new_logs)
{
    if (dry_run_flag) {
        if (verbose_flag) {
            printf("[DRY-RUN] Mise à jour de .backup_log (%s) non réalisée\n", backup_log_path);
        }
        return;
    }
    update_backup_log(backup_log_path, new_logs);
    if (verbose_flag) {
        printf("[INFO] .backup_log mis à jour : %s\n", backup_log_path);
    }
}

/**
 * @brief Copie un fichier si dry_run_flag est désactivé, sinon juste un message.
 */
static void copy_file_if_needed(const char *src, const char *dst)
{
    if (dry_run_flag) {
        if (verbose_flag) {
            printf("[DRY-RUN] Copie du fichier %s vers %s non réalisée\n", src, dst);
        }
    } else {
        copy_file(src, dst);
        if (verbose_flag) {
            printf("[INFO] Fichier copié de %s vers %s\n", src, dst);
        }
    }
}

/**
 * @brief Écrit un fichier restauré à partir d'un tableau de chunks.
 * Si dry_run_flag est activé, n'écrit pas réellement le fichier, juste un message.
 */
void write_restored_file(const char *output_filename, Chunk *chunks, int chunk_count)
{
    if (dry_run_flag) {
        if (verbose_flag) {
            printf("[DRY-RUN] Écriture du fichier restauré : %s (%d chunks) non réalisée\n", output_filename, chunk_count);
        }
        return;
    }

    FILE *file = fopen(output_filename, "wb");
    if (!file) {
        perror("Erreur d'ouverture du fichier de destination pendant la restauration");
        return;
    }
    for (int i = 0; i < chunk_count; i++) {
        fwrite(chunks[i].data, 1, CHUNK_SIZE, file);
    }
    fclose(file);

    if (verbose_flag) {
        printf("[INFO] Fichier restauré écrit : %s\n", output_filename);
    }
}

/**
 * @brief Crée une nouvelle sauvegarde incrémentale.
 */
void create_backup(const char *source_dir, const char *backup_dir)
{
    char backup_log_path[1024];
    snprintf(backup_log_path, sizeof(backup_log_path), "%s/.backup_log", backup_dir);
    int first_backup = !file_exists_local(backup_log_path);

    if (verbose_flag) {
        printf("[INFO] Début de la sauvegarde. Source : %s, Destination : %s\n", source_dir, backup_dir);
        if (first_backup) {
            printf("[INFO] Aucune sauvegarde précédente trouvée. Création de la première sauvegarde.\n");
        }
    }

    if (!file_exists_local(backup_dir)) {
        if (dry_run_flag) {
            if (verbose_flag) {
                printf("[DRY-RUN] Création du répertoire : %s non réalisée\n", backup_dir);
            }
        } else {
            if (create_directory_local(backup_dir) != 0) {
                perror("Erreur création backup_dir");
                return;
            }
            if (verbose_flag) {
                printf("[INFO] Répertoire créé : %s\n", backup_dir);
            }
        }
    }

    char timestamp[128];
    get_timestamp_local(timestamp, sizeof(timestamp));
    char new_backup_path[2048];
    snprintf(new_backup_path, sizeof(new_backup_path), "%s/%s", backup_dir, timestamp);

    if (dry_run_flag) {
        if (verbose_flag) {
            printf("[DRY-RUN] Création du répertoire de sauvegarde : %s non réalisée\n", new_backup_path);
        }
    } else {
        if (create_directory_local(new_backup_path) != 0) {
            perror("Erreur new_backup_path");
            return;
        }
        if (verbose_flag) {
            printf("[INFO] Nouveau répertoire de sauvegarde créé : %s\n", new_backup_path);
        }
    }

    log_t old_logs = {0};
    if (!first_backup) {
        old_logs = read_backup_log(backup_log_path);
    }

    if (verbose_flag) {
        printf("[INFO] Traitement des fichiers de la source : %s\n", source_dir);
    }

    char last_backup_dir[2048] = {0};
    if (!first_backup) {
        find_last_backup_local(backup_dir, last_backup_dir, sizeof(last_backup_dir));
    }

    // Duplication de la dernière sauvegarde par liens durs
    if (!first_backup && last_backup_dir[0] != '\0') {
        typedef struct {
            char path[2048];
        } dir_stack_entry;
        dir_stack_entry stack[1000];
        int top = 0;
        strncpy(stack[top++].path, last_backup_dir, sizeof(stack[0].path) - 1);

        while (top > 0) {
            dir_stack_entry current = stack[--top];
            DIR *dir = opendir(current.path);
            if (!dir) {
                continue;
            }
            struct dirent *entry;

            while ((entry = readdir(dir)) != NULL) {
                if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                    continue;
                }
                char src_path[2048], dst_path[2048];
                snprintf(src_path, sizeof(src_path), "%s/%s", current.path, entry->d_name);
                size_t lblen = strlen(last_backup_dir);
                const char *rel = src_path + lblen;
                while (*rel == '/') {
                    rel++;
                }
                snprintf(dst_path, sizeof(dst_path), "%s/%s", new_backup_path, rel);
                struct stat st;

                if (stat(src_path, &st) == 0) {
                    if (S_ISDIR(st.st_mode)) {
                        if (dry_run_flag) {
                            if (verbose_flag) {
                                printf("[DRY-RUN] Création du répertoire : %s non réalisée\n", dst_path);
                            }
                        } else {
                            create_directory_local(dst_path);
                            if (verbose_flag) {
                                printf("[INFO] Répertoire créé : %s\n", dst_path);
                            }
                        }
                        strncpy(stack[top++].path, src_path, sizeof(stack[0].path) - 1);

                    } else if (S_ISREG(st.st_mode)) {
                        // Sauvegarde du fichier par lien ou copie
                        if (dry_run_flag) {
                            if (verbose_flag) {
                                printf("[DRY-RUN] Sauvegarde du fichier %s vers %s non réalisée\n", src_path, dst_path);
                            }
                        } else {
                            if (link(src_path, dst_path) != 0) {
                                copy_file_if_needed(src_path, dst_path);
                            } else if (verbose_flag) {
                                printf("[INFO] Fichier lié de %s vers %s\n", src_path, dst_path);
                            }
                        }
                    }
                }
            }
            closedir(dir);
        }
        if (verbose_flag) {
            printf("[INFO] Fin de la sauvegarde de base.\n");
        }
    }

    // Parcourt la source et met à jour incrémentalement
    log_t new_logs = {0};
    new_logs.head = NULL;
    new_logs.tail = NULL;

    typedef struct {
        char path[2048];
    } dir_stack_entry2;
    dir_stack_entry2 src_stack[1000];
    int src_top = 0;
    strncpy(src_stack[src_top++].path, source_dir, sizeof(src_stack[0].path) - 1);
    size_t source_dir_len = strlen(source_dir);

    while (src_top > 0) {
        dir_stack_entry2 current = src_stack[--src_top];
        DIR *dir = opendir(current.path);
        if (!dir) {
            continue;
        }
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            char filepath[2048];
            snprintf(filepath, sizeof(filepath), "%s/%s", current.path, entry->d_name);
            struct stat st;
            if (stat(filepath, &st) == 0) {
                const char *rel_path = filepath + source_dir_len;
                while (*rel_path == '/') {
                    rel_path++;
                }
                char dst_path[2048];
                snprintf(dst_path, sizeof(dst_path), "%s/%s", new_backup_path, rel_path);

                if (S_ISDIR(st.st_mode)) {
                    if (!file_exists_local(dst_path)) {
                        if (dry_run_flag) {
                            if (verbose_flag) {
                                printf("[DRY-RUN] Création du répertoire %s non réalisée\n", dst_path);
                            }
                        } else {
                            create_directory_local(dst_path);
                            if (verbose_flag) {
                                printf("[INFO] Répertoire créé : %s\n", dst_path);
                            }
                        }
                    }

                    if (!file_exists_local(dst_path)) {
                        if (dry_run_flag && verbose_flag) {
                            printf("[DRY-RUN] Création du répertoire %s non réalisée\n", dst_path);
                        } else if (!dry_run_flag) {
                            create_directory_local(dst_path);
                        }
                    }
                    strncpy(src_stack[src_top++].path, filepath, sizeof(src_stack[0].path) - 1);

                } else if (S_ISREG(st.st_mode)) {
                    // Calcul MD5
                    unsigned char md5_sum[MD5_DIGEST_LENGTH];
                    {
                        FILE *fcheck = fopen(filepath, "rb");
                        if (fcheck) {
                            unsigned char buffer[4096];
                            MD5_CTX ctx;
                            MD5_Init(&ctx);
                            size_t r;
                            while ((r = fread(buffer, 1, sizeof(buffer), fcheck)) > 0) {
                                MD5_Update(&ctx, buffer, r);
                            }
                            MD5_Final(md5_sum, &ctx);
                            fclose(fcheck);
                        } else {
                            memset(md5_sum, 0, MD5_DIGEST_LENGTH);
                        }
                    }

                    // Chercher old_elt
                    log_element *old_elt = NULL;
                    if (!first_backup && old_logs.head) {
                        for (log_element *e = old_logs.head; e; e = e->next) {
                            const char *sep = strchr(e->path, '/');
                            if (sep) {
                                const char *old_rel = sep + 1;
                                if (strcmp(old_rel, rel_path) == 0) {
                                    old_elt = e;
                                    break;
                                }
                            }
                        }
                    }

                    char dedup_filename[2048];
                    snprintf(dedup_filename, sizeof(dedup_filename), "%s/%s.dedup", new_backup_path, rel_path);
                    int file_unchanged = 0;
                    if (!first_backup && old_elt && memcmp(old_elt->md5, md5_sum, MD5_DIGEST_LENGTH) == 0) {
                        if (file_exists_local(dedup_filename)) {
                            file_unchanged = 1;
                        }
                    }

                    if (!file_unchanged) {
                        // Redédupliquer
                        FILE *f = fopen(filepath, "rb");
                        if (f) {
                            Chunk chunks[MAX_CHUNKS];
                            memset(chunks, 0, sizeof(chunks));
                            Md5Entry hash_table[HASH_TABLE_SIZE];
                            memset(hash_table, 0, sizeof(hash_table));
                            deduplicate_file(f, chunks, hash_table);
                            fclose(f);
                            int chunk_count = 0;
                            for (int i = 0; i < MAX_CHUNKS; i++) {
                                if (!chunks[i].data) {
                                    break;
                                }
                                chunk_count++;
                            }

                            {
                                char tmp[2048];
                                strncpy(tmp, dedup_filename, sizeof(tmp));
                                for (char *p = tmp + strlen(new_backup_path) + 1; *p; p++) {
                                    if (*p == '/') {
                                        *p = '\0';
                                        if (dry_run_flag) {
                                            if (verbose_flag) {
                                                printf("[DRY-RUN] Création du répertoire %s (non réalisée)\n", tmp);
                                            }
                                        } else {
                                            create_directory_local(tmp);
                                        }
                                        *p = '/';
                                    }
                                }
                            }

                            write_backup_file(dedup_filename, chunks, chunk_count);
                            for (int i = 0; i < chunk_count; i++) {
                                free(chunks[i].data);
                            }
                        }
                    }

                    // Ajout au new_logs
                    log_element *elt = (log_element *)malloc(sizeof(log_element));
                    memset(elt, 0, sizeof(log_element));
                    char log_path[2048];
                    snprintf(log_path, sizeof(log_path), "%s/%s", timestamp, rel_path);
                    elt->path = strdup(log_path);
                    struct tm *tm_info = localtime(&st.st_mtime);
                    char mod_time[128];
                    snprintf(
                        mod_time, sizeof(mod_time), "%04d-%02d-%02d-%02d:%02d:%02d.%03d",
                        tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
                        tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec, 0
                    );
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
        }
        closedir(dir);
    }

    // Supprime ce qui n'existe plus
    if (!first_backup) {
        for (log_element *e = old_logs.head; e; e = e->next) {
            const char *sep = strchr(e->path, '/');
            if (!sep) {
                continue;
            }
            const char *old_rel = sep + 1;
            char src_path[2048];
            snprintf(src_path, sizeof(src_path), "%s/%s", source_dir, old_rel);
            if (!file_exists_local(src_path)) {
                char dst_path[2048];
                snprintf(dst_path, sizeof(dst_path), "%s/%s", new_backup_path, old_rel);
                struct stat st;
                if (stat(dst_path, &st) == 0) {
                    typedef struct {
                        char path[2048];
                    } rm_entry;
                    rm_entry rm_stack[1000];
                    int rm_top = 0;
                    strncpy(rm_stack[rm_top++].path, dst_path, sizeof(rm_stack[0].path) - 1);
                    while (rm_top > 0) {
                        rm_entry cur = rm_stack[--rm_top];
                        DIR *dd = opendir(cur.path);
                        if (!dd) {
                            // rmdir ou unlink
                            if (dry_run_flag) {
                                if (verbose_flag) {
                                    printf("[DRY-RUN] Suppression du répertoire %s non réalisée\n", cur.path);
                                }
                            } else {
                                rmdir(cur.path);
                            }
                            continue;
                        }
                        struct dirent *en;
                        int empty = 1;
                        while ((en = readdir(dd)) != NULL) {
                            if (strcmp(en->d_name, ".") == 0 || strcmp(en->d_name, "..") == 0) {
                                continue;
                            }
                            empty = 0;
                            char fpath[2048];
                            snprintf(fpath, sizeof(fpath), "%s/%s", cur.path, en->d_name);
                            struct stat sst;
                            if (stat(fpath, &sst) == 0) {
                                if (S_ISDIR(sst.st_mode)) {
                                    strncpy(rm_stack[rm_top++].path, fpath, sizeof(rm_stack[0].path) - 1);
                                } else {
                                    // unlink
                                    if (dry_run_flag) {
                                        if (verbose_flag) {
                                            printf("[DRY-RUN] Suppression du fichier %s non réalisée\n", fpath);
                                        }
                                    } else {
                                        unlink(fpath);
                                    }
                                }
                            }
                        }
                        closedir(dd);
                        if (empty) {
                            if (dry_run_flag) {
                                if (verbose_flag) {
                                    printf("[DRY-RUN] Suppression du répertoire vide %s non réalisée\n", cur.path);
                                }
                            } else {
                                rmdir(cur.path);
                            }
                        } else {
                            rm_stack[rm_top++] = cur;
                        }
                    }
                }
            }
        }
    }

    // Met à jour .backup_log
    update_backup_log_if_needed(backup_log_path, &new_logs);

    {
        char new_backup_log_path[2048];
        snprintf(new_backup_log_path, sizeof(new_backup_log_path), "%s/.backup_log", new_backup_path);
        if (dry_run_flag) {
            if (verbose_flag) {
                printf("[DRY-RUN] Copie du .backup_log vers %s non réalisée\n", new_backup_log_path);
            }
        } else {
            copy_file(backup_log_path, new_backup_log_path);
            if (verbose_flag) {
                printf("[INFO] .backup_log copié vers %s\n", new_backup_log_path);
            }
        }
    }

    for (log_element *x = new_logs.head; x;) {
        log_element *nx = x->next;
        free((char *)x->path);
        free(x->date);
        free(x);
        x = nx;
    }
}

/**
 * @brief Déduplique un fichier.
 */
void backup_file(const char *filename)
{
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("Err backup_file fopen");
        return;
    }

    Chunk chunks[MAX_CHUNKS];
    memset(chunks, 0, sizeof(chunks));
    Md5Entry hash_table[HASH_TABLE_SIZE];
    memset(hash_table, 0, sizeof(hash_table));
    deduplicate_file(file, chunks, hash_table);
    fclose(file);

    int chunk_count = 0;
    for (int i = 0; i < MAX_CHUNKS; i++) {
        if (!chunks[i].data) {
            break;
        }
        chunk_count++;
    }

    char output_filename[MAX_SIZE_PATH];
    snprintf(output_filename, sizeof(output_filename), "%s.dedup", filename);
    write_backup_file(output_filename, chunks, chunk_count);
    for (int i = 0; i < chunk_count; i++) {
        free(chunks[i].data);
    }
}

/**
 * @brief Restaure une sauvegarde.
 */
void restore_backup(const char *backup_id, const char *restore_dir)
{
    char backup_dir[MAX_SIZE_PATH];
    strcpy(backup_dir, backup_id);
    char *last_slash = strrchr(backup_dir, '/');
    if (last_slash) {
        *last_slash = '\0';
    }
    char backup_log_path[MAX_SIZE_PATH];
    snprintf(backup_log_path, sizeof(backup_log_path), "%s/.backup_log", backup_dir);
    if (!file_exists_local(backup_log_path)) {
        return;
    }

    log_t logs = read_backup_log(backup_log_path);
    if (dry_run_flag) {
        if (verbose_flag) {
            printf("[DRY-RUN] Création du répertoire de restauration %s non réalisée\n", restore_dir);
        }
    } else {
        create_directory_local(restore_dir);
    }

    for (log_element *elt = logs.head; elt; elt = elt->next) {
        const char *sep = strchr(elt->path, '/');
        if (!sep) {
            continue;
        }
        const char *rel_path = sep + 1;
        char dedup_file[MAX_SIZE_PATH];
        snprintf(dedup_file, sizeof(dedup_file), "%s/%s.dedup", backup_id, rel_path);
        if (!file_exists_local(dedup_file)) {
            continue;
        }
        FILE *fin = fopen(dedup_file, "rb");
        if (!fin) {
            continue;
        }
        Chunk *chunks = NULL;
        int chunk_count = 0;
        undeduplicate_file(fin, &chunks, &chunk_count);
        fclose(fin);

        char restored_file[MAX_SIZE_PATH];
        snprintf(restored_file, sizeof(restored_file), "%s/%s", restore_dir, rel_path);

        {
            char temp[2048];
            strncpy(temp, restored_file, sizeof(temp));
            char *p = strrchr(temp, '/');
            if (p) {
                *p = '\0';
                if (dry_run_flag) {
                    if (verbose_flag) {
                        printf("[DRY-RUN] Création du répertoire %s pour restauration non réalisée\n", temp);
                    }
                } else {
                    create_directory_local(temp);
                }
            }
        }

        write_restored_file(restored_file, chunks, chunk_count);

        for (int i = 0; i < chunk_count; i++) {
            free(chunks[i].data);
        }
        free(chunks);
    }
}

/**
 * @brief Liste les sauvegardes.
 */
void list_backups(const char *backup_dir)
{
    DIR *dir = opendir(backup_dir);
    if (dir == NULL) {
        perror("Erreur pendant l'ouverture du dossier");
        return;
    }
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        if (strcmp(entry->d_name, ".backup_log") == 0) {
            continue;
        }
        char path[MAX_SIZE_PATH];
        snprintf(path, sizeof(path), "%s/%s", backup_dir, entry->d_name);
        struct stat st;
        if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
            printf("%s\n", entry->d_name);
        }
    }
    closedir(dir);
}
