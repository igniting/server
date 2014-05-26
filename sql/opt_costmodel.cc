#include "sql_base.h"
#include "key.h"
#include "opt_costmodel.h"

/* Name of database to which the all_constants table belongs */
static const LEX_STRING db_name= { C_STRING_WITH_LEN("mysql") };

/* Name of all_constants table */
static const LEX_STRING table_name = { C_STRING_WITH_LEN("all_constants") };

/* Name of the constants present in table */
static const LEX_STRING read_time_factor = { C_STRING_WITH_LEN("READ_TIME_FACTOR") };
static const LEX_STRING scan_time_factor = { C_STRING_WITH_LEN("SCAN_TIME_FACTOR") };

/**
  @details
  The function builds a TABLE_LIST containing only one element 'tbl' for
  the all_constants table.
  The lock type of the element is set to TL_READ if for_write = FALSE,
  otherwise it is set to TL_WRITE.
*/

static
inline void init_table_list(TABLE_LIST *tbl, bool for_write)
{
  memset((char *) tbl, 0, sizeof(TABLE_LIST));

  tbl->db= db_name.str;
  tbl->db_length= db_name.length;
  tbl->alias= tbl->table_name= table_name.str;
  tbl->table_name_length= table_name.length;
  tbl->lock_type= for_write ? TL_WRITE : TL_READ;
}

/**
  @brief
  Open all_constants table and lock it
*/
static
inline int open_table(THD *thd, TABLE_LIST *table,
                      Open_tables_backup *backup,
                      bool for_write)
{
  init_table_list(table, for_write);
  init_mdl_requests(table);
  return open_system_tables_for_read(thd, table, backup);
}

class All_constants
{
private:
  /* Handler used for retrieval of all_constants table */
  handler *all_constants_file;
  /* Table to read constants from or to update/delete */
  TABLE *all_constants_table;
  /* Length of the key to access all_constants table */
  uint key_length;
  /* Number of the keys to access all_constants table */
  uint key_idx;
  /* Structure for the index to access all_constants table */
  KEY *key_info;
  /* Record buffers used to access/update all_constants table */
  uchar *record[2];

  LEX_STRING const_name;

  Field *const_name_field;
  Field *const_value_field;

  double const_value;

public:
  All_constants(TABLE *tab, LEX_STRING name)
    :all_constants_table(tab), const_name(name)
  {
    all_constants_file= all_constants_table->file;
    /* all_constants table has only one key */
    key_idx= 0;
    key_info= &all_constants_table->key_info[key_idx];
    key_length= key_info->key_length;
    record[0]= all_constants_table->record[0];
    record[1]= all_constants_table->record[1];
    const_name_field= all_constants_table->field[ALL_CONSTANTS_CONST_NAME];
    const_value_field= all_constants_table->field[ALL_CONSTANTS_CONST_VALUE];
    const_value= 1.0;
  }

  /** 
    @brief
    Set the key fields for the all_constants table

    @details
    The function sets the value of the field const_name
    in the record buffer for the table all_constants.
    This field is the primary key for the table.

    @note
    The function is supposed to be called before any use of the  
    method find_const for an object of the All_constants class. 
  */

  void set_key_fields()
  {
    const_name_field->store(const_name.str, const_name.length, system_charset_info);
  }

  /**
    @brief
    Find a record in the all_constants table by a primary key

    @details
    The function looks for a record in all_constants by its primary key.
    It assumes that the key fields have been already stored in the record 
    buffer of all_constants table.

    @retval
    FALSE     the record is not found
    @retval
    TRUE      the record is found
  */

  bool find_const()
  {
    uchar key[MAX_KEY_LENGTH];
    key_copy(key, record[0], key_info, key_length);
    return !all_constants_file->ha_index_read_idx_map(record[0], key_idx, key,
                                                      HA_WHOLE_KEY, HA_READ_KEY_EXACT);
  }

  void read_const_value()
  {
    if (find_const())
    {
      Field *const_field= all_constants_table->field[ALL_CONSTANTS_CONST_VALUE];
      if(!const_field->is_null())
      {
        const_value= const_field->val_real();
      }
    }
  }

  double get_const_value()
  {
    return const_value;
  }

  ~All_constants() {}
};

static double read_constant_from_table(THD *thd, const LEX_STRING const_name)
{
  TABLE_LIST table_list;
  Open_tables_backup open_tables_backup;

  if (open_table(thd, &table_list, &open_tables_backup, FALSE))
  {
    thd->clear_error();
    return 1.0;
  }

  All_constants all_constants(table_list.table, const_name);
  all_constants.set_key_fields();
  all_constants.read_const_value();

  close_system_tables(thd, &open_tables_backup);

  return all_constants.get_const_value();
}

double get_read_time_factor(THD *thd)
{
  return read_constant_from_table(thd, read_time_factor);
}

double get_scan_time_factor(THD *thd)
{
  return read_constant_from_table(thd, scan_time_factor);
}
