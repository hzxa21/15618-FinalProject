#include <iostream>
#include <fstream>
#include <boost/program_options.hpp>
#include "CycleTimer.h"

using namespace std;
namespace po = boost::program_options;

bool ParseArgs(int argc, char *argv[], po::variables_map& args) {
  po::options_description desc("Options");
  desc.add_options()
  ("help,h", "Print help messages");
  
  try {
    po::store(po::parse_command_line(argc, argv, desc), args);
    
    if (args.count("help") > 0) {
      cout << desc << endl;
      return false;
    }
    
    po::notify(args);
  } catch(po::error& e) {
    std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
    std::cerr << desc << std::endl;
    return false;
  } catch(exception& e) {
    std::cerr << "Exception" << endl;
  }
  return true;
}

int main(int argc, char *argv[]) {
  // Parse command line arguments
  po::variables_map args;
  if (!ParseArgs(argc, argv, args))
    return 0;
  
}

