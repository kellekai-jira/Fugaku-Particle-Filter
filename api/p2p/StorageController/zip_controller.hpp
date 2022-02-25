#ifndef __ZIP_CONTROLLER__
#define __ZIP_CONTROLLER__

#include "fti_kernel.hpp"
#include <queue>
#include <set>
#include <map>

enum FTIT_CPC_CASE {
  FTI_CPC_CASE_NONE = 0,
  FTI_CPC_ADAPT,
  FTI_CPC_VALIDATE,
};

namespace melissa {
  namespace zip {
    
    FTIT_CPC_MODE string2mode( std::string str );
    FTIT_CPC_TYPE string2type( std::string str );
    FTIT_CPC_CASE string2case( std::string str );
    
    struct zip_t {
      FTIT_CPC_CASE method;
      FTIT_CPC_MODE mode;
      int id;
      int parameter; 
      FTIT_CPC_TYPE type;
      double rate;
      double sigma;

      public:

      bool operator<(const zip_t &rhs) const {
        return rhs.rate < rate;
      }
    };
   
    struct zip_params_t {
      
      zip_params_t(const zip_t & copy) {
        method    = copy.method    ;
        mode      = copy.mode      ;
        id        = copy.id        ;
        parameter = copy.parameter ; 
        type      = copy.type      ;
      }

      FTIT_CPC_CASE method;
      FTIT_CPC_MODE mode;
      int id;
      int parameter; 
      FTIT_CPC_TYPE type;
      
      public:

      bool operator<(const zip_params_t &rhs) const {
        if (   method <  rhs.method ) return true;
        if ( ( method == rhs.method ) && ( mode <  rhs.mode ) ) return true;
        if ( ( method == rhs.method ) && ( mode == rhs.mode ) && ( id <  rhs.id ) )return true;
        if ( ( method == rhs.method ) && ( mode == rhs.mode ) && ( id == rhs.id ) && ( parameter <  rhs.parameter ) ) return true;
        if ( ( method == rhs.method ) && ( mode == rhs.mode ) && ( id == rhs.id ) && ( parameter == rhs.parameter ) && ( type < rhs.type ) ) return true;
        return false;
      }
    };
    
    std::vector<zip_params_t> intersection (const std::vector<std::vector<zip_params_t>> &vecs);

  }
}

class ZipController {

  public:
   
  void init();
  bool is_adapt() { return m_case == FTI_CPC_ADAPT; }
  bool is_validate() { return m_case == FTI_CPC_VALIDATE; }
  void adaptParameter ( FTI::data_t* data, std::string name );
  
  private:
	
	std::map<std::string, std::queue<melissa::zip::zip_t> > m_vars;
	
  std::map<std::string, std::set<melissa::zip::zip_t> > m_vars_set;
  
  bool m_is_first;
  
  FTIT_CPC_CASE m_case;

  void select_parameters ( FTI::data_t* data, std::string name, double* original );
  void minimize ( FTI::data_t* data, std::string name, double* original );
  
  FTI::Kernel m_kernel;

};

#endif // __ZIP_CONTROLLER__
