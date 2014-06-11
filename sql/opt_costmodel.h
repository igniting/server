/* Interface to get constants */

#ifndef SQL_OPT_COSTMODEL_INCLUDED
#define SQL_OPT_COSTMODEL_INCLUDED

namespace Cost_factors
{
  static double read_time_factor;
  static double scan_time_factor;

  void init();
  inline double read_factor()
  {
    return read_time_factor;
  }
  inline double scan_factor()
  {
    return scan_time_factor;
  }
};

#endif /* SQL_OPT_COSTMODEL_INCLUDED */
