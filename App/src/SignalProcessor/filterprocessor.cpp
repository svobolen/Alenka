#include "filterprocessor.h"

#include "../options.h"
#include "../error.h"

#include <cassert>

using namespace std;

FilterProcessor::FilterProcessor(unsigned int M, unsigned int blockWidth, unsigned int blockHeight, OpenCLContext* context) : M(M), width(blockWidth), height(blockHeight)
{
	assert(blockWidth%4 == 0);

	cl_int err;
	clfftStatus errFFT;

	FILE* file = fopen(PROGRAM_OPTIONS["kernels"].as<string>().c_str(), "rb");
	checkNotErrorCode(file, nullptr, "File '" << PROGRAM_OPTIONS["kernels"].as<string>() << "' could not be opened.");

	OpenCLProgram program(file, context);

	fclose(file);

	filterKernel = program.createKernel("filter");
	zeroKernel = program.createKernel("zero");

	cl_mem_flags flags = CL_MEM_READ_WRITE;
#ifdef NDEBUG
#if CL_VERSION_1_2
	flags |= CL_MEM_HOST_WRITE_ONLY;
#endif
#endif

	filterBuffer = clCreateBuffer(context->getCLContext(), flags, (width + 4)*sizeof(float), nullptr, &err);
	checkErrorCode(err, CL_SUCCESS, "clCreateBuffer");

	// Construct the fft plans.
	size_t size = width;
	size_t bufferDistance = size + 4;

	errFFT = clfftCreateDefaultPlan(&fftPlan, context->getCLContext(), CLFFT_1D, &size);
	checkErrorCode(errFFT, CLFFT_SUCCESS, "clfftCreateDefaultPlan()");
	clfftSetLayout(fftPlan, CLFFT_REAL, CLFFT_HERMITIAN_INTERLEAVED);
	clfftSetResultLocation(fftPlan, CLFFT_INPLACE);
	clfftSetPlanBatchSize(fftPlan, 1);
	//clfftSetPlanDistance(fftPlan, bufferDistance, bufferDistance/2);

	errFFT = clfftCreateDefaultPlan(&fftPlanBatch, context->getCLContext(), CLFFT_1D, &size);
	checkErrorCode(errFFT, CLFFT_SUCCESS, "clfftCreateDefaultPlan()");
	clfftSetLayout(fftPlanBatch, CLFFT_REAL, CLFFT_HERMITIAN_INTERLEAVED);
	clfftSetResultLocation(fftPlanBatch, CLFFT_INPLACE);
	clfftSetPlanBatchSize(fftPlanBatch, height);
	clfftSetPlanDistance(fftPlanBatch, bufferDistance, bufferDistance/2);

	errFFT = clfftCreateDefaultPlan(&ifftPlanBatch, context->getCLContext(), CLFFT_1D, &size);
	checkErrorCode(errFFT, CLFFT_SUCCESS, "clfftCreateDefaultPlan()");
	clfftSetLayout(ifftPlanBatch, CLFFT_HERMITIAN_INTERLEAVED, CLFFT_REAL);
	clfftSetResultLocation(ifftPlanBatch, CLFFT_INPLACE);
	clfftSetPlanBatchSize(ifftPlanBatch, height);
	clfftSetPlanDistance(ifftPlanBatch, bufferDistance/2, bufferDistance);
}

FilterProcessor::~FilterProcessor()
{
	cl_int err;

	err = clReleaseKernel(filterKernel);
	checkErrorCode(err, CL_SUCCESS, "clReleaseKernel()");

	err = clReleaseKernel(zeroKernel);
	checkErrorCode(err, CL_SUCCESS, "clReleaseKernel()");

	err = clReleaseMemObject(filterBuffer);
	checkErrorCode(err, CL_SUCCESS, "clReleaseMemObject()");

	clfftStatus errFFT;
	errFFT = clfftDestroyPlan(&fftPlan);
	checkErrorCode(errFFT, CLFFT_SUCCESS, "clfftDestroyPlan()");
	errFFT = clfftDestroyPlan(&fftPlanBatch);
	checkErrorCode(errFFT, CLFFT_SUCCESS, "clfftDestroyPlan()");
	errFFT = clfftDestroyPlan(&ifftPlanBatch);
	checkErrorCode(errFFT, CLFFT_SUCCESS, "clfftDestroyPlan()");
}

void FilterProcessor::change(Filter* filter)
{
	auto tmp = filter->computeCoefficients();

	for (unsigned int i = 0; i < M; ++i)
	{
		coefficients.push_back(static_cast<float>(tmp[i]));
	}

	coefficientsChanged = true;
}

void FilterProcessor::process(cl_mem buffer, cl_command_queue queue)
{
	cl_int err;
	clfftStatus errFFT;

	if (coefficientsChanged)
	{
		// Update values in the filterBuffer.
		coefficientsChanged = false;

		// This section is disabled because a bug in implementation of clEnqueueFillBuffer().
//#if CL_VERSION_1_2
//		float zero = 0;
//		err = clEnqueueFillBuffer(queue, filterBuffer, &zero, sizeof(zero), 0, width + 4, 0, nullptr, nullptr);
//		checkErrorCode(err, CL_SUCCESS, "clEnqueueFillBuffer()");
//#else
		err = clSetKernelArg(zeroKernel, 0, sizeof(cl_mem), &filterBuffer);
		checkErrorCode(err, CL_SUCCESS, "clSetKernelArg()");

		size_t globalWorkSize = (width + 4)/4;
		size_t globalWorkOffset = M/4;
		err = clEnqueueNDRangeKernel(queue, zeroKernel, 1, &globalWorkOffset, &globalWorkSize, nullptr, 0, nullptr, nullptr);
		checkErrorCode(err, CL_SUCCESS, "clEnqueueNDRangeKernel()");
//#endif

		err = clEnqueueWriteBuffer(queue, filterBuffer, CL_FALSE, 0, M*sizeof(float), coefficients.data(), 0, nullptr, nullptr);
		checkErrorCode(err, CL_SUCCESS, "clEnqueueWriteBuffer()");

		errFFT = clfftEnqueueTransform(fftPlan, CLFFT_FORWARD, 1, &queue, 0, nullptr, nullptr, &filterBuffer, nullptr, nullptr);
		checkErrorCode(errFFT, CLFFT_SUCCESS, "clfftEnqueueTransform");

		printBuffer("filterBuffer.txt", filterBuffer, queue);
	}

	// FFT.
	errFFT = clfftEnqueueTransform(fftPlanBatch, CLFFT_FORWARD, 1, &queue, 0, nullptr, nullptr, &buffer, nullptr, nullptr);
	checkErrorCode(errFFT, CLFFT_SUCCESS, "clfftEnqueueTransform");

	printBuffer("after_fft.txt", buffer, queue);

	// Multiply.
	err = clSetKernelArg(filterKernel, 0, sizeof(cl_mem), &buffer);
	checkErrorCode(err, CL_SUCCESS, "clSetKernelArg()");

	err = clSetKernelArg(filterKernel, 1, sizeof(cl_mem), &filterBuffer);
	checkErrorCode(err, CL_SUCCESS, "clSetKernelArg()");

	size_t globalWorkSize[2] = {height, (width + 4)/4};

	err = clEnqueueNDRangeKernel(queue, filterKernel, 2, nullptr, globalWorkSize, nullptr, 0, nullptr, nullptr);
	checkErrorCode(err, CL_SUCCESS, "clEnqueueNDRangeKernel()");

	printBuffer("after_multiply.txt", buffer, queue);

	// IFFT.
	errFFT = clfftEnqueueTransform(ifftPlanBatch, CLFFT_BACKWARD, 1, &queue, 0, nullptr, nullptr, &buffer, nullptr, nullptr);
	checkErrorCode(errFFT, CLFFT_SUCCESS, "clfftEnqueueTransform");
}
