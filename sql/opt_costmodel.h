/* Interface to get constants */

#ifndef SQL_OPT_COSTMODEL_INCLUDED
#define SQL_OPT_COSTMODEL_INCLUDED

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
  ulonglong total_queries;
  double total_query_time;
  double total_query_time_squared;
  
  Cost_factor()
  {
    total_queries= 0;
    total_query_time= 0;
    total_query_time_squared= 0;
  }

  inline void add_query_time(double query_time)
  {
    total_queries++;
    total_query_time += query_time;
    total_query_time_squared += (query_time*query_time);
  }

  inline void update_all(Cost_factor that)
  {
    total_queries+= that.total_queries;
    total_query_time+= that.total_query_time;
    total_query_time_squared+= that.total_query_time_squared;
  }
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

  Global_cost_factors()
  {
    time_for_compare.value= 5;   // 5 compares == one read
    time_for_compare_rowid.value= 500;
  }

  void set_global_factor(const char *name, double value);
  void update_global_factor(uint index, double query_time);
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

  Engine_cost_factors()
  {
    read_time.value= 1;
    scan_time.value= 1;
  }

  void set_engine_factor(const char *name, double value);
  void update_engine_factor(uint index, double query_time);
  inline void update_engine_factor(Engine_cost_factors that)
  {
    read_time.update_all(that.read_time);
    scan_time.update_all(that.scan_time);
  }
};

class Cost_factors
{
private:
  Global_cost_factors global;
  //TODO: MAX_HA is not accessible here
  Engine_cost_factors engine[64];

public:
  void init();
  void re_init(THD *thd);
  /* Engine specific constants */
  double read_factor(const handler *) const;
  double scan_factor(const handler *) const;

  /* Global constants */
  double time_for_compare() const;
  double time_for_compare_rowid() const;

  /* Update a cost factor */
  void update_cost_factor(uint index, double query_time);

  /* Add data from another Cost_factors object */
  void add_data(Cost_factors that);
};

extern Cost_factors cost_factors;

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
