#include "crawler.h"

#include <filesystem>
#include <iostream>
#include <vector>

// Recursively collects file paths relative to the root directory.
std::vector<std::filesystem::path> crawler(const std::filesystem::path &root_path)
{
    std::vector<std::filesystem::path> files;

    if (!std::filesystem::exists(root_path))
    {
        std::cerr << "Error: root path does not exist\n";
        return files;
    }

    if (!std::filesystem::is_directory(root_path))
    {
        std::cerr << "Error: root path is not a directory\n";
        return files;
    }

    try
    {
        for (const auto &entry : std::filesystem::recursive_directory_iterator(root_path))
        {
            if (entry.is_regular_file())
            {
                // Preserve relative paths for later document lookup.
                files.push_back(std::filesystem::proximate(entry.path(), root_path));
            }
        }
    }
    catch (const std::filesystem::filesystem_error &e)
    {
        std::cerr << "Caught error: " << e.what() << "\n";
    }

    return files;
}
