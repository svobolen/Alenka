#include "signalprocessor.h"

#include <algorithm>
#include <stdexcept>
#include <fstream>

using namespace std;

SignalProcessor::SignalProcessor()
{
	cl_int err;

	context = new OpenCLContext(OPENCL_CONTEXT_CONSTRUCTOR_PARAMETERS, QOpenGLContext::currentContext());

	commandQueue = clCreateCommandQueue(context->getCLContext(), context->getCLDevice(), 0, &err);
	checkErrorCode(err, CL_SUCCESS, "clCreateCommandQueue()");
}

SignalProcessor::~SignalProcessor()
{
	cl_int err;

	delete context;

	err = clReleaseCommandQueue(commandQueue);
	checkErrorCode(err, CL_SUCCESS, "clReleaseCommandQueue()");

	destroy();

	gl();
}

void SignalProcessor::changeFilter(Filter* filter)
{
	if (noFile)
	{
		return;
	}

	using namespace std;

	if (PROGRAM_OPTIONS.isSet("printFilter"))
	{
		if (PROGRAM_OPTIONS.isSet("printFilterFile"))
		{
			FILE* file = fopen(PROGRAM_OPTIONS["printFilterFile"].as<string>().c_str(), "w");
			checkNotErrorCode(file, nullptr, "File '" << PROGRAM_OPTIONS["printFilterFile"].as<string>() << "' could not be opened for wtiting.");

			filter->printCoefficients(file);

			fclose(file);
		}
		else
		{
			filter->printCoefficients(stderr);
		}
	}

	filterProcessor->change(filter);

	if (onlineFilter == false)
	{
		cache->clear();
	}
}

void SignalProcessor::changeMontage(Montage* montage)
{
	if (noFile)
	{
		return;
	}

	cl_int err;

	montageProcessor->change(montage);

	deleteOutputBuffer();

	unsigned int outputBlockSize = blockSize*montage->getNumberOfRows();

	GLuint buffer;

	gl()->glGenBuffers(1, &buffer);
	gl()->glGenVertexArrays(1, &processorVertexArray);

	gl()->glBindVertexArray(processorVertexArray);
	gl()->glBindBuffer(GL_ARRAY_BUFFER, buffer);
	gl()->glVertexAttribPointer(0, 1, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<void*>(0));
	gl()->glEnableVertexAttribArray(0);
	gl()->glBufferData(GL_ARRAY_BUFFER, outputBlockSize*sizeof(float), nullptr, GL_STATIC_DRAW);

	cl_mem_flags flags = CL_MEM_READ_WRITE;
#ifdef NDEBUG
	flags = CL_MEM_WRITE_ONLY;
#if CL_VERSION_1_2
	flags |= CL_MEM_HOST_NO_ACCESS;
#endif
#endif

	processorOutputBuffer = clCreateFromGLBuffer(context->getCLContext(), flags, buffer, &err);
	checkErrorCode(err, CL_SUCCESS, "clCreateFromGLBuffer()");

	gl()->glBindBuffer(GL_ARRAY_BUFFER, 0);
	gl()->glBindVertexArray(0);
	gl()->glDeleteBuffers(1, &buffer);
}

SignalBlock SignalProcessor::getAnyBlock(const std::set<int>& indexSet)
{
	assert(noFile == false);
	assert(indexSet.empty() == false);

	cl_int err;

	cl_event readyEvent = clCreateUserEvent(context->getCLContext(), &err);
	checkErrorCode(err, CL_SUCCESS, "clCreateUserEvent()");

#if CL_VERSION_1_2
	err = clEnqueueBarrierWithWaitList(commandQueue, 1, &readyEvent, nullptr);
	checkErrorCode(err, CL_SUCCESS, "clEnqueueBarrierWithWaitList()");
#else
	err = clEnqueueWaitForEvents(commandQueue, 1, &readyEvent);
	checkErrorCode(err, CL_SUCCESS, "clEnqueueWaitForEvents()");
#endif

	err = clReleaseEvent(readyEvent);
	checkErrorCode(err, CL_SUCCESS, "clReleaseEvent()");

	int index = cache->getAny(indexSet, processorTmpBuffer, readyEvent);

	printBuffer("after_getAny.txt", processorTmpBuffer, commandQueue);

	if (onlineFilter)
	{
		filterProcessor->process(processorTmpBuffer, commandQueue);

		printBuffer("after_filter.txt", processorTmpBuffer, commandQueue);

		err = clFlush(commandQueue);
		checkErrorCode(err, CL_SUCCESS, "clFlush()");
	}

	gl()->glFinish(); // Could be replaced by a fence.

	err = clEnqueueAcquireGLObjects(commandQueue, 1, &processorOutputBuffer, 0, nullptr, nullptr);
	checkErrorCode(err, CL_SUCCESS, "clEnqueueAcquireGLObjects()");

	montageProcessor->process(processorTmpBuffer, processorOutputBuffer, commandQueue);

	printBuffer("after_montage.txt", processorOutputBuffer, commandQueue);

	err = clFinish(commandQueue);
	checkErrorCode(err, CL_SUCCESS, "clFinish()");

	err = clEnqueueReleaseGLObjects(commandQueue, 1, &processorOutputBuffer, 0, nullptr, nullptr);
	checkErrorCode(err, CL_SUCCESS, "clEnqueueReleaseGLObjects()");

	err = clFinish(commandQueue);
	checkErrorCode(err, CL_SUCCESS, "clFinish()");

	auto fromTo = DataFile::getBlockBoundaries(index, getBlockSize());
	return SignalBlock(index, montageProcessor->getNumberOfRows(), fromTo.first, fromTo.second, processorVertexArray);
}

void SignalProcessor::changeFile(DataFile* file)
{
	destroy();

	if (file == nullptr)
	{
		noFile = true;
	}
	else
	{
		noFile = false;

		cl_int err;

		int M = file->getSamplingFrequency();
		int offset = M;
		int delay = M/2 - 1;
		blockSize = PROGRAM_OPTIONS["blockSize"].as<unsigned int>() - offset;
		unsigned int tmpBlockSize = (blockSize + offset + 4)*file->getChannelCount();

		// Check block sizes.
		if (M%4 || (blockSize + offset)%4)
		{
			throw runtime_error("SignalProcessor requires both the filter length and block length to be multiples of 4");
		}

		// Create filter and montage processors.
		filterProcessor = new FilterProcessor(M, blockSize + offset, file->getChannelCount(), context);

		montageProcessor = new MontageProcessor(offset, blockSize);

		// Construct the cache.
		onlineFilter = PROGRAM_OPTIONS["onlineFilter"].as<bool>();

		int64_t memory = PROGRAM_OPTIONS["gpuMemorySize"].as<int64_t>();

		if (memory <= 0)
		{
			cl_ulong size;

			err = clGetDeviceInfo(context->getCLDevice(), CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(cl_ulong), &size, nullptr);
			checkErrorCode(err, CL_SUCCESS, "clGetDeviceInfo()");

			memory += size;
		}

		memory -= tmpBlockSize + blockSize*sizeof(float)*200; // substract the sizes of the tmp buffer and the output buffer (for a realistically big montage)

		cache = new GPUCache(blockSize, offset, delay, memory, file, context, onlineFilter ? nullptr : filterProcessor);

		// Construct tmp buffer.
		cl_mem_flags flags = CL_MEM_READ_WRITE;
#ifdef NDEBUG
#if CL_VERSION_1_2
		flags |= CL_MEM_HOST_NO_ACCESS;
#endif
#endif

		processorTmpBuffer = clCreateBuffer(context->getCLContext(), flags, tmpBlockSize*sizeof(float), nullptr, &err);
		checkErrorCode(err, CL_SUCCESS, "clCreateBuffer()");

		// Default filter and montage.
		double Fs = file->getSamplingFrequency();
		Filter* filter = new Filter(M, Fs);
		filter->setHighpass(PROGRAM_OPTIONS["highpass"].as<double>());
		filter->setLowpass(PROGRAM_OPTIONS["lowpass"].as<double>());
		filter->setNotch(PROGRAM_OPTIONS["notch"].as<bool>());

		vector<string> rows;
		if (PROGRAM_OPTIONS.isSet("montageFile"))
		{
			ifstream mf(PROGRAM_OPTIONS["montageFile"].as<string>());

			while (mf.peek() != EOF)
			{
				string s;
				getline(mf, s);
				rows.push_back(s);
			}
		}
		else
		{
			for (unsigned int i = 0; i < file->getChannelCount(); ++i)
			{
				stringstream ss;
				ss << "out = in(" << i << ");";
				rows.push_back(ss.str());
			}
		}
		Montage* montage = new Montage(rows, context);

		changeFilter(filter);
		delete filter;

		changeMontage(montage);
		delete montage;
	}
}

void SignalProcessor::destroy()
{
	if (noFile == false)
	{
		cl_int err;

		delete filterProcessor;
		delete montageProcessor;
		delete cache;

		err = clReleaseMemObject(processorTmpBuffer);
		checkErrorCode(err, CL_SUCCESS, "clReleaseMemObject()");

		deleteOutputBuffer();
	}
}
