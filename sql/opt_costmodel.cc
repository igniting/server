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
struct st_factor {
  const char *name;
  double *value;
};

double Cost_factors::read_time_factor= 1;
double Cost_factors::scan_time_factor= 1;

st_factor factors[] = {
  {"READ_TIME_FACTOR", &Cost_factors::read_time_factor},
  {"SCAN_TIME_FACTOR", &Cost_factors::scan_time_factor},
  {0, 0}
};

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

/* Interface functions */

void Cost_factors::init()
{
  TABLE_LIST table_list;
  Open_tables_backup open_tables_backup;
  READ_RECORD read_record_info;
  TABLE *table;
  MEM_ROOT mem;
  DBUG_ENTER("Cost_factors::init");

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
    goto end;
  }

  table= table_list.table;
  if(init_read_record(&read_record_info, new_thd, table, NULL, 1, 0, FALSE))
  {
    goto end;
  }

  table->use_all_columns();
  while (!read_record_info.read_record(&read_record_info))
  {
    char *const_name= get_field(&mem, table->field[COST_FACTORS_CONST_NAME]);
    double const_value;
    const_value= table->field[COST_FACTORS_CONST_VALUE]->val_real();
    st_factor *f;
    for(f=factors; f->name; f++)
    {
      if(strcasecmp(f->name, const_name) == 0)
      {
        *(f->value)= const_value;
        break;
      }
    }

    if(f->name == 0)
      sql_print_warning("Invalid row in the optimizer_cost_factors_table: %s",
          const_name);
  }

  end_read_record(&read_record_info);

end:
  close_system_tables(new_thd, &open_tables_backup);
  delete new_thd;
  free_root(&mem, MYF(0));
  set_current_thd(0);
  DBUG_VOID_RETURN;
}
