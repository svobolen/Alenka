#include "montageprocessor.h"

#include "../error.h"

using namespace std;

MontageProcessor::MontageProcessor(unsigned int offset, unsigned int blockWidth) :
	inputRowLength((offset + blockWidth + 4)/4), inputRowOffset(offset/4), outputRowLength(blockWidth/4)
{

}

MontageProcessor::~MontageProcessor()
{
	releaseMontage();
}

void MontageProcessor::change(Montage* montage)
{
	releaseMontage();
	montageKernel = montage->getKernel();
	numberOfRows = montage->getNumberOfRows();
}

void MontageProcessor::process(cl_mem inBuffer, cl_mem outBuffer, cl_command_queue queue)
{
	cl_int err;

	err = clSetKernelArg(montageKernel, 0, sizeof(cl_mem), &inBuffer);
	checkErrorCode(err, CL_SUCCESS, "clSetKernelArg()");

	err = clSetKernelArg(montageKernel, 1, sizeof(cl_mem), &outBuffer);
	checkErrorCode(err, CL_SUCCESS, "clSetKernelArg()");

	err = clSetKernelArg(montageKernel, 2, sizeof(cl_int), &inputRowLength);
	checkErrorCode(err, CL_SUCCESS, "clSetKernelArg()");

	err = clSetKernelArg(montageKernel, 3, sizeof(cl_int), &inputRowOffset);
	checkErrorCode(err, CL_SUCCESS, "clSetKernelArg()");

	err = clSetKernelArg(montageKernel, 4, sizeof(cl_int), &outputRowLength);
	checkErrorCode(err, CL_SUCCESS, "clSetKernelArg()");

	size_t globalWorkSize = outputRowLength;

	err = clEnqueueNDRangeKernel(queue, montageKernel, 1, nullptr, &globalWorkSize, nullptr, 0, nullptr, nullptr);
	checkErrorCode(err, CL_SUCCESS, "clEnqueueNDRangeKernel()");
}
