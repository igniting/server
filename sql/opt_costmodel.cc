#include "sql_base.h"
#include "key.h"
#include "records.h"
#include "opt_costmodel.h"

/* Name of database to which the optimizer_cost_factors table belongs */
const LEX_STRING db_name= { C_STRING_WITH_LEN("mysql") };

/* Name of all_constants table */
const LEX_STRING table_name = { C_STRING_WITH_LEN("optimizer_cost_factors") };

/* Columns in the optimizer_cost_factors_table */
enum cost_factors_col
{
  COST_FACTORS_CONST_NAME,
  COST_FACTORS_ENGINE_NAME,
  COST_FACTORS_CONST_VALUE
};

/* Helper functions for Cost_factors::init() */
inline int open_table(THD *thd, TABLE_LIST *table,
                      Open_tables_backup *backup,
                      bool for_write)
{
  enum thr_lock_type lock_type_arg= for_write? TL_WRITE: TL_READ;
  table->init_one_table(db_name.str, db_name.length, table_name.str,
                        table_name.length, table_name.str, lock_type_arg);
  return open_system_tables_for_read(thd, table, backup);
}

/* Helper structure for assigning the value to appropriate variable by name */
struct st_factor {
  const char *name;
  double *value;
};

void assign_factor_value(const char *const_name,
                         double const_value,
                         st_factor *factors)
{
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
  {
    sql_print_warning("Invalid row in the optimizer_cost_factors_table: %s",
        const_name);
  }
}

void Global_cost_factors::set_global_factor(const char *name, double value)
{
  st_factor all_names[] = {
    {"TIME_FOR_COMPARE", &time_for_compare.value},
    {"TIME_FOR_COMPARE_ROWID", &time_for_compare_rowid.value},
    {0, 0}
  };
  assign_factor_value(name, value, all_names);
}

void Engine_cost_factors::set_engine_factor(const char *name, double value)
{
  st_factor all_names[] = {
    {"READ_TIME_FACTOR", &read_time.value},
    {"SCAN_TIME_FACTOR", &scan_time.value},
    {0, 0}
  };
  assign_factor_value(name, value, all_names);
}

/* Interface functions */

void Cost_factors::init()
{
  DBUG_ENTER("Cost_factors::init");

  THD *new_thd = new THD;

  if(!new_thd)
  {
    DBUG_VOID_RETURN;
  }

  new_thd->thread_stack= (char *) &new_thd;
  new_thd->store_globals();
  new_thd->set_db(db_name.str, db_name.length);

  Cost_factors::re_init(new_thd);

  delete new_thd;
  set_current_thd(0);
  DBUG_VOID_RETURN;
}

void Cost_factors::re_init(THD *thd)
{
  TABLE_LIST table_list;
  Open_tables_backup open_tables_backup;
  READ_RECORD read_record_info;
  TABLE *table;
  MEM_ROOT mem;

  DBUG_ENTER("Cost_factors::re_init");

  init_sql_alloc(&mem, 1024, 0, MYF(0));

  if(open_table(thd, &table_list, &open_tables_backup, FALSE))
  {
    goto end;
  }
  table= table_list.table;
  if(init_read_record(&read_record_info, thd, table, NULL, 1, 0, FALSE))
  {
    goto end;
  }

  table->use_all_columns();
  while (!read_record_info.read_record(&read_record_info))
  {
    char *const_name= get_field(&mem, table->field[COST_FACTORS_CONST_NAME]);
    LEX_STRING engine_name;
    engine_name.str= get_field(&mem, table->field[COST_FACTORS_ENGINE_NAME]);
    double const_value;
    const_value= table->field[COST_FACTORS_CONST_VALUE]->val_real();

    // Engine name is null for global cost factors
    if(!engine_name.str)
    {
      global.set_global_factor(const_name, const_value);
    }
    else
    {
      engine_name.length= strlen(engine_name.str);
      plugin_ref engine_plugin= ha_resolve_by_name(thd, &engine_name);
      if(engine_plugin != NULL)
      {
        uint slot= plugin_data(engine_plugin, handlerton *)->slot;
        engine[slot].set_engine_factor(const_name, const_value);
      }
    }
  }

  end_read_record(&read_record_info);

end:
  close_system_tables(thd, &open_tables_backup);
  free_root(&mem, MYF(0));
  DBUG_VOID_RETURN;
}

double Cost_factors::read_factor(const handler *h) const
{
  return engine[h->ht->slot].read_time.value;
}

double Cost_factors::scan_factor(const handler *h) const
{
  return engine[h->ht->slot].scan_time.value;
}

double Cost_factors::time_for_compare() const
{
  return global.time_for_compare.value;
}

double Cost_factors::time_for_compare_rowid() const
{
  return global.time_for_compare_rowid.value;
}

Cost_factors cost_factors;
