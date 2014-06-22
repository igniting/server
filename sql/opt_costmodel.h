/* Interface to get constants */

#ifndef SQL_OPT_COSTMODEL_INCLUDED
#define SQL_OPT_COSTMODEL_INCLUDED

class handler;

#define MAX_EQUATIONS 30

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
};

extern Cost_factors cost_factors;

/*
   Structure to store coefficients of system of linear equations containing
   all constants and the total time
*/
struct measurement
{
  ulonglong time_for_compare;
  ulonglong time_for_compare_rowid;
  struct per_engine
  {
    ulonglong scan_time;
    ulonglong read_time;
    per_engine()
    {
      scan_time= 0;
      read_time= 0;
    }
  } per_engine[64];
  ulonglong total_time;

  measurement()
  {
    time_for_compare= 0;
    time_for_compare_rowid= 0;
    total_time= 0;
  }
};

#endif /* SQL_OPT_COSTMODEL_INCLUDED */
