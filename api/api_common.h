#include <string>

//
// zmq context:
// TODO: push into Singleton or something comparable
extern void* context;
extern int runner_id;

struct Field;
extern Field field;


extern void* job_req_socket;
extern void* weight_push_socket;



std::string fix_port_name(const char * port_name_);
