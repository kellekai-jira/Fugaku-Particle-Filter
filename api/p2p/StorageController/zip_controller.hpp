#ifndef __ZIP_CONTROLLER__
#define __ZIP_CONTROLLER__

#include "fti_kernel.hpp"
#include <vector>
#include <deque>
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
      
      zip_t() :
        method(FTI_CPC_CASE_NONE), 
        mode(FTI_CPC_MODE_NONE),
        id(0),
        parameter(0),
        type(FTI_CPC_TYPE_NONE)
      {}

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
  int get_num_parameters() { return m_num_parameters; }
  bool is_adapt() { return m_case == FTI_CPC_ADAPT; }
  bool is_validate() { return m_case == FTI_CPC_VALIDATE; }
  bool get_parameter_id() { return m_num_parameters - (m_parameter_id+1); }
  bool to_validate();
  
  void advance_validate();

  void adaptParameter ( FTI::data_t* data, std::string name );
  
  private:
	
  std::map<std::string,int> m_vars_parameter_id;
	
  std::map<std::string, std::vector<melissa::zip::zip_t> > m_vars;
	
  std::map<std::string, int> m_vars_num_parameters;
  
  int m_num_parameters;
  int m_parameter_id;

  std::map<std::string, std::set<melissa::zip::zip_t> > m_vars_set;
  
  bool m_is_first;
  bool m_validate_phase;

  FTIT_CPC_CASE m_case;

  void select_parameters ( FTI::data_t* data, std::string name, double* original );
  void minimize ( FTI::data_t* data, std::string name, double* original );
  
  FTI::Kernel m_kernel;

};

#endif // __ZIP_CONTROLLER__
