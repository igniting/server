/* Interface to get constants */

#ifndef SQL_OPT_COSTMODEL_INCLUDED
#define SQL_OPT_COSTMODEL_INCLUDED

class Cost_factors
{
private:
  static bool isInitialized;
  static double read_time_factor;
  static double scan_time_factor;

public:
  static void init();
  static inline double get_read_time_factor()
  {
    return read_time_factor;
  }
  static inline double get_scan_time_factor()
  {
    return scan_time_factor;
  }
};

#endif /* SQL_OPT_COSTMODEL_INCLUDED */
