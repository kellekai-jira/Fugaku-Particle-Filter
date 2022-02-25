
#include <iostream>
#include "zip_controller.hpp"
#include <cmath>
#include <climits>
#include <fstream>
#include <boost/json/src.hpp>
#include "utils.h"
#include "io_controller.hpp"
#include <iomanip>

namespace json = boost::json;

namespace melissa {
  namespace zip {
    void populate( json::object & obj, std::map<std::string, std::queue<melissa::zip::zip_t> > & vars, FTIT_CPC_CASE zcase );
  }
}

void str_to_lower( std::string & str ) {
  std::transform(str.begin(), str.end(), str.begin(),
      [](unsigned char c){ return std::tolower(c); });
}

constexpr unsigned int str2int(const char* str, int h = 0)
{
    return !str[h] ? 5381 : (str2int(str, h+1) * 33) ^ str[h];
}

std::string stream_as_string( std::istream& stm ) // #include <iterator>
{ return { std::istreambuf_iterator<char>(stm), std::istreambuf_iterator<char>{} } ; }

void ZipController::init() {
 
  m_is_first = true;

  std::ifstream jsonfile("compression.json");

  json::value tree = json::parse( stream_as_string(jsonfile) );
  
  json::object root = tree.as_object()["compression"].as_object();
  
  if ( root.find("method") == root.end() ) {
    std::cerr << "[error] no method specified" << std::endl;
    return;
  }

  std::string method = root["method"].as_string().c_str(); str_to_lower(method);
  
  if ( method == "adapt" ) {
    
    /****************************************************************************
     *
     *    store the parameters for the adapt case
     *
     **************************************************************************/

    if ( !root["adapt"].is_array() ) { 
      std::cerr << "[error] unexpected structure in json file" << std::endl;
      return;
    }

    auto am_var = root["adapt"].as_array().begin();
    for ( ; am_var != root["adapt"].as_array().end(); am_var++ ) {

      json::object obj = am_var->as_object();

      melissa::zip::populate( obj, m_vars, FTI_CPC_ADAPT );

    }

    m_case = FTI_CPC_ADAPT;

  } else if ( method == "validate" ) {
    
    /****************************************************************************
     *
     *    store the parameters for the validate case
     *
     **************************************************************************/
    
    if ( !root["validate"].is_array() ) { 
      std::cerr << "[error] unexpected structure in json file" << std::endl;
      return;
    }

    auto am_var = root["validate"].as_array().begin();
    for ( ; am_var != root["validate"].as_array().end(); am_var++ ) {
      
      json::object obj = am_var->as_object();

      melissa::zip::populate( obj, m_vars, FTI_CPC_VALIDATE );

    }
    
    m_case = FTI_CPC_VALIDATE;
    
  } else {
    MWRN("no method defined! skip compression.");
	}

}

void melissa::zip::populate( json::object & obj, std::map<std::string, std::queue<melissa::zip::zip_t> > & vars, FTIT_CPC_CASE zcase ) {
  
  static int id = 1;

  if ( obj.find("name") == obj.end() ) {
    std::cerr << "[error] variable without 'name'" << std::endl;
    return;
  }

  if ( !obj["name"].is_string() ) {
    std::cerr << "[error] variable 'name' has to be a string" << std::endl;
    return;
  }

  if ( obj.find("mode") == obj.end() ) {
    std::cerr << "[error] variable without 'mode'" << std::endl;
    return;
  }

  if ( obj.find("sigma") == obj.end() ) {
    std::cerr << "[error] variable without 'sigma' (i.e., error bound)" << std::endl;
    return;
  }   

  std::string name = obj["name"].as_string().c_str(); str_to_lower(name);
  std::string mode = obj["mode"].as_string().c_str(); str_to_lower(mode);

  if ( (obj.find("type") == obj.end()) && (mode == "zfp") ) {
    std::cerr << "[error] for 'mode' = 'zfp', 'type' can not be missing" << std::endl;
    return;
  }

  std::string type; 
  if ( obj.find("type") == obj.end() ) {
    type  = "none";
  } else {
    type = obj["type"].as_string().c_str(); str_to_lower(mode);
  }

  double sigma;
  if ( obj["sigma"].is_string() ) {
    sigma = std::stod( obj["sigma"].as_string().c_str() );
  } else if ( obj["sigma"].is_double() ) {
    sigma = obj["sigma"].as_double();
  }

  melissa::zip::zip_t zip;

  zip.method  = zcase;
  zip.mode    = melissa::zip::string2mode(mode);
  zip.type    = melissa::zip::string2type(type);
  zip.sigma   = sigma;
  zip.id      = id++;

  if ( obj.find("parameter") == obj.end() ) {
    zip.parameter = 0;
    vars[name].push(zip);
  } else if ( obj["parameter"].is_string() ) {
    zip.parameter = std::stoi( obj["parameter"].as_string().c_str() );
    vars[name].push(zip);
  } else if ( obj["parameter"].is_int64() ){
    zip.parameter = obj["parameter"].as_int64();
    vars[name].push(zip);
  } else if ( obj["parameter"].is_array() ) {
    auto pit = obj["parameter"].as_array().begin();
    for ( ; pit != obj["parameter"].as_array().end(); pit++ ) {
      if ( pit->is_string() ) {
        zip.parameter = std::stoi( pit->as_string().c_str() );
      } else if ( pit->is_int64() ){
        zip.parameter = pit->as_int64();
      }
      vars[name].push(zip);
    }
  }

}


void ZipController::adaptParameter ( FTI::data_t* data, std::string name ) {
  
  if ( m_vars.find(name) == m_vars.end() ) {
		return;
	}
  
  melissa::zip::zip_t zip;

  zip.method  = FTI_CPC_VALIDATE;
  zip.mode    = FTI_CPC_MODE_NONE;
  zip.type    = FTI_CPC_TYPE_NONE;
  zip.sigma   = 0;
  zip.id      = 0;
  zip.parameter = 0;
  zip.rate   = 0;
  m_vars[name].push(zip);

  if ( is_adapt() ) {
    
    double* original = (double*) data->ptr;

    if( m_is_first ) {
      select_parameters( data, name, original );
      std::cout << "== m_vars["<<name<<"] <- sorted ->" << std::endl;
      int i = 0;
      for(auto p : m_vars_set[name]) {
        std::cout << "==    ["<<i++<<"]" << std::endl;
        std::cout << "==    RATE      : " << p.rate << std::endl;
        std::cout << "==    mode      : " << p.mode << std::endl;
        std::cout << "==    parameter : " << p.parameter << std::endl;
        std::cout << "==    type      : " << p.type << std::endl;
      }
      fflush(stdout);
      m_is_first = false;
    } else {
      minimize( data, name, original );
    }

  }

  if ( is_validate() ) { 

  }

}
  
void ZipController::select_parameters ( FTI::data_t* data, std::string name, double* original ) {
 
  int64_t minSize = INT64_MAX;
  double* ptr = new double[data->count];
  
  while ( !m_vars[name].empty() ) {
    
    bool inBound = true;
    
    melissa::zip::zip_t zip = m_vars[name].front(); m_vars[name].pop();
    
    double maxError = zip.sigma;
  
    memcpy( ptr, original, data->count*sizeof(double));
    
    FTI::data_t data_train = *data;
    data_train.ptr = (void*) ptr;
    data_train.compression.mode = zip.mode;
    data_train.compression.parameter = zip.parameter;
    data_train.compression.type = zip.type;

    std::cout << "[BEFORE TRANSFORM]: memory available: " << static_cast<double>(get_mem_total())/(1024*1024) << " GB" << std::endl;
    double t0 = MPI_Wtime();
    try {
      m_kernel.transform( &data_train );
    } catch ( IoException &e ) {
      MDBG("%s", e.what() );
      continue;
    }
    std::cout << "[AFTER TRANSFORM]: memory available: " << static_cast<double>(get_mem_total())/(1024*1024) << " GB" << std::endl;
    double t1 = MPI_Wtime();
    
    double* compressed = (double*) data_train.ptr;

    double maxErrorTrain = 0;
    int64_t minSizeTrain = data_train.compression.size;

    for ( int i=0; i<data_train.count; i++ ) {
      double error = fabs( original[i] - compressed[i] );
      if ( error > zip.sigma ) {
        inBound = false;
        break;
      }
      if ( error > maxErrorTrain ) maxErrorTrain = error;
    }

    zip.rate = ((double)data->size) / data_train.compression.size;
     
    if( inBound ) {
      std::cout << "[add parameter (sigma="<<zip.sigma<<")    ]";
      std::cout << " mode: " << std::setw(2) << zip.mode;
      std::cout << " parameter: " << std::setw(3) << zip.parameter;
      std::cout << " rate: " << std::setw(10) << zip.rate;
      std::cout << " max error: " << std::setw(10) << maxErrorTrain;
      std::cout << " time [sec]: " << std::setw(10) << t1-t0 << std::endl;
      m_vars_set[name].insert( zip );
    } else {
      std::cout << "[discard parameter (sigma="<<zip.sigma<<")]";
      std::cout << " mode: " << std::setw(2) << zip.mode;
      std::cout << " parameter: " << std::setw(3) << zip.parameter;
      std::cout << " rate: " << std::setw(10) << zip.rate;
      std::cout << " max error: " << std::setw(10) << maxErrorTrain;
      std::cout << " time [sec]: " << std::setw(10) << t1-t0 << std::endl;
    }
    fflush(stdout);

    if( inBound && (minSizeTrain < minSize) ) {
      data->compression.mode = zip.mode; 
      data->compression.parameter = zip.parameter; 
      data->compression.type = zip.type;
      maxError = maxErrorTrain;
      minSize = minSizeTrain;
    }
  
  }
  
  delete ptr;

}

void ZipController::minimize ( FTI::data_t* data, std::string name, double* original ) {
 
  int64_t minSize = INT64_MAX;
  double* ptr = new double[data->count];
 		  
  std::cout << "[IN MINIMIZE FUNCTION]: memory available: " << static_cast<double>(get_mem_total())/(1024*1024) << " GB" << std::endl;
   
  for(auto zip : m_vars_set[name]) {
    
    bool inBound = true;

    memcpy( ptr, original, data->count*sizeof(double));
    
    FTI::data_t data_train = *data;
    data_train.ptr = (void*) ptr;
    data_train.compression.mode = zip.mode;
    data_train.compression.parameter = zip.parameter;
    data_train.compression.type = zip.type;

    try {
      m_kernel.transform( &data_train );
    } catch ( IoException &e ) {
      MDBG("%s", e.what() );
      continue;
    }
    
    double* compressed = (double*) data_train.ptr;

    for ( int i=0; i<data_train.count; i++ ) {
      double error = fabs( original[i] - compressed[i] );
      if ( error > zip.sigma ) {
        inBound = false;
        break;
      }
    }
     
    if( inBound ) {
      data->compression.mode = zip.mode; 
      data->compression.parameter = zip.parameter; 
      data->compression.type = zip.type;
      break;
    }
  
  }
  
  delete ptr;

}

FTIT_CPC_MODE melissa::zip::string2mode( std::string str ) {
  switch( str2int(str.c_str()) ) { 
    case str2int("none"):
      return FTI_CPC_MODE_NONE;
    case str2int("fpzip"):
      return FTI_CPC_FPZIP;
    case str2int("zfp"):
      return FTI_CPC_ZFP;
    case str2int("single"):
      return FTI_CPC_SINGLE;
    case str2int("half"):
      return FTI_CPC_HALF;
    default:
      std::cout << "[WARNING] - unknown compression mode '"<<str<<"'!" << std::endl;
      return FTI_CPC_MODE_NONE;
  }
}

FTIT_CPC_TYPE melissa::zip::string2type( std::string str ) {
  switch( str2int(str.c_str()) ) { 
    case str2int("none"):
      return FTI_CPC_TYPE_NONE;
    case str2int("accuracy"):
      return FTI_CPC_ACCURACY;
    case str2int("precision"):
      return FTI_CPC_PRECISION;
    default:
      std::cout << "[WARNING] - unknown compression mode '"<<str<<"'!" << std::endl;
      return FTI_CPC_TYPE_NONE;
  }
}

FTIT_CPC_CASE melissa::zip::string2case( std::string str ) {
  switch( str2int(str.c_str()) ) { 
    case str2int("accuracy"):
      return FTI_CPC_ADAPT;
    case str2int("precision"):
      return FTI_CPC_VALIDATE;
    default:
      std::cout << "[WARNING] - unknown compression mode '"<<str<<"'!" << std::endl;
      return FTI_CPC_CASE_NONE;
  }
}

std::vector<melissa::zip::zip_params_t> melissa::zip::intersection (const std::vector<std::vector<melissa::zip::zip_params_t>> &vecs) {

    auto last_intersection = vecs[0];
    std::vector<melissa::zip::zip_params_t> curr_intersection;

    for (std::size_t i = 1; i < vecs.size(); ++i) {
        std::set_intersection(last_intersection.begin(), last_intersection.end(),
            vecs[i].begin(), vecs[i].end(),
            std::back_inserter(curr_intersection));
        std::swap(last_intersection, curr_intersection);
        curr_intersection.clear();
    }
    return last_intersection;
}

