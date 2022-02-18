
#include <iostream>
#include "zip_controller.hpp"
#include <cmath>
#include <climits>
#include <fstream>
#include <boost/json/src.hpp>

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
  
  if ( !root["adapted"].is_array() ) { 
    std::cerr << "[error] unexpected structure in json file" << std::endl;
    return;
  }

  /****************************************************************************
   *
   *    store the parameters for the adaptive case
   *
   **************************************************************************/
  
  auto am_var = root["adapted"].as_array().begin();
  for ( ; am_var != root["adapted"].as_array().end(); am_var++ ) {
    
    json::object obj = am_var->as_object();
    
    std::cout << "static" << std::endl;

    melissa::zip::populate( obj, m_vars, FTI_CPC_ADAPTED );
    
  }

  /****************************************************************************
   *
   *    store the parameters for the static case
   *
   **************************************************************************/
  
  // TODO activate

  //am_var = root["static"].as_array().begin();
  //for ( ; am_var != root["static"].as_array().end(); am_var++ ) {
  //  
  //  json::object obj = am_var->as_object();
  //  
  //  populate( obj, FTI_CPC_STATIC );
  //  
  //}


}

void melissa::zip::populate( json::object & obj, std::map<std::string, std::queue<melissa::zip::zip_t> > & vars, FTIT_CPC_CASE zcase ) {

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

  double* original = (double*) data->ptr;
  
  if( m_is_first ) {
    select_parameters( data, name, original );
    m_is_first = false;
  } else {
    minimize( data, name, original );
  }

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

    m_kernel.transform( &data_train );
    
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
      m_vars_set[name].insert( zip );
    }

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
   
  for(auto zip : m_vars_set[name]) {
    
    bool inBound = true;

    memcpy( ptr, original, data->count*sizeof(double));
    
    FTI::data_t data_train = *data;
    data_train.ptr = (void*) ptr;
    data_train.compression.mode = zip.mode;
    data_train.compression.parameter = zip.parameter;
    data_train.compression.type = zip.type;

    m_kernel.transform( &data_train );
    
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
      return FTI_CPC_ADAPTED;
    case str2int("precision"):
      return FTI_CPC_STATIC;
    default:
      std::cout << "[WARNING] - unknown compression mode '"<<str<<"'!" << std::endl;
      return FTI_CPC_CASE_NONE;
  }
}
