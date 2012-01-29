
#ifndef STD_FILENO_H
#define STD_FILENO_H

#include <iosfwd>

// Fileno template
template <typename charT, typename traits>
int fileno(const std::basic_ios<charT, traits>& stream);

#endif // STD_FILENO_H
