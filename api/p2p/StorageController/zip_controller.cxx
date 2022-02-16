
#include <iostream>
#include "zip_controller.hpp"
#include <cmath>
#include <climits>

void ZipController::adaptParameter ( FTI::data_t* data, std::queue<zip_t> parameters, double sigma ) {
  
  if ( m_vars.find(data->id) == m_vars.end() ) {
    std::set<zip_t> s;
    m_vars[data->id] = s;
  }

  double* original = (double*) data->ptr;
  
  double error = minimize( data, parameters, original, sigma );

  std::cout << "== m_vars["<<data->id<<"] <- sorted ->" << std::endl;
  int i = 0;
  for(auto p : m_vars[data->id]) {
    std::cout << "==    ["<<i++<<"]" << std::endl;
    std::cout << "==    RATE      : " << p.rate << std::endl;
    std::cout << "==    mode      : " << p.mode << std::endl;
    std::cout << "==    parameter : " << p.parameter << std::endl;
    std::cout << "==    type      : " << p.type << std::endl;
  }
	fflush(stdout);

}
  
double ZipController::minimize ( FTI::data_t* data, std::queue<zip_t> parameters, double* original, double sigma ) {
 
  double maxError = sigma;
  int64_t minSize = INT64_MAX;
  double* ptr = new double[data->count];
   
  while ( !parameters.empty() ) {
     
    bool inBound = true;
    
    zip_t zip = parameters.front(); parameters.pop();
  
    memcpy( ptr, original, data->count*sizeof(double));
    
    FTI::data_t data_train = *data;
    data_train.ptr = (void*) ptr;
    data_train.compression.mode = zip.mode;
    data_train.compression.parameter = zip.parameter;
    data_train.compression.type = zip.type;

    m_kernel.transform( &data_train );
    
    double* compressed = (double*) data_train.ptr;

    double maxErrorTrain = 0;
    int64_t minSizeTrain = data_train.sizeStored;

    for ( int i=0; i<data_train.count; i++ ) {
      double error = fabs( original[i] - compressed[i] );
      if ( error > sigma ) {
        inBound = false;
        break;
      }
      if ( error > maxErrorTrain ) maxErrorTrain = error;
    }

    zip.rate = ((double)data->size) / data_train.sizeStored;
     
    if( inBound ) {
      m_vars[data->id].insert( zip );
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

  return maxError;

}

