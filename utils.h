#ifndef AISE_UTILS_H
#define AISE_UTILS_H

#include "node.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/ToolOutputFile.h"

namespace aise
{

// ReadBitcode parses bitcode file as DAGs.
// Returns the number of parsed DAGs, -1 if there is any error.
int ParseBitcode(llvm::Twine path, std::list<NodeArray *> &buffer);

// ParseMISO parses the miso file with each instruction as a DAG.
// Returns the number of instructions loaded, -1 if there is any error.
int ParseMISO(llvm::Twine path, std::list<NodeArray *> &buffer);

// ParseInt parses str as an int and saves it into buffer.
// Returns -1 if there is any error, 0 otherwise.
int ParseInt(const std::string &str, int &buffer);

std::string ToString(int a);

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

class Permutation
{
    std::vector<size_t> index;
    std::vector<size_t> status;

  public:
    Permutation(size_t n);

    bool HasNext() const { return !status.empty(); }
    const std::vector<size_t> &Next();
};

} // namespace aise

#endif
