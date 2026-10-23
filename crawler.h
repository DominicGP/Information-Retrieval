#pragma once

#include <filesystem>
#include <vector>

std::vector<std::filesystem::path>
crawler(const std::filesystem::path &root_path);