#ifndef AISE_IOUTILS_H
#define AISE_IOUTILS_H

#include "node.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/ToolOutputFile.h"
#include <vector>
#include <list>

namespace aise
{

typedef std::vector<Node *> NodeArray;

// ReadBitcode parses bitcode file as DAGs.
// Returns the number of parsed DAGs, -1 if there is any error.
int ParseBitcode(llvm::Twine path, std::list<NodeArray *> &buffer);

int ParseMISO(llvm::Twine path);

// Returns -1 if there is any error.
int ParseInt(std::string &str, int &buffer);

class OutFile
{
    llvm::tool_output_file *out;
    OutFile(llvm::tool_output_file *_out) : out(_out) {}

    void operator=(const OutFile &) LLVM_DELETED_FUNCTION;
    OutFile(const OutFile &) LLVM_DELETED_FUNCTION;

  public:
    static OutFile *Create(const char *path);
    llvm::raw_ostream &OS() { return out->os(); };
    void Close();
};

} // namespace aise

#endif
