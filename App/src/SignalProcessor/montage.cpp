#include "montage.h"

#include <sstream>
#include <fstream>

using namespace std;

Montage::Montage(const vector<string>& sources, OpenCLContext* context) : sources(sources)
{
	program = new OpenCLProgram(buildSource(sources), context);
}

Montage::~Montage()
{
	delete program;
}

string Montage::test(const string& source, OpenCLContext* context)
{
	// Use the OpenCL compiler to test the source.
	OpenCLProgram prog(buildSource(vector<string> {source}), context);

	if (prog.compilationSuccessful())
	{
		prog.createKernel("montage0");

		return ""; // Empty string means test was successful.
	}
	else
	{
		return "Compilation failed:\n" + prog.getCompilationLog();
	}
}

string Montage::buildSource(const vector<string>& sources)
{
	string src;

	ifstream fs("montageHeader.txt");
	while (fs.peek() != EOF)
	{
		src.push_back(fs.get());
	}

	src += "\n__kernel void montage(__global float4* input, __global float4* output, int inputRowLength, int inputRowOffset, int outputRowLength)\n{\n";

	for (unsigned int i = 0; i < sources.size(); ++i)
	{
		src += montageRow(i, sources[i]);
	}

	src += "\n}\n";

	return src;
}

string Montage::montageRow(unsigned int row, const string& code)
{
	stringstream ss;

	ss << "{" << endl;
	ss << "float4 out = 0;" << endl;
	ss << code << endl;
	ss << "output[outputRowLength*" << row << " + get_global_id(0)] = out;" << endl;
	ss << "}" << endl;

	return ss.str();
}

