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

// Récupère les valeurs de verbose_flag et dry_run_flag
extern int verbose_flag;
extern int dry_run_flag;

/**
 * @brief Teste l'existence d'un fichier ou répertoire.
 */
static int file_exists_local(const char *path) {
    struct stat st;
    return (stat(path, &st) == 0);
}

/**
 * @brief Crée un répertoire si non existant.
 */
static int create_directory_local(const char *path) {
    if (mkdir(path, 0755) == -1 && errno != EEXIST) {
        return -1;
    }
    return 0;
}

/**
 * @brief Génère un horodatage sous forme YYYY-MM-DD-hh:mm:ss.sss.
 */
static void get_timestamp_local(char *buffer, size_t size) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm *tm_info = localtime(&tv.tv_sec);
    snprintf(buffer, size, "%04d-%02d-%02d-%02d:%02d:%02d.%03ld",
             tm_info->tm_year+1900, tm_info->tm_mon+1, tm_info->tm_mday,
             tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec, tv.tv_usec/1000);
}

/**
 * @brief Trouve le dernier répertoire de sauvegarde (le plus récent).
 */
static void find_last_backup_local(const char *backup_dir, char *last_backup_dir, size_t size) {
    DIR *dir = opendir(backup_dir);
    if (!dir) {
        last_backup_dir[0] = '\0';
        return;
    }
    struct dirent *entry;
    char latest[2048] = {0};
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name,".")==0 || strcmp(entry->d_name,"..")==0) continue;
        if (strcmp(entry->d_name,".backup_log")==0) continue;
        char path[2048];
        snprintf(path,sizeof(path),"%s/%s",backup_dir,entry->d_name);
        struct stat st;
        if (stat(path,&st)==0 && S_ISDIR(st.st_mode)) {
            if (latest[0]=='\0') {
                strncpy(latest,entry->d_name,sizeof(latest)-1);
            } else {
                if (strcmp(entry->d_name,latest)>0) {
                    strncpy(latest,entry->d_name,sizeof(latest)-1);
                }
            }
        }
    }
    closedir(dir);
    if (latest[0] != '\0') {
        snprintf(last_backup_dir,size,"%s/%s",backup_dir,latest);
    } else {
        last_backup_dir[0]='\0';
    }
}

/**
 * @brief Crée une nouvelle sauvegarde incrémentale.
 * @see backup_manager.h pour la description détaillée.
 */
void create_backup(const char *source_dir, const char *backup_dir) {
    // Vérifie si première sauvegarde
    char backup_log_path[1024];
    snprintf(backup_log_path,sizeof(backup_log_path),"%s/.backup_log",backup_dir);
    int first_backup = !file_exists_local(backup_log_path);

    // Crée backup_dir si besoin
    if (!file_exists_local(backup_dir)) {
        if (create_directory_local(backup_dir)!=0) {
            perror("Erreur création backup_dir");
            return;
        }
    }

    // Timestamp et création nouvelle sauvegarde
    char timestamp[128];
    get_timestamp_local(timestamp,sizeof(timestamp));
    char new_backup_path[2048];
    snprintf(new_backup_path,sizeof(new_backup_path),"%s/%s",backup_dir,timestamp);
    if (create_directory_local(new_backup_path)!=0) {
        perror("Erreur new_backup_path");
        return;
    }

    // Lit ancien log si pas première sauvegarde
    log_t old_logs={0};
    if (!first_backup) {
        old_logs = read_backup_log(backup_log_path);
    }

    // Trouve last_backup si pas première
    char last_backup_dir[2048]={0};
    if (!first_backup) {
        find_last_backup_local(backup_dir,last_backup_dir,sizeof(last_backup_dir));
    }

    // Duplique dernière sauvegarde si pas première (liens durs)
    if (!first_backup && last_backup_dir[0]!='\0') {
        typedef struct {char path[2048];} dir_stack_entry;
        dir_stack_entry stack[1000];
        int top=0;
        strncpy(stack[top++].path,last_backup_dir,sizeof(stack[0].path)-1);
        while(top>0) {
            dir_stack_entry current = stack[--top];
            DIR *dir = opendir(current.path);
            if (!dir) continue;
            struct dirent *entry;
            while((entry=readdir(dir))!=NULL) {
                if (strcmp(entry->d_name,".")==0||strcmp(entry->d_name,"..")==0) continue;
                char src_path[2048],dst_path[2048];
                snprintf(src_path,sizeof(src_path),"%s/%s",current.path,entry->d_name);
                size_t lblen=strlen(last_backup_dir);
                const char *rel=src_path+lblen;while(*rel=='/')rel++;
                snprintf(dst_path,sizeof(dst_path),"%s/%s",new_backup_path,rel);
                struct stat st;
                if (stat(src_path,&st)==0) {
                    if (S_ISDIR(st.st_mode)) {
                        create_directory_local(dst_path);
                        strncpy(stack[top++].path,src_path,sizeof(stack[0].path)-1);
                    } else if (S_ISREG(st.st_mode)) {
                        if (link(src_path,dst_path)!=0) {
                            copy_file(src_path,dst_path);
                        }
                    }
                }
            }
            closedir(dir);
        }
    }

    // Parcourt la source et met à jour incrémentalement
    log_t new_logs={0};new_logs.head=NULL;new_logs.tail=NULL;
    typedef struct {char path[2048];} dir_stack_entry2;
    dir_stack_entry2 src_stack[1000];
    int src_top=0;
    strncpy(src_stack[src_top++].path,source_dir,sizeof(src_stack[0].path)-1);
    size_t source_dir_len=strlen(source_dir);

    while(src_top>0) {
        dir_stack_entry2 current=src_stack[--src_top];
        DIR *dir=opendir(current.path);
        if(!dir)continue;
        struct dirent *entry;
        while((entry=readdir(dir))!=NULL) {
            if(strcmp(entry->d_name,".")==0||strcmp(entry->d_name,"..")==0)continue;
            char filepath[2048];
            snprintf(filepath,sizeof(filepath),"%s/%s",current.path,entry->d_name);
            struct stat st;
            if(stat(filepath,&st)==0) {
                const char *rel_path=filepath+source_dir_len;while(*rel_path=='/')rel_path++;
                char dst_path[2048];snprintf(dst_path,sizeof(dst_path),"%s/%s",new_backup_path,rel_path);

                if(S_ISDIR(st.st_mode)) {
                    // Crée répertoire si inexistant
                    if(!file_exists_local(dst_path)) create_directory_local(dst_path);
                    // Empile pour exploration
                    strncpy(src_stack[src_top++].path,filepath,sizeof(src_stack[0].path)-1);
                } else if(S_ISREG(st.st_mode)) {
                    // Calcul MD5
                    unsigned char md5_sum[MD5_DIGEST_LENGTH];
                    {
                        FILE *fcheck=fopen(filepath,"rb");
                        if(fcheck) {
                            unsigned char buffer[4096];
                            MD5_CTX ctx;MD5_Init(&ctx);
                            size_t r;
                            while((r=fread(buffer,1,sizeof(buffer),fcheck))>0) {
                                MD5_Update(&ctx,buffer,r);
                            }
                            MD5_Final(md5_sum,&ctx);
                            fclose(fcheck);
                        } else memset(md5_sum,0,MD5_DIGEST_LENGTH);
                    }

                    // Chercher old_elt
                    log_element *old_elt=NULL;
                    if(!first_backup && old_logs.head) {
                        for(log_element*e=old_logs.head;e;e=e->next) {
                            const char *sep=strchr(e->path,'/');
                            if(sep) {
                                const char *old_rel=sep+1;
                                if(strcmp(old_rel,rel_path)==0) {old_elt=e;break;}
                            }
                        }
                    }

                    char dedup_filename[2048];
                    snprintf(dedup_filename,sizeof(dedup_filename),"%s/%s.dedup",new_backup_path,rel_path);
                    int file_unchanged=0;
                    if(!first_backup && old_elt && memcmp(old_elt->md5,md5_sum,MD5_DIGEST_LENGTH)==0) {
                        if(file_exists_local(dedup_filename)) {
                            file_unchanged=1;
                        }
                    }

                    if(!file_unchanged) {
                        // Redédupliquer
                        FILE *f=fopen(filepath,"rb");
                        if(f) {
                            Chunk chunks[MAX_CHUNKS];memset(chunks,0,sizeof(chunks));
                            Md5Entry hash_table[HASH_TABLE_SIZE];memset(hash_table,0,sizeof(hash_table));
                            deduplicate_file(f,chunks,hash_table);
                            fclose(f);
                            int chunk_count=0;for(int i=0;i<MAX_CHUNKS;i++){if(!chunks[i].data)break;chunk_count++;}
                            // Création répertoires parents si besoin
                            {
                                char tmp[2048];strncpy(tmp,dedup_filename,sizeof(tmp));
                                for(char*p=tmp+strlen(new_backup_path)+1;*p;p++){
                                    if(*p=='/'){*p='\0';create_directory_local(tmp);*p='/';}
                                }
                            }
                            write_backup_file(dedup_filename,chunks,chunk_count);
                            for(int i=0;i<chunk_count;i++){free(chunks[i].data);}
                        }
                    }

                    // Ajout au new_logs
                    log_element *elt=(log_element*)malloc(sizeof(log_element));
                    memset(elt,0,sizeof(log_element));
                    char log_path[2048];snprintf(log_path,sizeof(log_path),"%s/%s",timestamp,rel_path);
                    elt->path=strdup(log_path);
                    struct tm *tm_info=localtime(&st.st_mtime);
                    char mod_time[128];
                    snprintf(mod_time,sizeof(mod_time),"%04d-%02d-%02d-%02d:%02d:%02d.%03d",
                            tm_info->tm_year+1900, tm_info->tm_mon+1, tm_info->tm_mday,
                            tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec, 0);
                    elt->date=strdup(mod_time);
                    memcpy(elt->md5,md5_sum,MD5_DIGEST_LENGTH);
                    elt->next=NULL;elt->prev=new_logs.tail;
                    if(new_logs.tail)new_logs.tail->next=elt;else new_logs.head=elt;
                    new_logs.tail=elt;
                }
            }
        }
        closedir(dir);
    }

    // Supprime ce qui n'existe plus
    if(!first_backup) {
        for(log_element *e=old_logs.head;e;e=e->next) {
            const char*sep=strchr(e->path,'/');
            if(!sep)continue;
            const char*old_rel=sep+1;
            char src_path[2048];snprintf(src_path,sizeof(src_path),"%s/%s",source_dir,old_rel);
            if(!file_exists_local(src_path)) {
                char dst_path[2048];snprintf(dst_path,sizeof(dst_path),"%s/%s",new_backup_path,old_rel);
                struct stat st;
                if(stat(dst_path,&st)==0) {
                    // Suppression récursive répertoire ou fichier
                    typedef struct{char path[2048];}rm_entry;
                    rm_entry rm_stack[1000];int rm_top=0;
                    strncpy(rm_stack[rm_top++].path,dst_path,sizeof(rm_stack[0].path)-1);
                    while(rm_top>0) {
                        rm_entry cur=rm_stack[--rm_top];
                        DIR*dd=opendir(cur.path);
                        if(!dd){rmdir(cur.path);continue;}
                        struct dirent*en;int empty=1;
                        while((en=readdir(dd))!=NULL) {
                            if(strcmp(en->d_name,".")==0||strcmp(en->d_name,"..")==0)continue;
                            empty=0;
                            char fpath[2048];snprintf(fpath,sizeof(fpath),"%s/%s",cur.path,en->d_name);
                            struct stat sst;
                            if(stat(fpath,&sst)==0) {
                                if(S_ISDIR(sst.st_mode)) {
                                    strncpy(rm_stack[rm_top++].path,fpath,sizeof(rm_stack[0].path)-1);
                                } else {
                                    unlink(fpath);
                                }
                            }
                        }
                        closedir(dd);
                        if(empty) {rmdir(cur.path);} else {rm_stack[rm_top++]=cur;}
                    }
                }
            }
        }
    }

    // Met à jour .backup_log
    update_backup_log(backup_log_path,&new_logs);
    // Copie .backup_log dans la nouvelle sauvegarde
    {
        char new_backup_log_path[2048];
        snprintf(new_backup_log_path,sizeof(new_backup_log_path),"%s/.backup_log",new_backup_path);
        copy_file(backup_log_path,new_backup_log_path);
    }

    // Libère new_logs
    for(log_element*x=new_logs.head;x;) {
        log_element*nx=x->next;
        free((char*)x->path);free(x->date);free(x);
        x=nx;
    }
}

// Fonction permettant d'enregistrer dans fichier le tableau de chunk dédupliqué
void write_backup_file(const char *output_filename, Chunk *chunks, int chunk_count) {
    FILE *file = fopen(output_filename, "wb");
    if (file == NULL){
        perror("Erreur d'ouverture du fichier");
        return;
    }
    fwrite(&chunk_count, sizeof(int), 1, file);
    unsigned int chunk_size;
    for (int i=0; i<chunk_count; i++) {
        chunk_size = fwrite(chunks[i].md5, MD5_DIGEST_LENGTH, 1, file);
        fwrite(chunk_size, sizeof(unsigned int), 1, file);
        fwrite(chunks[i].data, 1, chunk_size, file);
    }
    fclose(file);
}

/**
 * @brief Déduplique un fichier.
 * @see backup_manager.h
 */
void backup_file(const char *filename) {
    FILE *file=fopen(filename,"rb");
    if(!file){perror("Err backup_file fopen");return;}
    Chunk chunks[MAX_CHUNKS];memset(chunks,0,sizeof(chunks));
    Md5Entry hash_table[HASH_TABLE_SIZE];memset(hash_table,0,sizeof(hash_table));
    deduplicate_file(file,chunks,hash_table);
    fclose(file);

    int chunk_count=0;
    for(int i=0;i<MAX_CHUNKS;i++){
        if(!chunks[i].data)break;
        chunk_count++;
    }

    char output_filename[MAX_SIZE_PATH];
    snprintf(output_filename,sizeof(output_filename),"%s.dedup",filename);
    write_backup_file(output_filename,chunks,chunk_count);
    for(int i=0;i<chunk_count;i++){
        free(chunks[i].data);
    }
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
 * @brief Restaure une sauvegarde.
 * @see backup_manager.h
 */
void restore_backup(const char *backup_id, const char *restore_dir) {
    char backup_dir[MAX_SIZE_PATH];strcpy(backup_dir,backup_id);
    char *last_slash=strrchr(backup_dir,'/');
    if(last_slash)*last_slash='\0';
    char backup_log_path[MAX_SIZE_PATH];
    snprintf(backup_log_path,sizeof(backup_log_path),"%s/.backup_log",backup_dir);
    if(!file_exists_local(backup_log_path))return;
    log_t logs=read_backup_log(backup_log_path);
    create_directory_local(restore_dir);

    for(log_element*elt=logs.head;elt;elt=elt->next) {
        const char*sep=strchr(elt->path,'/');
        if(!sep)continue;
        const char*rel_path=sep+1;
        char dedup_file[MAX_SIZE_PATH];snprintf(dedup_file,sizeof(dedup_file),"%s/%s.dedup",backup_id,rel_path);
        if(!file_exists_local(dedup_file))continue;
        FILE*fin=fopen(dedup_file,"rb");
        if(!fin)continue;
        Chunk *chunks=NULL;int chunk_count=0;
        undeduplicate_file(fin,&chunks,&chunk_count);
        fclose(fin);

        char restored_file[MAX_SIZE_PATH];snprintf(restored_file,sizeof(restored_file),"%s/%s",restore_dir,rel_path);
        {
            // Crée répertoires parents si besoin
            char temp[2048];strncpy(temp,restored_file,sizeof(temp));
            char*p=strrchr(temp,'/');
            if(p){*p='\0';create_directory_local(temp);}
        }

        // Écrit le fichier restauré
        write_restored_file(restored_file,chunks,chunk_count);

        for(int i=0;i<chunk_count;i++){free(chunks[i].data);}
        free(chunks);
    }
}

/**
 * @brief Liste les sauvegardes.
 * @see backup_manager.h
 */
void list_backups(const char *backup_dir) {
    DIR *dir = opendir(backup_dir);
    if (dir == NULL){
        perror("Erreur pendant l'ouverture du dossier");
        return;       
    }
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name,".") ==0 || strcmp(entry->d_name,"..")==0) {
            continue;
        }
        if (strcmp(entry->d_name,".backup_log")==0) continue;
        char path[MAX_SIZE_PATH];
        snprintf(path, sizeof(path), "%s/%s", backup_dir, entry->d_name);
        struct stat st;
        if (stat(path,&st)==0 && S_ISDIR(st.st_mode)){
            printf("%s\n",entry->d_name);
        }
    }
    closedir(dir);
}
