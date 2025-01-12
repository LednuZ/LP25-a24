# Projet LP25 - A2024

## Contexte

Le projet consiste à construire une solution de sauvegarde incrémentale d'un répertoire source (le répertoire à sauvegarder) vers un répertoire cible (le répertoire de sauvegarde). Il s'agit de développer un programme en langage C pour créer un outil de sauvegarde inspiré de Borg Backup, avec un accent sur la déduplication des données et la possibilité de réaliser des sauvegardes sur un serveur distant via des sockets.

## Quelques explications

- **Sauvegarde Incrémentale** : Contrairement à la sauvegarde complète qui sauvegarde à chaque itération l'ensemble des données de départ, la sauvegarde incrémentale ne sauvegarde que ce qui a changé. Lorsqu'il faut sauvegarder un dossier contenant 20 fichiers alors qu'un seul fichier a été modifié parmi les 20, la sauvegarde complète va de nouveau sauvegarder l'ensemble du dossier alors que la sauvegarde incrémentale ne va sauvegarder que le fichier qui a changé.
- **Déduplication** : La déduplication est un principe de sauvegarde qui consiste à identifier et supprimer dans un ensemble de données et en ne sauvegardant chaque bloc unique (*chunk*) qu'une seule fois. Les blocs qui se répètent sont remplacés par des références pointant vers les blocs déjà sauvegardés.
- **Chunk** : Un chunk appelé "bloc" en français est un bloc de données représentant une portion d'information découpée d'un ensemble plus grand (*un fichier par exemple*). L'objectif des chunks est de faciliter les opérations de transfert, de stockage, etc. 
  - Exemple : Imaginons qu'un paquet de sucre représente un fichier, et que ce paquet de sucre contient 50 morceaux de sucres de 4 couleurs différentes (bleu, rouge, brun, blanc). Un morceau de sucre représente un **chunk**. Si je veux sauvegarder ce fichier sans déduplication, je devrai copier tous les 50 morceaux, même si plusieurs sont identiques. Mais avec la déduplication, je n'ai besoin de sauvegarder que les **morceaux distincts** (bleu, rouge, brun, blanc), leur **nombre respectif**, et leur **position** dans le paquet. L'intérêt est un gain d'espace. Par exemple, au lieu de sauvegarder 50 morceaux, je note simplement qu'il y a :
    -	*8 morceaux bruns aux positions : 1,5,6,8,15,17,18,19*
    -	*15 morceaux bleu aux positions : 3,4,etc.*

  
## Architecture

Le projet comprend quatres modules :

- **file_handler** : Gère les opérations de fichier telles que la lecture, l'écriture et la liste des fichiers dans un répertoire de même que les répertoires
- **deduplication** : Lors de la sauvegarde,implémente la lecture des fichiers en chunks, calcule leur MD5, et compare ces sommes pour identifier les bloc de données doublons
- **backup_manager** : Implémente la logique de gestion de sauvegarde incrémentale
- **network** : Implémente les fonctionnalités de communication réseau en permettant l'envoi de données à un serveur distant et la réception de données à partir d'un port spécifié. Les sockets TCP sont implémentés pour établir des connexions entre le client et le serveur

```bash
projet_lp25/
│
├── src/
│   ├── main.c
│   ├── file_handler.c
│   ├── file_handler.h
│   ├── deduplication.c
│   ├── deduplication.h
│   ├── backup_manager.c
│   ├── backup_manager.h
│   ├── network.c
│   └── network.h
├── Makefile
└── README.md

```

## Options du programme
Le programme dispose de plusieurs options :

- `--backup` : crée une nouvelle sauvegarde du répertoire source, localement ou sur le serveur distant. Ne s'utilise pas avec les options `--restore` et `--list-backups`
- `--restore` : restaure une sauvegarde à partir du chemin, localement ou depuis le serveur. Ne s'utilise pas avec les options `--backup` et `--list-backups`
- `--list-backups` : liste toutes les sauvegardes existantes, localement ou sur le serveur. Ne s'utilise pas avec les options `--restore` et `--backup`
- `--dry-run` : test une sauvegarde ou une restauration sans effectuer de réelles copies
- `--d-server` : spécifie l'adresse IP du serveur à utiliser comme destination
- `--d-port` : spécifie le port du serveur de destination
- `--s-server` : spécifie l'adresse IP du serveur à utiliser comme source
- `--s-port` : spécifie le port du serveur source
- `--dest` : spécifie le chemin de destination de la sauvegarde ou de la restauration
- `--source` : spécifie le chemin source de la sauvegarde ou de la restauration
- `--verbose` ou `v` : affiche plus d'informations sur l'exécution du programme


### L'option `--backup`

L'option `backup` va permettre de faire des sauvegarde incrémentale d'un répertoire que ce soit localement ou vers un serveur distant. Pour cela :

1. Le programme vérifie si le chemin de la sauvegarde spécifié existe et est accessible. Si le chemin est sur un serveur, il établit une connexion via les sockets.
2. Le programme commence par effectuer une copie complète de la dernière sauvegarde, par liens durs, avec la date et l'heure actuelles comme nom, sous le format `"YYYY-MM-DD-hh:mm:ss.sss"` où :
	- `YYYY` est l'année sur 4 chiffres
	- `MM` est le mois, entre 01 et 12
	- `DD` est le jour, entre 01 et 31
	- `hh` est l'heure, entre 00 et 23
	- `mm` sont les minutes, entre 00 et 59
	- `ss.sss` sont les secondes et les millisecondes, entre 00.000 et 59.999

	S'il n'existe pas de sauvegarde précédente (i.e. c'est la première sauvegarde), le programme crée simplement un répertoire avec ce nom et un fichier `.backup_log` à la racine du dossier `/path/to/destination`.

	Cette première étape a donc pour effet que :
	- la sauvegarde de la source `/path/to/source` dans `/path/to/destination` soit en réalité située dans le répertoire `/path/to/destination/YYYY-MM-DD-hh:mm:ss.sss` où les champs du dernier répertoire sont remplacés par la date et l'heure réelles.
	- le fichier `.backup_log` soit rempli avec les informations concernant les fichiers dédupliqué qui ont été sauvegardé de même que leur md5 ligne par ligne. Chaque ligne est structurée comme suit : `YYYY-MM-DD-hh:mm:ss.sss/folder1/file1;mtime;md5` où :
		- `YYYY-MM-DD-hh:mm:ss.sss` est le nom du répertoire de sauvegarde
 		- `mtime` est la date de dernière modification de ce fichier
 		- `md5` est la somme md5 du fichier dédupliqué

3. Pour les prochaines sauvegardes, le programme vérifie le contenu du fichier `.backup_log` en suivant les règles ci-dessous (pour chaque changement, le fichier `.backup_log` est mis à jour :

	- un dossier dans la source est créé quand il n'existe pas dans la destination
	- un dossier dans la destination est supprimé quand il n'existe pas dans la source
	- un fichier dans la source, mais pas dans la destination, est copié dans la destination
	- un fichier dans la source et dans la destination est copié si :
		- la date de modification est postérieure dans la source et le contenu est différent
		- la taille est différente et le contenu est différent
	- un fichier de la destination est supprimé s'il n'existe plus dans la source
	- à la fin de la sauvegarde, le fichier `.backup_log` mis à jour est copié dans le répertoire de la sauvegarde

### L'option `--restore`
L'option `--restore` permet de restaurer une sauvegarde à partir d'un chemin spécifié, que ce soit localement ou depuis un serveur distant. La restauration peut être effectuée en utilisant les informations sur la sauvegarde disponible dans le répertoire de destination ou à travers une connexion réseau.

1. Le programme vérifie si le chemin de la sauvegarde spécifié existe et est accessible. Si le chemin est sur un serveur, il établit une connexion via les sockets.
2. Le programme parcours le fichier `.backup_log` présent dans le répertoire de la sauvegarde
3. Sur la base des chemins présents dans le fichier, le programme copie les fichiers de la sauvegarde dans le répertoire de destination spécifié, ou dans le répertoire par défaut (répertoire courant de l'utilisateur) si aucune destination n'est fournie.
4. Si un fichier restauré existe déjà dans la destination, le programme effectue les vérifications suivantes avant de remplacer le fichier :
	- Si la date de modification du fichier source est postérieure à celle du fichier de destination, il est remplacé.
 	- Si la taille des fichiers diffère, le fichier de destination est également remplacé.
5. Le programme notifie l'utilisateur du succès ou des échecs de chaque opération de restauration. Si l'option `--verbose` est activée, il affiche des messages détaillés (durée de la restauration).

### L'option `--list-backups`
L'option `--list-backups` permet d'afficher toutes les sauvegardes existantes, que ce soit localement ou sur un serveur distant. Cette fonctionnalité est utile pour que l'utilisateur puisse voir toutes les sauvegardes disponibles et décider laquelle restaurer.

1. Le programme vérifie si l'option `--s-server` serveur a été fournie. Si oui, il établit une connexion avec le serveur spécifié pour récupérer la liste des sauvegardes.
2. Si aucune adresse de serveur n'est fournie, le programme listera toutes les sauvegardes disponibles dans le répertoire par défaut (ou spécifié par l'utilisateur).
3. Chaque sauvegarde est affichée avec des détails, tels que le nom de la sauvegarde, la date de création, et la taille.
4. Si l'option `--verbose` est activée, des informations supplémentaires peuvent être affichées, comme le chemin complet des fichiers de sauvegarde ou des informations sur la connexion réseau.


## Communication réseaux 
Pour les fonctions du réseau, il n'y a principalement que deux fonctions :
- une fonction pour envoyer les données à un autre instance de borg-backup
- une fonction pour recevoir les données d'une instance de borg-backup

Afin de faire les transfert en réseau, vous démarrez deux instances du programme. Les deux instances sont démarrées de la façon suivante : 
- l'instance en mode serveur :
	- Exemple : `./lp25_borg_backup --backup --s-sever ip_source --d-server 127.0.0.1 --d-port num_port --dest nom_dossier`.
 	- Lorsque l'option `--d-server` est `127.0.0.1`, alors il passe en écoute et utilise la fonction `receive_data` pour récupérer les données qui seront envoyées puis les sauvegarde dans le dossier précisé en option `--dest`
- l'instance en mode client :
	- Exemple : `./lp25_borg_backup --backup --s-sever 127.0.0.1 --d-server ip_serveur --d-port num_port --source nom_dossier`.
 	- Lorsque l'option `--s-server` est `127.0.0.1` alors, il passe en mode client et utilise la fonction `send_data` pour envoyer à `ip_serveur`, les données à sauvegarder depuis le dossier précisé en option `--source`
 
Les deux instances suivent les étapes suivantes :
- Pour le Backup :
	1. Le client envoie le texte "`BACKUP`" puis se met en écoute.
	2. le serveur envoie au client le fichier "`.backup_log`" de la racine du répertoire de destination puis se met en écoute.
	3. Lorsque le client reçoit le fichier `.backup_log`, il le stocke en mémoire et fais les comparaison (_date de modif, md5_)
	4. Après comparaison, toutes les données sont dédupliquées avant d'être envoyées au serveur pour sauvegarde
	5. Une fois toutes les données envoyées, il envoie le mot "`EXIT`" pour mettre fin à l'échange.
- Pour la Restoration :
	1. Le client envoie le texte "`RESTORE BACKUP_ID`" (où `BACKUP_ID` est le nom de la sauvegarde à restorer) puis se met en écoute.
	2. Le serveur envoie utilise le fichier "`.backup_log`" de la sauvegarde correspondante et utilise la fonction undeduplicate pour reconstruire le fichiers originaux.
	3. Ensuite le serveur envoie au client les fichiers
	4. Le client restore dans le dossier précisé les fichiers reçus
	5. Une fois tous les fichiers envoyés, le serveur envoie le mot "`EXIT`"
- Pour lister les backups :
	1. Le client envoie le texte "`LIST-BACKUP`" puis se met en écoute
	2. Le serveur construit la liste des backups dans son répertoire de sauvegarde et l'envoie au client
	3. Le client reçoit les informations et les affiche au fur et à mesure sur le terminale
	4. Une fois que le serveur à fini d'envoyer les informations, il envoie le mot "`EXIT`"



## Points notables

- copie avec `sendfile`
- suppression avec `unlink`
- copie par lien dur avec `link`
- date : combinaison de `gettimeofday` avec `localtime` et `strftime`

# Modalités d'évaluation

L'évaluation porte sur 3 éléments :

- Le rapport
	- un canvas vous est fourni sur la plateforme moodle ; vous devrez détailler plus précisément ce qui sera présenté à la soutenance, ainsi que des résultats quantitatifs et/ou qualitatifs de votre travail.
- La soutenance
	- une présentation de 10 minutes de tout le groupe (en équilibrant les temps de parole) dans laquelle :
		- vous rappellerez le contexte du projet, les grandes lignes de son fonctionnement (2 minutes)
		- vous expliquerez les choix que vous avez eu à faire et pourquoi (3 minutes)
		- vous présenterez les difficultés les plus importantes rencontrées et vos analyse à ce sujet (2 minutes)
		- vous proposerez un RETEX (retour d'expérience) dans lequel vous répondrez du mieux que possible à la question suivante : "_fort(e)s de notre expérience sur le projet, que ferions-nous différemment si nous devions le recommencer aujourd'hui ?_"
		- vous ferez une brève conclusion (1 minute)
	- vous répondrez ensuite à des questions posées par les enseignants pendant une dizaine de minutes.
- Le code
	- fourni dans un dépôt git avec l'historique des contributions de tous les membres,
	- avec un Makefile qui permet la compilation simplement avec la commande `make`
	- capacité à effectuer les traitements demandés dans le sujet,
	- capacité à traiter les cas particuliers sujets à erreur (pointeurs NULL, etc.)
	- Respect des conventions d'écriture de code
	- Documentation du code
		- Avec des commentaires au format doxygen en entêtes de fonction (si nécessaire)
		- Des commentaires pertinents sur le flux d'une fonction (astuces, cas limites, détails de l'algorithme, etc.)
	- Absence ou faible quantité de fuites mémoire (vérifiables avec `valgrind`)
	- **ATTENTION !** le code doit compiler sous Linux ! Un code non compatible avec un système Linux sera pénalisé de 5 points sur 20.

# Annexes

## Convention de code

Il est attendu dans ce projet que le code rendu satisfasse un certain nombre de conventions (ce ne sont pas des contraintes du langages mais des choix au début d'un projet) :

- indentations : les indentations seront faites sur un nombre d'espaces à votre discrétion, mais ce nombre **doit être cohérent dans l'ensemble du code**.
- Déclaration des pointeurs : l'étoile du pointeur est séparée du type pointé par un espace, et collée au nom de la variable, ainsi :
	- `int *a` est correct
	- `int* a`, `int*a` et `int * a` sont incorrects
- Nommage des variables, des types et des fonctions : vous utiliserez le *snake case*, i.e. des noms sans majuscule et dont les parties sont séparées par des underscores `_`, par exemple :
	- `ma_variable`, `variable`, `variable_1` et `variable1` sont corrects
	- `maVariable`, `Variable`, `VariableUn` et `Variable1` sont incorrects
- Position des accolades : une accolade s'ouvre sur la ligne qui débute son bloc (fonction, if, for, etc.) et est immédiatement suivie d'un saut de ligne. Elle se ferme sur la ligne suivant la dernière instruction. L'accolade fermante n'est jamais suivie d'instructions à l'exception du `else` ou du `while` (structure `do ... while`) qui suit l'accolade fermante. Par exemple :

```c
for (...) {
	/*do something*/
}

if (true) {
	/*do something*/
} else {
	/*do something else*/
}

int max(int a, int b) {
	return a;
}
```

sont corrects mais :

```c
for (int i=0; i<5; ++i)
{ printf("%d\n", i);
}

for (int i=0; i<5; ++i) {
	printf("%d\n", i); }

if () {/*do something*/
}
else
{
	/*do something else*/}
```

sont incorrects.
- Espacement des parenthèses : la parenthèse ouvrante après `if`, `for`, et `while` est séparée de ces derniers par un espace. Après un nom de fonction, l'espace est collé au dernier caractère du nom. Il n'y a pas d'espace après une parenthèse ouvrante, ni avant une parenthèse fermante :
	- `while (a == 3)`, `for (int i=0; i<3; ++i)`, `if (a == 3)` et `void ma_fonction(void)` sont corrects
	- `while(a == 3 )`, `for ( i=0;i<3 ; ++i)`, `if ( a==3)` et `void ma_fonction (void )` sont incorrects
- Basé sur les exemples ci dessus, également, les opérateurs sont précédés et suivis d'un espace, sauf dans la définition d'une boucle `for` où ils sont collés aux membres de droite et de gauche.
- Le `;` qui sépare les termes de la boucle `for` ne prend pas d'espace avant, mais en prend un après.
