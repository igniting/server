#include "sql_base.h"
#include "key.h"
#include "records.h"
#include "opt_costmodel.h"

namespace Cost_factors
{
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

  struct Engine_cost_factors
  {
      double read_time_factor_val;
      double scan_time_factor_val;
      Engine_cost_factors()
      {
        read_time_factor_val= 1.0;
        scan_time_factor_val= 1.0;
      }
  };

  Engine_cost_factors engine[MAX_HA];

  /**
    The following is used to decide if MySQL should use table scanning
    instead of reading with keys.  The number says how many evaluation of the
    WHERE clause is comparable to reading one extra row from a table.
  */
  double time_for_compare_val= 5;     // 5 compares == one read

  /**
    Number of comparisons of table rowids equivalent to reading one row from a
    table.
  */
  double time_for_compare_rowid_val= 500;

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

  /* All global cost factors */

  st_factor global_factors[] = {
    {"TIME_FOR_COMPARE", &time_for_compare_val},
    {"TIME_FOR_COMPARE_ROWID", &time_for_compare_rowid_val},
    {0, 0}
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

  void set_engine_factor(const char *const_name,
                         double const_value,
                         Engine_cost_factors *engine_cost_factors)
  {
    /* All engine specific cost factors */
    st_factor engine_factors[] = {
      {"READ_TIME_FACTOR", &engine_cost_factors->read_time_factor_val},
      {"SCAN_TIME_FACTOR", &engine_cost_factors->scan_time_factor_val},
      {0, 0}
    };
    assign_factor_value(const_name, const_value, engine_factors);
  }

  /* Interface functions */

  void init()
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

    re_init(new_thd);

    delete new_thd;
    set_current_thd(0);
    DBUG_VOID_RETURN;
  }

  void re_init(THD *thd)
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
        assign_factor_value(const_name, const_value, global_factors);
      }
      else
      {
        engine_name.length= strlen(engine_name.str);
        plugin_ref engine_plugin= ha_resolve_by_name(thd, &engine_name);
        if(engine_plugin != NULL)
        {
          uint slot= plugin_data(engine_plugin, handlerton *)->slot;
          set_engine_factor(const_name, const_value, &engine[slot]);
        }
      }
    }

    end_read_record(&read_record_info);

  end:
    close_system_tables(thd, &open_tables_backup);
    free_root(&mem, MYF(0));
    DBUG_VOID_RETURN;
  }

  double read_factor(const handler *h)
  {
    return engine[h->ht->slot].read_time_factor_val;
  }

  double scan_factor(const handler *h)
  {
    return engine[h->ht->slot].scan_time_factor_val;
  }

  double time_for_compare()
  {
    return time_for_compare_val;
  }

  double time_for_compare_rowid()
  {
    return time_for_compare_rowid_val;
  }
}
