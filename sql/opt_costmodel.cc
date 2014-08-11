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
  COST_FACTORS_CONST_VALUE,
  COST_FACTORS_TOTAL_OPS,
  COST_FACTORS_TOTAL_TIME,
  COST_FACTORS_TOTAL_TIME_SQUARED
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

void assign_factor_value(const char *const_name, double value, ulonglong total_ops,
    double total_time, double total_time_squared, st_factor *factors)
{
  st_factor *f;
  for(f=factors; f->name; f++)
  {
    if(strcasecmp(f->name, const_name) == 0)
    {
      f->cost_factor->value= value;
      f->cost_factor->total_ops= total_ops;
      f->cost_factor->total_time= total_time;
      f->cost_factor->total_time_squared= total_time_squared;
      break;
    }
  }
  if(f->name == 0)
  {
    sql_print_warning("Invalid row in the optimizer_cost_factors_table: %s",
        const_name);
  }
}

void Global_cost_factors::set_global_factor(const char *name, double value,
    ulonglong total_ops, double total_time, double total_time_squared)
{
  assign_factor_value(name, value, total_ops, total_time, total_time_squared, all_names);
}

void Global_cost_factors::update_global_factor(uint index, ulonglong ops, double value)
{
  switch(index)
  {
    case TIME_FOR_COMPARE:
      time_for_compare.add_time(ops, value);
      break;
    case TIME_FOR_COMPARE_ROWID:
      time_for_compare_rowid.add_time(ops, value);
      break;
  }
}

void Engine_cost_factors::set_engine_factor(const char *name, double value,
    ulonglong total_ops, double total_time, double total_time_squared)
{
  assign_factor_value(name, value, total_ops, total_time, total_time_squared, all_names);
}

void Engine_cost_factors::update_engine_factor(uint index, ulonglong ops, double value)
{
  switch(index)
  {
    case READ_FACTOR:
      read_factor.add_time(ops, value);
      break;
    case SCAN_FACTOR:
      scan_factor.add_time(ops, value);
      break;
  }
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

  PSI_mutex_key key_cost_factors_lock;
  PSI_mutex_info mutexes[] = {{ &key_cost_factors_lock, "cost_factors_lock", 0}};
  mysql_mutex_register("sql", mutexes, 1);
  mysql_mutex_init(key_cost_factors_lock, &cost_factors_lock, NULL);
  is_lock_initialized= true;

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
    double const_value= table->field[COST_FACTORS_CONST_VALUE]->val_real();
    ulonglong total_ops= (ulonglong) table->field[COST_FACTORS_TOTAL_OPS]->val_int();
    double total_time= table->field[COST_FACTORS_TOTAL_TIME]->val_real();
    double total_time_squared= table->field[COST_FACTORS_TOTAL_TIME_SQUARED]->val_real();

    // Engine name is null for global cost factors
    if(!engine_name.str)
    {
      global.set_global_factor(const_name, const_value, total_ops, total_time, total_time_squared);
    }
    else
    {
      engine_name.length= strlen(engine_name.str);
      plugin_ref engine_plugin= ha_resolve_by_name(thd, &engine_name);
      if(engine_plugin != NULL)
      {
        uint slot= plugin_data(engine_plugin, handlerton *)->slot;
        engine_factor_map::iterator element= engine.find(slot);
        if(element != engine.end())
        {
          element->second->set_engine_factor(const_name, const_value, total_ops, total_time, total_time_squared);
        }
        else
        {
          Engine_cost_factors *new_element= new Engine_cost_factors();
          new_element->set_engine_factor(const_name, const_value, total_ops, total_time, total_time_squared);
          engine.insert(engine_factor_pair(slot, new_element));
        }
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
  engine_factor_map::const_iterator it= engine.find(h->ht->slot);
  if(it != engine.end())
  {
    return it->second->read_factor.value;
  }
  else
  {
    return Engine_cost_factors::DEFAULT_READ_FACTOR;
  }
}

double Cost_factors::scan_factor(const handler *h) const
{
  engine_factor_map::const_iterator it= engine.find(h->ht->slot);
  if(it != engine.end())
  {
    return it->second->scan_factor.value;
  }
  else
  {
    return Engine_cost_factors::DEFAULT_SCAN_FACTOR;
  }
}

double Cost_factors::time_for_compare() const
{
  return global.time_for_compare.value;
}

double Cost_factors::time_for_compare_rowid() const
{
  return global.time_for_compare_rowid.value;
}

void Cost_factors::update_cost_factor(uint index, ulonglong ops, double value)
{
  has_unsaved_data= true;
  if(index < MAX_GLOBAL_CONSTANTS)
    global.update_global_factor(index, ops, value);
  else
  {
    uint engine_no = (index - MAX_GLOBAL_CONSTANTS)/ MAX_ENGINE_CONSTANTS;
    uint const_no = (index - MAX_GLOBAL_CONSTANTS) % MAX_ENGINE_CONSTANTS;
    engine_factor_map::iterator element= engine.find(engine_no);
    if(element != engine.end())
    {
      element->second->update_engine_factor(const_no, ops, value);
    }
    else
    {
      Engine_cost_factors *new_element= new Engine_cost_factors();
      new_element->update_engine_factor(const_no, ops, value);
      engine.insert(engine_factor_pair(engine_no, new_element));
    }
  }
}

void Cost_factors::add_data(Cost_factors *that)
{
  has_unsaved_data= true;
  global.update_global_factor(that->global);
  for(engine_factor_map::iterator it= that->engine.begin();
      it!= that->engine.end(); it++)
  {
    engine_factor_map::iterator element= engine.find(it->first);
    if(element != engine.end())
    {
      element->second->update_engine_factor(it->second);
    }
    else
    {
      Engine_cost_factors *new_element= new Engine_cost_factors();
      new_element->update_engine_factor(it->second);
      engine.insert(engine_factor_pair(it->first, new_element));
    }
  }
}

void Cost_factors::write_to_table()
{
  TABLE_LIST table_list;
  Open_tables_backup open_tables_backup;
  TABLE *table;

  DBUG_ENTER("Cost_factors::write_to_table");

  if(!has_unsaved_data)
  {
    DBUG_VOID_RETURN;
  }

  THD *thd = new THD;

  if(!thd)
  {
    DBUG_VOID_RETURN;
  }

  thd->thread_stack= (char *) &thd;
  thd->store_globals();
  thd->set_db(db_name.str, db_name.length);

  if(open_table(thd, &table_list, &open_tables_backup, true))
  {
    goto end;
  }

  table= table_list.table;
  table->use_all_columns();

  uchar key[MAX_KEY_LENGTH];

  /* Write all global factors */
  for(st_factor *f= global.all_names; f->name; f++)
  {
    if(f->cost_factor->total_ops != 0)
    {
      table->field[0]->store(f->name, (uint) strlen(f->name), system_charset_info);
      table->field[1]->store("", 0, system_charset_info);
      key_copy(key, table->record[0], table->key_info, table->key_info->key_length);
      if (table->file->ha_index_read_idx_map(table->record[0], 0,
            key, HA_WHOLE_KEY, HA_READ_KEY_EXACT))
      {
        /* If the constant is not present, insert in table */
        table->field[2]->store(f->cost_factor->value);
        table->field[3]->store(f->cost_factor->total_ops);
        table->field[4]->store(f->cost_factor->total_time);
        table->field[5]->store(f->cost_factor->total_time_squared);
        table->file->ha_write_row(table->record[0]);
      }
      else
      {
        /* Update the column */
        store_record(table, record[1]);
        table->field[2]->store(f->cost_factor->value);
        table->field[3]->store(f->cost_factor->total_ops);
        table->field[4]->store(f->cost_factor->total_time);
        table->field[5]->store(f->cost_factor->total_time_squared);
        table->file->ha_update_row(table->record[1], table->record[0]);
      }
    }
  }

  /* Write all engine specific factors */
  for(engine_factor_map::iterator it= engine.begin(); it != engine.end(); it++)
  {
    /* Get the engine name from the slot number, it->first */
    LEX_STRING engine_name= hton2plugin[it->first]->name;
    for(st_factor *f= it->second->all_names; f->name; f++)
    {
      if(f->cost_factor->total_ops != 0)
      {
        table->field[0]->store(f->name, (uint) strlen(f->name), system_charset_info);
        table->field[1]->store(engine_name.str, engine_name.length, system_charset_info);
        key_copy(key, table->record[0], table->key_info, table->key_info->key_length);
        if (table->file->ha_index_read_idx_map(table->record[0], 0,
              key, HA_WHOLE_KEY, HA_READ_KEY_EXACT))
        {
          /* If the constant is not present, insert in table */
          table->field[2]->store(f->cost_factor->value);
          table->field[3]->store(f->cost_factor->total_ops);
          table->field[4]->store(f->cost_factor->total_time);
          table->field[5]->store(f->cost_factor->total_time_squared);
          table->file->ha_write_row(table->record[0]);
        }
        else
        {
          /* Update the column */
          store_record(table, record[1]);
          table->field[2]->store(f->cost_factor->value);
          table->field[3]->store(f->cost_factor->total_ops);
          table->field[4]->store(f->cost_factor->total_time);
          table->field[5]->store(f->cost_factor->total_time_squared);
          table->file->ha_update_row(table->record[1], table->record[0]);
        }
      }
    }
  }
  close_system_tables(thd, &open_tables_backup);
end:
  delete thd;
  set_current_thd(0);
  DBUG_VOID_RETURN;
}

void Cost_factors::cleanup()
{
  if(is_lock_initialized)
  {
    mysql_mutex_destroy(&cost_factors_lock);
  }
}

Cost_factors cost_factors;
mysql_mutex_t cost_factors_lock;
