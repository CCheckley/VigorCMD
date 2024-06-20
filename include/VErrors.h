#pragma once

#include <iostream>
#include <format>
#include <stdexcept>
#include <cstdarg>
#include <utility>

namespace Vigor
{
	namespace Errors
	{
		template <class ... Args>
		static void RaiseRuntimeError(const std::string& FormatStr, Args&&... args)
		{
			std::string errorMsg = std::vformat(FormatStr, std::make_format_args(std::forward<Args>(args)...));
			throw std::runtime_error(errorMsg);
		}
	}
}