// builds all boost.asio source as a separate compilation unit
#ifndef BOOST_ASIO_SOURCE
#define BOOST_ASIO_SOURCE
#endif

#include <boost/asio/impl/src.hpp>

// for openssl.
#if BOOST_VERSION >= 104610
#include <boost/asio/ssl/impl/src.hpp>
#endif
