/* Interface to get constants */

#ifndef SQL_OPT_COSTMODEL_INCLUDED
#define SQL_OPT_COSTMODEL_INCLUDED

namespace Cost_factors
{
  extern double read_time_factor_val;
  extern double scan_time_factor_val;
  extern double time_for_compare_val;
  extern double time_for_compare_rowid_val;

  void init();
  inline double read_factor()
  {
    return read_time_factor_val;
  }
  inline double scan_factor()
  {
    return scan_time_factor_val;
  }
  inline double time_for_compare()
  {
    return time_for_compare_val;
  }
  inline double time_for_compare_rowid()
  {
    return time_for_compare_rowid_val;
  }
}

#endif /* SQL_OPT_COSTMODEL_INCLUDED */
