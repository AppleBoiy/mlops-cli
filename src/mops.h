#ifndef MOPS_H
#define MOPS_H

#include <sqlite3.h>

/*
 * Database Operations
 */
int db_init(void);
void db_close(void);
sqlite3 *db_get_connection(void);

/*
 * Subcommand Dispatchers
 */
int cmd_disk(int argc, char **argv);
int cmd_sys(int argc, char **argv);
int cmd_dashboard(int argc, char **argv);
int cmd_net(int argc, char **argv);
int cmd_gcp(int argc, char **argv);
int cmd_task(int argc, char **argv);
int cmd_worker(int argc, char **argv);

/*
 * Utility Operations
 */
int cmd_doctor(int argc, char **argv);
int cmd_completion(int argc, char **argv);
int cmd_doctor(int argc, char **argv);
int cmd_completion(int argc, char **argv);


#endif /* MOPS_H */