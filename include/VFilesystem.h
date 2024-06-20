#pragma once

#include <vector>
#include <fstream>

namespace Vigor
{
    namespace Filesystem
    {
        static std::vector<char> Read(const std::string& filename)
        {
            std::ifstream file(filename, std::ios::ate | std::ios::binary);

            if (!file.is_open())
            {
                throw std::runtime_error(("Failed to open file! File Path: %s\n\n", filename));
            }

            // get size
            size_t fileSize = (size_t)file.tellg();
            std::vector<char> buffer(fileSize);

            // seek to file start and read all bytes
            file.seekg(0);
            file.read(buffer.data(), fileSize);

            // close and return data
            file.close();
            return buffer;
        }
    }
}