#ifndef __ZIP_CONTROLLER__
#define __ZIP_CONTROLLER__

#include "fti_kernel.hpp"
#include <queue>
#include <set>
#include <map>

class ZipController {

  public:
  
  struct zip_t {
    FTIT_CPC_MODE mode;
    int parameter; 
    FTIT_CPC_TYPE type;
    double rate;

    public:
    
    bool operator<(const zip_t &rhs) const {
        return rhs.rate < rate;
    }
  };
  
  void adaptParameter ( FTI::data_t* data, std::queue<zip_t> parameters, double sigma );
  
  private:
 
  std::map< int, std::set<zip_t> > m_vars;
  double minimize ( FTI::data_t* data, std::queue<zip_t> parameter, double* original, double sigma );

  FTI::Kernel m_kernel;

};

#endif // __ZIP_CONTROLLER__
