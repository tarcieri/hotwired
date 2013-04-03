#ifndef CONFIG_H
#define CONFIG_H

int config_init(char *filename);
void config_destroy();
char *config_vlist_value(char *section, char *var, int i);
char *config_value(char *section, char *var);
int config_int_value(char *section, char *var);
int config_truth_value(char *section, char *var);

#endif
