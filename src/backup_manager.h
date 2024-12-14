#ifndef BACKUP_MANAGER_H
#define BACKUP_MANAGER_H

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

/**
 * @brief Crée une nouvelle sauvegarde incrémentale du répertoire source dans le répertoire de backup.
 *
 * Cette fonction :
 * - Vérifie si c'est la première sauvegarde ou non (via .backup_log).
 * - Crée un répertoire horodaté pour la nouvelle sauvegarde.
 * - Si ce n'est pas la première sauvegarde, duplique la dernière en créant des liens durs.
 * - Parcourt le répertoire source, déduplique ou lie les fichiers inchangés, crée les répertoires manquants.
 * - Supprime les fichiers/répertoires qui n'existent plus dans la source.
 * - Met à jour le fichier .backup_log et le copie dans la nouvelle sauvegarde.
 *
 * @param source_dir Chemin du répertoire source à sauvegarder.
 * @param backup_dir Chemin du répertoire de destination des sauvegardes.
 */
void create_backup(const char *source_dir, const char *backup_dir);

/**
 * @brief Restaure une sauvegarde depuis un répertoire de backup vers un répertoire destination.
 *
 * Cette fonction lit le .backup_log, parcourt chaque fichier, le reconstruit à partir du .dedup
 * et le place dans le répertoire de restauration. Elle remplace le fichier de destination uniquement
 * si la source est plus récente ou diffère par la taille, conformément aux règles du projet.
 *
 * @param backup_id Chemin vers le répertoire de la sauvegarde.
 * @param restore_dir Chemin vers le répertoire où restaurer les fichiers.
 */
void restore_backup(const char *backup_id, const char *restore_dir);

/**
 * @brief Écrit dans un fichier de backup dédupliqué le tableau de chunks.
 *
 * @param output_filename Nom du fichier de sortie (fichier .dedup).
 * @param chunks Tableau de chunks dédupliqués.
 * @param chunk_count Nombre de chunks.
 */
void write_backup_file(const char *output_filename, Chunk *chunks, int chunk_count);

/**
 * @brief Déduplique un fichier spécifique et écrit sa version .dedup.
 *
 * @param filename Chemin du fichier à dédupliquer.
 */
void backup_file(const char *filename);

/**
 * @brief Écrit un fichier restauré à partir d'un tableau de chunks.
 *
 * @param output_filename Nom du fichier restauré.
 * @param chunks Tableau de chunks.
 * @param chunk_count Nombre de chunks.
 */
void write_restored_file(const char *output_filename, Chunk *chunks, int chunk_count);

/**
 * @brief Liste toutes les sauvegardes existantes (répertoires horodatés) dans un répertoire de backup.
 *
 * @param backup_dir Chemin du répertoire contenant les sauvegardes.
 */
void list_backups(const char *backup_dir);

#endif // BACKUP_MANAGER_H
