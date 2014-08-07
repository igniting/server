/* Interface to get constants */

#ifndef SQL_OPT_COSTMODEL_INCLUDED
#define SQL_OPT_COSTMODEL_INCLUDED

#include <map>

class handler;

#define MAX_EQUATIONS 20
#define MAX_CONSTANTS 130
#define MAX_GLOBAL_CONSTANTS 2
#define TIME_FOR_COMPARE 0
#define TIME_FOR_COMPARE_ROWID 1
#define MAX_ENGINE_CONSTANTS 2
#define READ_TIME 0
#define SCAN_TIME 1

inline uint get_factor_index(uint offset, int engine_index=-1)
{
  if(engine_index == -1)
    return offset;
  return MAX_GLOBAL_CONSTANTS + engine_index*MAX_ENGINE_CONSTANTS + offset;
}

class Cost_factor
{
public:
  double value;
  ulonglong total_ops;
  double total_time;
  double total_time_squared;
  
  Cost_factor()
  {
    total_ops= 0;
    total_time= 0;
    total_time_squared= 0;
  }

  inline void add_time(ulonglong ops, double value)
  {
    total_ops+= ops;
    total_time += (ops*value);
    total_time_squared += (ops*value)*(ops*value);
  }

  inline void update_all(Cost_factor that)
  {
    total_ops+= that.total_ops;
    total_time+= that.total_time;
    total_time_squared+= that.total_time_squared;
  }
};

/* Helper structure for assigning the value to appropriate variable by name */
struct st_factor {
  const char *name;
  Cost_factor *cost_factor;
  st_factor():name(0),cost_factor(0) {}
  st_factor(const char *name, Cost_factor *cost_factor):
    name(name),cost_factor(cost_factor) {}
};

class Global_cost_factors
{
public:
  /**
    The following is used to decide if MySQL should use table scanning
    instead of reading with keys.  The number says how many evaluation of the
    WHERE clause is comparable to reading one extra row from a table.
  */
  Cost_factor time_for_compare;
  /**
    Number of comparisons of table rowids equivalent to reading one row from a
    table.
  */
  Cost_factor time_for_compare_rowid;
  st_factor all_names[MAX_GLOBAL_CONSTANTS+1];

  Global_cost_factors()
  {
    time_for_compare.value= 5;   // 5 compares == one read
    time_for_compare_rowid.value= 500;
    set_all_names();
  }

  inline void set_all_names()
  {
    all_names[0] = st_factor("TIME_FOR_COMPARE", &time_for_compare);
    all_names[1] = st_factor("TIME_FOR_COMPARE_ROWID", &time_for_compare_rowid);
  }
  void set_global_factor(const char *name, double value, ulonglong total_ops,
      double total_time, double total_time_squared);
  void update_global_factor(uint index, ulonglong ops, double value);
  inline void update_global_factor(Global_cost_factors that)
  {
    time_for_compare.update_all(that.time_for_compare);
    time_for_compare_rowid.update_all(that.time_for_compare_rowid);
  }
};

class Engine_cost_factors
{
public:
  Cost_factor read_time;
  Cost_factor scan_time;
  static const double DEFAULT_READ_TIME= 1;
  static const double DEFAULT_SCAN_TIME= 1;
  st_factor all_names[MAX_ENGINE_CONSTANTS+1];

  Engine_cost_factors()
  {
    read_time.value= DEFAULT_READ_TIME;
    scan_time.value= DEFAULT_SCAN_TIME;
    set_all_names();
  }

  inline void set_all_names()
  {
    all_names[0]= st_factor("READ_TIME_FACTOR", &read_time);
    all_names[1]= st_factor("SCAN_TIME_FACTOR", &scan_time);
  }
  void set_engine_factor(const char *name, double value, ulonglong total_ops,
      double total_time, double total_time_squared);
  void update_engine_factor(uint index, ulonglong ops, double value);
  inline void update_engine_factor(Engine_cost_factors *that)
  {
    read_time.update_all(that->read_time);
    scan_time.update_all(that->scan_time);
  }
};

typedef std::map<uint, Engine_cost_factors*> engine_factor_map;
typedef std::pair<uint, Engine_cost_factors*> engine_factor_pair;

class Cost_factors
{
private:
  Global_cost_factors global;
  engine_factor_map engine;
  bool is_lock_initialized;
  bool has_unsaved_data;

public:
  Cost_factors(): is_lock_initialized(false) {}
  void init();
  void re_init(THD *thd);
  /* Engine specific constants */
  double read_factor(const handler *) const;
  double scan_factor(const handler *) const;

  /* Global constants */
  double time_for_compare() const;
  double time_for_compare_rowid() const;

  /* Update a cost factor */
  void update_cost_factor(uint index, ulonglong total_ops, double value);

  /* Add data from another Cost_factors object */
  void add_data(Cost_factors *that);

  /* Write data to optimizer_cost_factors table */
  void write_to_table();

  void cleanup();

  ~Cost_factors()
  {
    for(engine_factor_map::iterator it= engine.begin(); it != engine.end(); it++)
      delete it->second;
    engine.clear();
  }
};

extern Cost_factors cost_factors;
extern mysql_mutex_t cost_factors_lock;

/* Structure to store coefficients of system of linear equations */
struct eq_coefficient
{
  ulonglong value;
  eq_coefficient()
  {
    value= 0;
  }
};

#endif /* SQL_OPT_COSTMODEL_INCLUDED */
