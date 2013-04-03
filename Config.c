/*
   Config file parser - Horrible, hackish, overrecycled code.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include <HashTable.h>
#include <RCL.h>
#include <Config.h>
#include <xmalloc.h>

HashTable conf_table = NULL;
RCL conf_rcl = NULL;

/* Free each section of a hash table */
static void section_free(void *ptr)
{
	hash_table_destroy((HashTable)ptr, xfree);
}

/* In-place whitespace stripping function */
static void strip_whitespace(char *buffer)
{ 
	unsigned int i, j, q; 

	for(i = 0, j = 0, q = 0; i < strlen(buffer); i++) { 
		if(q == 1) { 
			if(buffer[i] == '\"') { 
				q = 0; 
				buffer[j++] = '\"'; 
				continue; 
			} 

			buffer[j++] = buffer[i]; 
			continue; 
		} 

		if(buffer[i] == '\"') { 
			q = 1; 
			buffer[j++] = '\"'; 
			continue; 
		} 

		if(buffer[i] == '#') { 
			buffer[j] = '\0'; 

			return; 
		} 

		if(isspace(buffer[i])) 
			continue; 

		buffer[j++] = buffer[i]; 
	} 

	buffer[j] = '\0'; 
}

static void add_value(char *section, char *varname, char **values)
{
	HashTable sect_table;

	if((sect_table = hash_table_lookup(conf_table, section)) == NULL) {
		sect_table = hash_table_create();
		hash_table_insert(conf_table, section, sect_table);
	}

	hash_table_insert(sect_table, varname, values);
}

int config_init(char *filename)
{
	FILE *conffile;
	int i, c, vc;
	char *buffer, *section = NULL, *tmptr, *varptr, **v;

	if(conf_rcl == NULL)
		conf_rcl = rcl_create();

	rcl_write_lock(conf_rcl);

	if((conffile = fopen(filename, "r")) == NULL) {
		rcl_write_unlock(conf_rcl);
		return -1;
	}

	if(conf_table != NULL)
		hash_table_destroy(conf_table, section_free);

	conf_table = hash_table_create();

	/* Keep buffer off of the stack */
	buffer = (char *)xmalloc(512);

	while(fgets(buffer, 512, conffile) != NULL) {
		strip_whitespace(buffer);
		if((tmptr = strchr(buffer, '[')) != NULL && strchr(tmptr + 1, ']') != NULL)
		{
			if(section != NULL) 
				xfree(section);

			*strchr(buffer, ']') = '\0';
			section = xstrdup(strchr(buffer, '[') + 1);

			v = NULL;
			continue;
		}

		if((tmptr = strchr(buffer, '=')) == NULL) 
			continue;

		*(tmptr++) = '\0';

		for(i = 0, c = 0; tmptr[i] != '\0'; i++)
			if(tmptr[i] == ',')
				c++;

		v = (char **)xmalloc((c + 2) * sizeof(char *));
		vc = 0;

		while(tmptr != NULL) {
			varptr = tmptr;
			
			if((tmptr = strchr(tmptr, ',')) != NULL)
				*tmptr++ = '\0';
			
			if(*varptr == '\"')
				varptr++;

			if(varptr[strlen(varptr) - 1] == '\"')
				varptr[strlen(varptr) - 1] = '\0';

			v[vc++] = xstrdup(varptr);
		}

		v[vc] = NULL;
		add_value(section, buffer, v);
	}

	if(section != NULL) 
		xfree(section);

	xfree(buffer);
	fclose(conffile);

	rcl_write_unlock(conf_rcl);

	return 0;
}

void config_destroy()
{
	hash_table_destroy(conf_table, section_free);
}

static char **config_vlist(char *section, char *var)
{
	HashTable sect_table;

	if(conf_table == NULL) 
		return NULL;

	if((sect_table = hash_table_lookup(conf_table, section)) == NULL)
		return NULL;

	return (char **)hash_table_lookup(sect_table, var);
}

char *config_vlist_value(char *section, char *var, int i)
{
	char **vlist, *ret = NULL;

	rcl_read_lock(conf_rcl);
	if((vlist = config_vlist(section, var)) != NULL && vlist[i] != NULL)
		ret = xstrdup(vlist[i]);

	rcl_read_unlock(conf_rcl);
	return ret;
}

char *config_value(char *section, char *var)
{
	char **vlist, *ret = NULL;

	rcl_read_lock(conf_rcl); 
	if((vlist = config_vlist(section, var)) != NULL && vlist[0] != NULL)
		ret = xstrdup(vlist[0]); 

	rcl_read_unlock(conf_rcl);
	return ret;
}

int config_int_value(char *section, char *var)
{
	int ret = -1;
	char **vlist;

	rcl_read_lock(conf_rcl);
	if((vlist = config_vlist(section, var)) != NULL && vlist[0] != NULL) 
		ret = atoi(vlist[0]);

	rcl_read_unlock(conf_rcl);

	return ret;
}

int config_truth_value(char *section, char *var)
{
	int ret = -1;
	char **vlist, *value;

	rcl_read_lock(conf_rcl);
	if((vlist = config_vlist(section, var)) == NULL || vlist[0] == NULL)
		goto done;
	
	ret = 1;
	value = vlist[0];

	if(!strcasecmp(value, "true"))
		goto done;

	if(!strcasecmp(value, "yes"))
		goto done;

	if(strlen(value) == 1 && (*value == 'y' || *value == 'Y'))
		goto done;

	ret = 0;
done:
	rcl_read_unlock(conf_rcl);
	return ret;
}
