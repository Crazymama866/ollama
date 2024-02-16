#ifndef __APPLE__  // TODO - maybe consider nvidia support on intel macs?

#include <string.h>
#include "gpu_info_cudart.h"

void cudart_init(char *cudart_lib_path, cudart_init_resp_t *resp) {
  cudartReturn_t ret;
  resp->err = NULL;
  const int buflen = 256;
  char buf[buflen + 1];
  int i;

  struct lookup {
    char *s;
    void **p;
  } l[] = {
      {"cudaSetDevice", (void *)&resp->ch.cudaSetDevice},
      {"cudaDeviceReset", (void *)&resp->ch.cudaDeviceReset},
      {"cudaMemGetInfo", (void *)&resp->ch.cudaMemGetInfo},
      {"cudaGetDeviceCount", (void *)&resp->ch.cudaGetDeviceCount},
      {"cudaDeviceGetAttribute", (void *)&resp->ch.cudaDeviceGetAttribute},
      {"cudaDriverGetVersion", (void *)&resp->ch.cudaDriverGetVersion},
      {NULL, NULL},
  };

  resp->ch.handle = LOAD_LIBRARY(cudart_lib_path, RTLD_LAZY);
  if (!resp->ch.handle) {
    char *msg = LOAD_ERR();
    LOG(resp->ch.verbose, "library %s load err: %s\n", cudart_lib_path, msg);
    snprintf(buf, buflen,
            "Unable to load %s library to query for Nvidia GPUs: %s",
            cudart_lib_path, msg);
    free(msg);
    resp->err = strdup(buf);
    return;
  }

  // TODO once we've squashed the remaining corner cases remove this log
  LOG(resp->ch.verbose, "wiring cudart library functions in %s\n", cudart_lib_path);
  
  for (i = 0; l[i].s != NULL; i++) {
    // TODO once we've squashed the remaining corner cases remove this log
    LOG(resp->ch.verbose, "dlsym: %s\n", l[i].s);

    *l[i].p = LOAD_SYMBOL(resp->ch.handle, l[i].s);
    if (!l[i].p) {
      char *msg = LOAD_ERR();
      LOG(resp->ch.verbose, "dlerr: %s\n", msg);
      UNLOAD_LIBRARY(resp->ch.handle);
      resp->ch.handle = NULL;
      snprintf(buf, buflen, "symbol lookup for %s failed: %s", l[i].s,
              msg);
      free(msg);
      resp->err = strdup(buf);
      return;
    }
  }

  ret = (*resp->ch.cudaSetDevice)(0);
  if (ret != CUDART_SUCCESS) {
    LOG(resp->ch.verbose, "cudaSetDevice(0) err: %d\n", ret);
    UNLOAD_LIBRARY(resp->ch.handle);
    resp->ch.handle = NULL;
    snprintf(buf, buflen, "cuda runtime api init failure: %d", ret);
    resp->err = strdup(buf);
    return;
  }

  int version = 0;
  cudartDriverVersion_t driverVersion;
  driverVersion.major = 0;
  driverVersion.minor = 0;

  // Report driver version if we're in verbose mode, ignore errors
  ret = (*resp->ch.cudaDriverGetVersion)(&version);
  if (ret != CUDART_SUCCESS) {
    LOG(resp->ch.verbose, "cudaDriverGetVersion failed: %d\n", ret);
  } else {
    driverVersion.major = version / 1000;
    driverVersion.minor = (version - (driverVersion.major * 1000)) / 10;
    LOG(resp->ch.verbose, "CUDA driver version: %d-%d\n", driverVersion.major, driverVersion.minor);
  }
}


void cudart_check_vram(cudart_handle_t h, mem_info_t *resp) {
  resp->err = NULL;
  cudartMemory_t memInfo = {0};
  cudartReturn_t ret;
  const int buflen = 256;
  char buf[buflen + 1];
  int i;

  if (h.handle == NULL) {
    resp->err = strdup("cudart handle isn't initialized");
    return;
  }

  // cudaGetDeviceCount takes int type, resp-> count is uint
  int deviceCount;
  ret = (*h.cudaGetDeviceCount)(&deviceCount);
  if (ret != CUDART_SUCCESS) {
    snprintf(buf, buflen, "unable to get device count: %d", ret);
    resp->err = strdup(buf);
    return;
  } else {
    resp->count = (unsigned int)deviceCount;
  }

  resp->total = 0;
  resp->free = 0;
  for (i = 0; i < resp-> count; i++) {  
    ret = (*h.cudaSetDevice)(i);
    ret = (*h.cudaMemGetInfo)(&resp->free, &resp->total);
    if (ret != CUDART_SUCCESS) {
      snprintf(buf, buflen, "cudart device memory info lookup failure %d", ret);
      resp->err = strdup(buf);
      return;
    }

    if (h.verbose) {
      cudartBrandType_t brand = 0;
      // When in verbose mode, report more information about
      // the card we discover, but don't fail on error
      // Need to map out alternatives of these for CUDART libraries
      // For now just returning an
      ret = CUDART_UNSUPPORTED;
      if (ret != CUDART_SUCCESS) {
        LOG(h.verbose, "nvmlDeviceGetName unsupported with CUDART libraries: %d\n", ret);
      } else {
        LOG(h.verbose, "[%d] CUDA device name: %s\n", i, buf);
      }
      if (ret != CUDART_SUCCESS) {
        LOG(h.verbose, "nvmlDeviceGetBoardPartNumber unsupported with CUDART libraries: %d\n", ret);
      } else {
        LOG(h.verbose, "[%d] CUDA part number: %s\n", i, buf);
      }
      if (ret != CUDART_SUCCESS) {
        LOG(h.verbose, "nvmlDeviceGetSerial unsupported with CUDART libraries: %d\n", ret);
      } else {
        LOG(h.verbose, "[%d] CUDA S/N: %s\n", i, buf);
      }
      if (ret != CUDART_SUCCESS) {
        LOG(h.verbose, "nvmlDeviceGetVbiosVersion unsupported with CUDART libraries: %d\n", ret);
      } else {
        LOG(h.verbose, "[%d] CUDA vbios version: %s\n", i, buf);
      }
      if (ret != CUDART_SUCCESS) {
        LOG(h.verbose, "nvmlDeviceGetBrand unsupported with CUDART libraries: %d\n", ret);
      } else {
        LOG(h.verbose, "[%d] CUDA brand: %d\n", i, brand);
      }
    }

    LOG(h.verbose, "[%d] CUDA totalMem %ld\n", i, resp->total);
    LOG(h.verbose, "[%d] CUDA freeMem %ld\n", i, resp->free);

    resp->total += memInfo.total;
    resp->free += memInfo.free;
  }
}

void cudart_compute_capability(cudart_handle_t h, cudart_compute_capability_t *resp) {
  resp->err = NULL;
  resp->major = 0;
  resp->minor = 0;
  int major = 0;
  int minor = 0;
  cudartReturn_t ret;
  const int buflen = 256;
  char buf[buflen + 1];
  int i;

  if (h.handle == NULL) {
    resp->err = strdup("cudart handle not initialized");
    return;
  }

  int devices;
  ret = (*h.cudaGetDeviceCount)(&devices);
  if (ret != CUDART_SUCCESS) {
    snprintf(buf, buflen, "unable to get cudart device count: %d", ret);
    resp->err = strdup(buf);
    return;
  }

  for (i = 0; i < devices; i++) {
    ret = (*h.cudaDeviceGetAttribute)(&major, cudartDevAttrComputeCapabilityMajor, i);
    if (ret != CUDART_SUCCESS) {
      snprintf(buf, buflen, "device compute capability lookup failure %d: %d", i, ret);
      resp->err = strdup(buf);
      return;
    }
    ret = (*h.cudaDeviceGetAttribute)(&minor, cudartDevAttrComputeCapabilityMinor, i);
    if (ret != CUDART_SUCCESS) {
      snprintf(buf, buflen, "device compute capability lookup failure %d: %d", i, ret);
      resp->err = strdup(buf);
      return;
    }
      
    // Report the lowest major.minor we detect as that limits our compatibility
    if (resp->major == 0 || resp->major > major ) {
      resp->major = major;
      resp->minor = minor;
    } else if ( resp->major == major && resp->minor > minor ) {
      resp->minor = minor;
    }
  }
}

#endif  // __APPLE__