#pragma once

#include <string_view>
#include <vector>

namespace su {

	template <typename T>
	std::vector<std::basic_string_view<T>> split(
		std::basic_string_view<T> const str,
		char const sep,
		bool const multisep = false,     // allow multiple sep
		bool const no_empty = false,     // don't add any empty element
		bool const trim_end = true)      // don't add last empty element
	{
		std::vector<std::basic_string_view<T>> strings;
		if (!str.size()) return strings;
		std::size_t beg = 0, pos = 0;

		while (pos < str.size()) {
			if (str[pos] != sep) {
				++pos;
			} else {
				if (!no_empty || beg < pos)
					strings.emplace_back(std::basic_string_view<T>(str.data() + beg, pos - beg));
				if (beg = ++pos; multisep && beg < pos) for (; str[pos] == sep; beg = ++pos);
			}
		}

		if ((!no_empty || beg < pos) && (!trim_end || pos != beg))
			strings.emplace_back(std::basic_string_view<T>(str.data() + beg, pos - beg));
		return strings;
	}

}