#pragma once
// Upstream only uses BOOST_BINARY for literals. C++14+ has native binary literals.
#define BOOST_BINARY_JOIN(x) 0b##x
#define BOOST_BINARY(x) BOOST_BINARY_JOIN(x)
