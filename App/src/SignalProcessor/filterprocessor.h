#ifndef FILTERPROCESSOR_H
#define FILTERPROCESSOR_H

#include "filter.h"
#include "../openclcontext.h"
#include "../openclprogram.h"

#include <clFFT.h>

#include <vector>

class FilterProcessor
{
public:
	FilterProcessor(unsigned int M, unsigned int blockWidth, unsigned int blockHeight, OpenCLContext* context);
	~FilterProcessor();

	void change(Filter* filter);
	void process(cl_mem buffer, cl_command_queue queue);

private:
	unsigned int M;
	unsigned int width;
	unsigned int height;
	OpenCLProgram* program;
	cl_kernel filterKernel;
	cl_kernel zeroKernel;
	cl_mem filterBuffer;
	bool coefficientsChanged = false;
	std::vector<float> coefficients;
	clfftPlanHandle fftPlan;
	clfftPlanHandle fftPlanBatch;
	clfftPlanHandle ifftPlanBatch;
};

#endif // FILTERPROCESSOR_H