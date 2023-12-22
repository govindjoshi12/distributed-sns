#include <string>
#include <vector>

std::vector<std::string> splitString(const std::string &str, std::string delim);
std::vector<std::string> splitFilepath(const std::string &filepath);
std::string getParentFolderPath(const std::string &filepath);
std::string getRelativeFilename(const std::string &filepath);
bool isNameValid(const std::string &filename);