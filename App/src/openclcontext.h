#ifndef OPENCLCONTEXT_H
#define OPENCLCONTEXT_H

#include <QOpenGLContext>
#include <CL/cl_gl.h>

#include <string>

class OpenCLContext
{
public:
	OpenCLContext(unsigned int platform, unsigned int device, cl_device_type deviceType, QOpenGLContext* parentContext = nullptr);
	~OpenCLContext();

	cl_context getCLContext() const
	{
		return context;
	}
	cl_platform_id getCLPlatform() const
	{
		return platformId;
	}
	cl_device_id getCLDevice() const
	{
		return deviceId;
	}
	std::string getPlatformInfo() const;
	std::string getDeviceInfo() const;

private:
	cl_context context;
	cl_platform_id platformId;
	cl_device_id deviceId;
};

#define OPENCL_CONTEXT_CONSTRUCTOR_PARAMETERS PROGRAM_OPTIONS["clPlatform"].as<int>(), PROGRAM_OPTIONS["clDevice"].as<int>(), CL_DEVICE_TYPE_ALL

#endif // OPENCLCONTEXT_H

