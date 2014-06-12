/* Interface to get constants */

#ifndef SQL_OPT_COSTMODEL_INCLUDED
#define SQL_OPT_COSTMODEL_INCLUDED

class handler;

namespace Cost_factors
{
  void init();
  /* Engine specific constants */
  double read_factor(const handler *h);
  double scan_factor(const handler *h);

  /* Global constants */
  double time_for_compare();
  double time_for_compare_rowid();
}

#endif /* SQL_OPT_COSTMODEL_INCLUDED */
