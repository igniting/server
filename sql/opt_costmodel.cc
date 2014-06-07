#include "sql_base.h"
#include "key.h"
#include "records.h"
#include "opt_costmodel.h"

/* Name of database to which the optimizer_cost_factors table belongs */
static const LEX_STRING db_name= { C_STRING_WITH_LEN("mysql") };

/* Name of all_constants table */
static const LEX_STRING table_name = { C_STRING_WITH_LEN("optimizer_cost_factors") };

/* Columns in the optimizer_cost_factors_table */
enum cost_factors_col
{
  COST_FACTORS_CONST_NAME,
  COST_FACTORS_CONST_VALUE
};

/* Name of the constants present in table */
static const LEX_STRING read_time_factor_name = { C_STRING_WITH_LEN("READ_TIME_FACTOR") };
static const LEX_STRING scan_time_factor_name = { C_STRING_WITH_LEN("SCAN_TIME_FACTOR") };

/* Helper functions for Cost_factors::init() */

static
inline int open_table(THD *thd, TABLE_LIST *table,
                      Open_tables_backup *backup,
                      bool for_write)
{
  enum thr_lock_type lock_type_arg= for_write? TL_WRITE: TL_READ;
  table->init_one_table(db_name.str, db_name.length, table_name.str,
                        table_name.length, table_name.str, lock_type_arg);
  return open_system_tables_for_read(thd, table, backup);
}

static inline void clean_up(THD *thd)
{
  close_mysql_tables(thd);
  delete thd;
}

/* Initialize static class members */
bool Cost_factors::isInitialized= false;
double Cost_factors::read_time_factor= 1.0;
double Cost_factors::scan_time_factor= 1.0;

/* Interface functions */

void Cost_factors::init()
{
  TABLE_LIST table_list;
  Open_tables_backup open_tables_backup;
  READ_RECORD read_record_info;
  TABLE *table;
  MEM_ROOT mem;
  init_sql_alloc(&mem, 1024, 0, MYF(0));
  THD *new_thd = new THD;
  
  if(!new_thd)
  {
    free_root(&mem, MYF(0));
    DBUG_VOID_RETURN;
  }

  new_thd->thread_stack= (char *) &new_thd;
  new_thd->store_globals();
  new_thd->set_db(db_name.str, db_name.length);

  if(open_table(new_thd, &table_list, &open_tables_backup, FALSE))
  {
    clean_up(new_thd);
    DBUG_VOID_RETURN;
  }

  table= table_list.table;
  if(init_read_record(&read_record_info, new_thd, table, NULL, 1, 0, FALSE))
  {
    clean_up(new_thd);
    DBUG_VOID_RETURN;
  }

  table->use_all_columns();
  while (!read_record_info.read_record(&read_record_info))
  {
    LEX_STRING const_name;
    const_name.str= get_field(&mem, table->field[COST_FACTORS_CONST_NAME]);
    const_name.length= strlen(const_name.str);

    double const_value;
    const_value= table->field[COST_FACTORS_CONST_VALUE]->val_real();
    if(!strcmp(const_name.str, read_time_factor_name.str))
    {
      Cost_factors::read_time_factor= const_value;
    }
    else if(!strcmp(const_name.str, scan_time_factor_name.str))
    {
      Cost_factors::scan_time_factor= const_value;
    }
  }

  clean_up(new_thd);
  DBUG_VOID_RETURN;
}
