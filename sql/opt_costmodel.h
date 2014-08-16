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
#define READ_FACTOR 0
#define SCAN_FACTOR 1

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
  ulonglong count;
  double sum;
  double sum_squared;
  
  Cost_factor()
  {
    count= 0;
    sum= 0;
    sum_squared= 0;
  }

  inline void add_value(double value)
  {
    count++;
    sum += value;
    sum_squared += value*value;
  }

  inline void update_all(Cost_factor that)
  {
    count+= that.count;
    sum+= that.sum;
    sum_squared+= that.sum_squared;
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
  void set_global_factor(const char *name, ulonglong count,
      double sum, double sum_squared);
  void update_global_factor(uint index, double value);
  inline void update_global_factor(Global_cost_factors that)
  {
    time_for_compare.update_all(that.time_for_compare);
    time_for_compare_rowid.update_all(that.time_for_compare_rowid);
  }
};

class Engine_cost_factors
{
public:
  Cost_factor read_factor;
  Cost_factor scan_factor;
  static const double DEFAULT_READ_FACTOR= 1;
  static const double DEFAULT_SCAN_FACTOR= 1;
  st_factor all_names[MAX_ENGINE_CONSTANTS+1];

  Engine_cost_factors()
  {
    read_factor.value= DEFAULT_READ_FACTOR;
    scan_factor.value= DEFAULT_SCAN_FACTOR;
    set_all_names();
  }

  inline void set_all_names()
  {
    all_names[0]= st_factor("READ_TIME_FACTOR", &read_factor);
    all_names[1]= st_factor("SCAN_TIME_FACTOR", &scan_factor);
  }
  void set_engine_factor(const char *name, ulonglong count,
      double sum, double sum_squared);
  void update_engine_factor(uint index, double value);
  inline void update_engine_factor(Engine_cost_factors *that)
  {
    read_factor.update_all(that->read_factor);
    scan_factor.update_all(that->scan_factor);
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
  void update_cost_factor(uint index, double value);

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
class eq_coefficient
{
public:
  ulonglong ops;
  double extra_factor;
  eq_coefficient()
  {
    ops= 0;
    extra_factor= 1;
  }
  inline double value() { return ops*extra_factor; }
};

#endif /* SQL_OPT_COSTMODEL_INCLUDED */
