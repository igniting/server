/* Interface to get constants */

#ifndef _opt_costmodel_h
#define _opt_costmodel_h

enum enum_all_constants_col
{
  ALL_CONSTANTS_CONST_NAME,
  ALL_CONSTANTS_CONST_VALUE
};

double get_read_time_factor(THD *thd);
double get_scan_time_factor(THD *thd);

#endif /* _opt_costmodel_h */
