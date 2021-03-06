#ifndef cl_boilerplate_hpp
#define cl_boilerplate_hpp
#include <vector>
#include <fstream>

#define __CL_ENABLE_EXCEPTIONS
#include "CL/cl.hpp"

#include "bitecoin_log.hpp"

namespace bitecoin {
std::string loadSource(const char *fileName) {
  std::string baseDir = ".";
  if (getenv("HPCE_CL_SRC_DIR")) {
    baseDir = getenv("HPCE_CL_SRC_DIR");
  }

  std::string fullName = baseDir + "/" + fileName;

  std::ifstream src(fullName, std::ios::in | std::ios::binary);
  if (!src.is_open())
    throw std::runtime_error("loadSource : Couldn't load cl file from '" +
                             fullName + "'.");

  return std::string(
      (std::istreambuf_iterator<char>(src)), // Node the extra brackets.
      std::istreambuf_iterator<char>());
}

cl::Program setupOpenCL(std::vector<cl::Platform> &platforms,
                        std::vector<cl::Device> &devices, cl::Device &device,
                        cl::Context &context, std::shared_ptr<ILog> &log) {
  // Initialise OpenCL
  cl::Platform::get(&platforms);
  if (platforms.size() == 0)
    throw std::runtime_error("No OpenCL platforms found.");

  bool foundDevice = false;
  // Choose ONLY NVIDIA GPU
  log->Log(Log_Info, "[OpenCL] Found %d platforms", platforms.size());
  for (unsigned i = 0; i < platforms.size(); i++) {
    std::string vendor = platforms[i].getInfo<CL_PLATFORM_VENDOR>();
    log->Log(Log_Info, "[OpenCL]  Platform %i: %s", i, vendor.c_str());

    cl::Platform platform = platforms.at(i);
    platform.getDevices(CL_DEVICE_TYPE_GPU, &devices);
    if (devices.size() != 0) {
      // select the first GPU device
      log->Log(Log_Info, "[OpenCL] Found %d GPU devices", devices.size());
      for (unsigned i = 0; i < devices.size(); i++) {
        std::string name = devices[i].getInfo<CL_DEVICE_NAME>();
        log->Log(Log_Info, "[OpenCL]  Device %i: %s", i, name.c_str());
        std::string manufacturer = devices[i].getInfo<CL_DEVICE_VENDOR>();
        if (manufacturer.find("NVIDIA") != std::string::npos) {
          log->Log(Log_Info, "[OpenCL] Choosing device %d", 0);
          device = devices.at(0);
          foundDevice = true;
          break;
        }
      }
    }
    if (foundDevice)
      break;
  }
  if (!foundDevice) {
    throw std::runtime_error("No GPU devices found on any platforms.");
  }

  context = cl::Context(devices);

  std::string kernelSource = loadSource("miner/pool_hash_step.cl");
  cl::Program::Sources sources; // A vector of (data,length) pairs
  sources.push_back(
      std::make_pair(kernelSource.c_str(), kernelSource.size() + 1));
  cl::Program program(context, sources);
  try {
    log->Log(Log_Info, "[OpenCL] Building Kernel");
    program.build(devices);
    return program;
  }
  catch (...) {
    for (unsigned i = 0; i < devices.size(); i++) {
      std::cerr << "Log for device " << devices[i].getInfo<CL_DEVICE_NAME>()
                << ":\n\n";
      std::cerr << program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(devices[i])
                << "\n\n";
    }
    throw;
  }
}
}; // namespace bitecoin
#endif
