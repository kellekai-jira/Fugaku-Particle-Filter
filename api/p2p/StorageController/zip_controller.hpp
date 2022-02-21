#ifndef __ZIP_CONTROLLER__
#define __ZIP_CONTROLLER__

#include "fti_kernel.hpp"
#include <queue>
#include <set>
#include <map>

enum FTIT_CPC_CASE {
  FTI_CPC_CASE_NONE = 0,
  FTI_CPC_ADAPTED = 1,
  FTI_CPC_STATIC,
};

namespace melissa {
  namespace zip {
    
    FTIT_CPC_MODE string2mode( std::string str );
    FTIT_CPC_TYPE string2type( std::string str );
    FTIT_CPC_CASE string2case( std::string str );

    struct zip_t {
      FTIT_CPC_CASE method;
      FTIT_CPC_MODE mode;
      int parameter; 
      FTIT_CPC_TYPE type;
      double rate;
      double sigma;

      public:

      bool operator<(const zip_t &rhs) const {
        return rhs.rate < rate;
      }
    };

  }
}

class ZipController {

  public:
   
  void init();

  void adaptParameter ( FTI::data_t* data, std::string name );
  
  private:
	
	std::map<std::string, std::queue<melissa::zip::zip_t> > m_vars;
	
  std::map<std::string, std::set<melissa::zip::zip_t> > m_vars_set;
  
  bool m_is_first;

  void select_parameters ( FTI::data_t* data, std::string name, double* original );
  void minimize ( FTI::data_t* data, std::string name, double* original );

  FTI::Kernel m_kernel;

};

#endif // __ZIP_CONTROLLER__