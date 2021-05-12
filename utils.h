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

NodeArray *ParseMISO(const std::string &RefRPN);

// ParseConf parses the conf file for LLVM assembly.
// Returns the number of configurations loaded, -1 if there is any error.
int ParseConf(llvm::Twine path, std::list<size_t> &buffer);

// ParseInt parses str as an int and saves it into buffer.
// Returns -1 if there is any error, 0 otherwise.
int ParseInt(const std::string &str, int &buffer);

std::string ToString(int a);

// OutFile provides a writer interface that automatically flushes content
// when deconstructed. It's recommanded to use in a braced context.
class OutFile
{
    llvm::tool_output_file *out;
    void operator=(const OutFile &) LLVM_DELETED_FUNCTION;
    OutFile(const OutFile &) LLVM_DELETED_FUNCTION;

  public:
    OutFile(const char *path);
    bool IsOpen() { return out != NULL; };
    llvm::raw_ostream &OS() { return out->os(); };
    ~OutFile();
};

class Permutation
{
    std::vector<size_t> index;
    std::vector<size_t> status;

  public:
    Permutation(size_t n);

    bool HasNext() const { return !status.empty(); }
    // Next returns a permutation of indexes {0, 1, ..., n - 1}.
    const std::vector<size_t> &Next();
};

} // namespace aise

#endif
