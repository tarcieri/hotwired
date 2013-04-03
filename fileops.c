#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>

#include <Config.h>
#include <HashTable.h>
#include <RCL.h>
#include <Transaction.h>
#include <fileops.h>
#include <hlid.h>
#include <log.h>
#include <output.h>
#include <xmalloc.h>

/* TODO: Intelligent directory structure caching.  This code is largely
   inefficient.  Also, not all clients sort directory listings, so listings
   should be returned presorted.
   */

struct ext_list_element {
	char *ext;
	char *type;
};

#define REGISTERED_EXTENSIONS	27
#define MAX_EXT_LEN		4
struct ext_list_element ext_list[REGISTERED_EXTENSIONS] = {
	{ "hpf", 	"HTftHTLC" },
	{ "txt", 	"TEXTttxt" },
	{ "doc",	"W6BNMSWD" },
	{ "mp3", 	"MP3 MAmp" },
	{ "rm",		"RAE REAL" },
	{ "ram",	"RAE REAL" },
	{ "mid",	"MiDiTVOD" },
	{ "wav",	"WAVETVOD" },
	{ "aif",	"AIFFTVOD" },
	{ "aiff",	"AIFFTVOD" },
	{ "avi",	"VfW TVOD" },
	{ "mpg",	"MPEGTVOD" },
	{ "mpeg",	"MPEGTVOD" },
	{ "mov",	"MooVTVOD" },
	{ "fli",	"FLI TVOD" },
	{ "png",	"PNGfTVOD" },
	{ "exe",	"DEXECWIE" },
	{ "zip",	"ZIP ZIP " },
	{ "sit",	"SITDSIT!" },
	{ "bmp",	"BMP GKON" },
	{ "gif",	"GIFfGKON" },
	{ "jpg",	"JPEGGKON" },
	{ "jpeg",	"JPEGGKON" },
	{ "pct",	"PICTGKON" },
	{ "pict",	"PICTGKON" },
	{ "tif",	"TIFFGKON" },
	{ "tiff",	"TIFFGKON" }
};

static RCL bp_lock = NULL;
int base_path_l = 0;
static char *base_path = NULL;
HashTable ext_table = NULL;

void f_init()
{
	int r = 0, l, i;
	char *server_bp, *server_fp;
	struct stat st;

	if(bp_lock == NULL)
		bp_lock = rcl_create();

	rcl_write_lock(bp_lock);

	if(base_path != NULL)
		xfree(base_path);

	if((server_fp = config_value("paths", "file_path")) == NULL) {
		log("!!! Warning: No paths::file_path declared");

		base_path = NULL;
		goto done;
	}

	if((server_bp = config_value("paths", "base_path")) == NULL || strlen(server_bp) == 0) {
		if(*server_fp != '/') 
			log("!!! Warning: paths::base_path not declared and no leading / on paths::file_path");

		if((l = strlen(server_fp)) > 0 && server_fp[l - 1] != '/') {
			base_path = xasprintf("%s/", server_fp);
			xfree(server_fp);
		} else
			base_path = server_fp;

		if(server_bp != NULL)
			xfree(server_bp);

		goto done;
	}

	if(*server_bp != '/') 
		log("!!! Warning: No leading / on paths::base_path");

	if(server_bp[strlen(server_bp) - 1] != '/' && *server_fp != '/')
		base_path = xasprintf("%s/%s/", server_bp, server_fp);
	else
		base_path = xasprintf("%s%s/", server_bp, server_fp);

	if((l = strlen(base_path)) > 1 && base_path[l - 2] == '/')
		base_path[l - 1] = '\0';

	xfree(server_bp);
	xfree(server_fp);

done:
	if(base_path == NULL || (r = stat(base_path, &st)) < 0) {
		if(r < 0) 
			log("!!! Error: %s is inaccessable: %s", base_path, strerror(errno));

		if(base_path != NULL)
			xfree(base_path);

		base_path = (char *)xmalloc(MAXPATHLEN);
		getcwd(base_path, MAXPATHLEN);
		if((base_path_l = strlen(base_path)) > 0) {
			if(base_path[base_path_l - 1] != '/' && base_path_l + 1 < MAXPATHLEN)
				strcpy(base_path + base_path_l, "/");
		}


		log("!!! Using the current working directory (%s) for paths::file_path", base_path);
		log("!!! This probably isn't what you want.  Please correct your configuration");
	}

	base_path_l = strlen(base_path);

	if(ext_table == NULL) {
		ext_table = hash_table_create();

		for(i = 0; i < REGISTERED_EXTENSIONS; i++)
			hash_table_insert(ext_table, ext_list[i].ext, (void *)ext_list[i].type);
	}

	rcl_write_unlock(bp_lock);
}

char *f_addpath(char *str)
{
	char *ret;
	int l;

	if(str == NULL) {
		rcl_read_lock(bp_lock);
		ret = xstrdup(base_path);
		rcl_read_unlock(bp_lock);
	} else {
		l = strlen(str);
		ret = (char *)xmalloc(base_path_l + l + 1);
		rcl_read_lock(bp_lock);
		memcpy(ret, base_path, base_path_l);
		rcl_read_unlock(bp_lock);
		strcpy(ret + base_path_l, str);
	}

	return ret;
}

char *f_type(char *filename)
{
	int i, l;
	char *ext, lc_ext[MAX_EXT_LEN + 1], *tag;
	const char *def = "BINAdosa";

	if((ext = strrchr(filename, '.')) == NULL || strchr(ext, '/') != NULL) 
		return (char *)def;

	ext++;

	if((l = strlen(ext)) > MAX_EXT_LEN)
		return (char *)def;

	for(i = 0; i < l; i++)
		lc_ext[i] = tolower(ext[i]);

	lc_ext[i] = '\0';

	if((tag = (char *)hash_table_lookup(ext_table, lc_ext)) == NULL)
		return (char *)def;

	return tag;
}

/* This returns the number of items in a given directory.  A more efficient
   implementation of this code is probably in order */
static int dir_count(char *path)
{
	DIR *d;
	int ret = 0;

	if((d = opendir(path)) == NULL)
		return -1;

	while(readdir(d) != NULL)
		ret++;

	closedir(d);

	return ret - 2;
}

TransactionOut f_list_create(TransactionIn t, char *path)
{
	DIR *dir;
	struct dirent *d;
	struct stat st;

	int l, rl, s = 0;
	char fpath[MAXPATHLEN], *fp;
	uint8_t entry[20 + MAXPATHLEN];
	int32_t v;
	TransactionOut ret;

	path = f_addpath(path);
	l = strlen(path);

	if(l > 0 && path[l - 1] != '/')
		s = 1;

	if((rl = MAXPATHLEN - l - s) <= 0)
		goto err;

	strcpy(fpath, path);
	if(s)
		fpath[l] = '/';

	fp = fpath + l + s;

	if((v = dir_count(path)) < 0)
		goto err;

	if((dir = opendir(path)) == NULL)
		goto err;

	xfree(path);

	ret = transaction_reply_create(t, 0, v);

	while((d = readdir(dir)) != NULL) {
		l = strlen(d->d_name);

		if(l == 1 && *d->d_name == '.')
			continue;

		if(l == 2 && !strcmp(d->d_name, ".."))
			continue;

		strncpy(fp, d->d_name, rl);

		if(stat(fpath, &st) < 0) 
			continue;

		if(S_ISDIR(st.st_mode)) {
			memcpy(entry, "fldr\0\0\0\0", 8);
			if((v = dir_count(fpath)) < 0)
				continue;
			v = htonl(v);
		} else {
			memcpy(entry, f_type(fpath), 8);
			v = htonl(st.st_size);
		}

		memcpy(entry + 8, &v, 4);
		v = 0;
		memcpy(entry + 12, &v, 4);
		v = htonl(l);
		memcpy(entry + 16, &v, 4);
		memcpy(entry + 20, d->d_name, l);

		transaction_add_object(ret, HL_FILELIST_ENTRY, entry, 20 + l);
	}

	closedir(dir);
	return ret;
err:
	xfree(path);
	ret = transaction_reply_create(t, 1, 1);
	transaction_add_string(ret, HL_ERRORMSG, "Error: Access denied.");

	return ret;
}

static int rmdir_recursive(char *path)
{
	DIR *dir;
	struct dirent *d;
	struct stat st;
	int l, rl, s = 0, ret = -1;
	char fpath[MAXPATHLEN], *fp;

#ifdef DEBUG
	debug("rmdir_recursive() called for %s", path);
#endif
	l = strlen(path);

	if(l > 0 && path[l - 1] != '/')
		s = 1;

	if((rl = MAXPATHLEN - l - s) <= 0) 
		return -1;

	strcpy(fpath, path);
	if(s)
		fpath[l] = '/';

	fp = fpath + l + s;

	if((dir = opendir(path)) == NULL) 
		return -1;

	while((d = readdir(dir)) != NULL) {
		l = strlen(d->d_name);

		if(l == 1 && *d->d_name == '.')
			continue;

		if(l == 2 && !strcmp(d->d_name, ".."))
			continue;

		strncpy(fp, d->d_name, rl);

		if(stat(fpath, &st) < 0)
			goto done;

		if(S_ISDIR(st.st_mode)) {
			if(rmdir_recursive(fpath) < 0)
				goto done;
		} else {
			if(unlink(fpath) < 0)
				goto done;
		}
	}

	if(!rmdir(path))
		ret = 0;
done:
	closedir(dir);
	return ret;
}

int f_rmdir_recursive(char *path)
{
#ifdef DEBUG
	debug("f_rmdir_recursive() called for %s", path);
#endif

	if(rmdir(path) < 0) {
		if(config_truth_value("global", "allow_recursive_remove") != 1) 
			return -1;

		return rmdir_recursive(path);
	}

	return 0;
}
