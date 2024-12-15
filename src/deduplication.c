#include "deduplication.h"
#include "file_handler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/md5.h>
#include <dirent.h>

// Récupère les valeurs de verbose_flag et dry_run_flag
extern int verbose_flag;
extern int dry_run_flag;

// Fonction de hachage MD5 pour l'indexation
// dans la table de hachage
unsigned int hash_md5(unsigned char *md5) {
    unsigned int hash = 0;
    for (int i=0; i<MD5_DIGEST_LENGTH; i++) {
        hash = (hash << 5) + hash + md5[i];
    }
    return hash % HASH_TABLE_SIZE;
}

// Fonction pour calculer le MD5 d'un chunk
void compute_md5(void *data, size_t len, unsigned char *md5_out) {
    if (!data || !md5_out) {
        return; // vérifie que les pointeurs pointent une variable
    }
    MD5(data, len, md5_out);
    return;
}

// Fonction permettant de chercher un MD5 dans la table de hachage
int find_md5(Md5Entry *hash_table, unsigned char *md5) {
    /* @param: hash_table est le tableau de hachage qui contient les MD5 et l'index des chunks unique
    *           md5 est le md5 du chunk dont on veut déterminer l'unicité
    *  @return: retourne l'index s'il trouve le md5 dans le tableau et -1 sinon
    */
    Md5Entry *parcours = hash_table;

    int liste_non_finie = 1;
    while(liste_non_finie){ //tant qu'il a des valeurs dans le tableau de hachage
        if (parcours->index == 0) { // hash_table déclaré par calloc, \
                                    donc si index et md5 == 0 -> fin de la liste
            int md5_not_null = 0; // Variable mise à 1 si au moins un char de md5 != 0
            for (int i=0; i<16; ++i) {
                if (parcours->md5[i] != 0) {
                    md5_not_null = 1;
                }
            }
            if (!md5_not_null) {
                liste_non_finie = 0;
            }
        }

        if (liste_non_finie) { // On vérifie d'abord qu'on puisse acceder à parcours->md5
            if ((hash_md5(&(parcours->md5))) == hash_md5(md5)) {
                if (strcmp(&(parcours->md5), md5) == 0) {
                    return parcours->index; //on retourne son index
                }
            }
            ++parcours; // aller à l'adresse suivante
        }
    }
    return -1;
}

// Ajouter un MD5 dans la table de hachage
void add_md5(Md5Entry *hash_table, unsigned char *md5, int index) {
    Md5Entry *parcours = hash_table;

    int liste_non_finie = 1;
    while(liste_non_finie) { // on cherche à trouver la première case libre de la liste
        if (parcours->index == 0) {
            int md5_not_null = 0; // Variable mise à 1 si au moins un char de md5 != 0
            for (int i=0; i<16; ++i) {
                if (parcours->md5[i] != 0) {
                    md5_not_null = 1;
                }
            }
            if (!md5_not_null) {
                liste_non_finie = 0;
            }
        }
        if (liste_non_finie){
            ++parcours; // aller à l'adresse suivante
        }
    }
    memcpy(&(parcours->md5), md5, MD5_DIGEST_LENGTH);
    parcours->index = index;
}

// Fonction pour convertir un fichier non dédupliqué en tableau de chunks
void deduplicate_file(FILE *file, Chunk *chunks, Md5Entry *hash_table) {
    /* @param:  file est le fichier qui sera dédupliqué
    *           chunks est le tableau de chunks initialisés qui contiendra les chunks issu du fichier
    *           hash_table est le tableau de hachage qui contient les MD5 et l'index des chunks unique
    */

    // tampon de la taille d'un chunk (4096 octets = 4096 unsigned char)
    unsigned char buffer[CHUNK_SIZE];
    unsigned int index = 0;
    Chunk *parcours_chunk = chunks;
    while (!feof(file)) {
        // Lecture d'un chunk
        size_t taille_bloc = fread(buffer, 1, CHUNK_SIZE, file);
        if (taille_bloc != CHUNK_SIZE) {
            if (ferror(file)) {
                perror("Erreur dans la lecture du fichier");
                return;
            }
        }
        unsigned char *md5 = malloc(16);
        compute_md5(buffer, taille_bloc, md5);
        int md5_index = find_md5(hash_table, md5);
        if (md5_index != -1) {
            parcours_chunk->data = malloc(sizeof(unsigned int));
            memcpy(parcours_chunk->data, &md5_index, sizeof(int));
            memcpy(&(parcours_chunk->md5), md5, MD5_DIGEST_LENGTH);
        } else {
            add_md5(hash_table, md5, index);
            memcpy(&(parcours_chunk->md5), md5, MD5_DIGEST_LENGTH);
            parcours_chunk->data = malloc(CHUNK_SIZE);
            memcpy(parcours_chunk->data, buffer, CHUNK_SIZE);
        }
        ++parcours_chunk;
        ++index;
    }  

}


// Fonction permettant de charger un fichier dédupliqué en table de chunks
// en remplaçant les références par les données correspondantes
void undeduplicate_file(FILE *file, Chunk **chunks, int *chunk_count) {
    /* @param: file est le nom du fichier dédupliqué présent dans le répertoire de sauvegarde
    *           chunks représente le tableau de chunk qui contiendra les chunks restauré depuis filename
    *           chunk_count est un compteur du nombre de chunk restauré depuis le fichier filename
    */
    unsigned char buffer[CHUNK_SIZE];
    fread(chunk_count, 1, sizeof(int), file);
    *chunks = calloc(CHUNK_SIZE, sizeof(Chunk));
    Chunk *parcours_chunk = *chunks;
    unsigned int chunk_size_on_file;
    unsigned char chunk_md5[MD5_DIGEST_LENGTH];
    unsigned char chunk_md5_computed[MD5_DIGEST_LENGTH];
    unsigned char *chunk_data;
    for (int i=0; i<chunk_count; ++i) {
        fread(chunk_md5, 1, MD5_DIGEST_LENGTH, file); // lecture du md5
        memcpy(&(parcours_chunk->md5),chunk_md5, MD5_DIGEST_LENGTH);
        fread(chunk_size_on_file, 1, sizeof(unsigned int), file);
        if (chunk_size_on_file != CHUNK_SIZE && chunk_size_on_file == sizeof(unsigned int)) { //savoir si le chunk est un index
            chunk_data = malloc(sizeof(unsigned int));
            fread(chunk_data, 1, sizeof(unsigned int), file);
            compute_md5(chunk_data, sizeof(unsigned int),chunk_md5_computed);
            if (hash_md5(chunk_md5) == hash_md5(chunk_md5_computed)) { // comparaison rapide si différents
                if (strcmp(chunk_md5, chunk_md5_computed) == 0) { // chunks fait une longueur de 4 octets
                    memcpy(parcours_chunk->data, chunk_data, sizeof(unsigned int));
                } else {
                    memcpy(parcours_chunk->data, (*chunks + *chunk_data)->data, sizeof CHUNK_SIZE);
                } 
            }
            free(chunk_data);
        } else {
            if (chunk_size_on_file != CHUNK_SIZE) { //Cas ou on est dans le dernier chunk incomplet (pas à 4096o)
                chunk_data = malloc(chunk_size_on_file);
                fread(chunk_data, 1, chunk_size_on_file, file);
                memcpy(parcours_chunk->data, chunk_data, chunk_size_on_file);
                free(chunk_data);
            } else { // Cas général : on se situe dans  un chunk de 4096 octets
                chunk_data = malloc(CHUNK_SIZE);
                fread(chunk_data, 1, CHUNK_SIZE, file);
                memcpy(parcours_chunk->data, chunk_data, CHUNK_SIZE);
                free(chunk_data);
            }
        }
    }
}
