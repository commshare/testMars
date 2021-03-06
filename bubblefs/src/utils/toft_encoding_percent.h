// Copyright 2011, The Toft Authors.
// All rights reserved.
//
// Author: CHEN Feng <chen3feng@gmail.com>

// toft/encoding/percent.h

#ifndef BUBBLEFS_UTILS_TOFT_ENCODING_PERCENT_H_
#define BUBBLEFS_UTILS_TOFT_ENCODING_PERCENT_H_

#include <ctype.h>
#include <stdint.h>
#include <string>
#include "utils/toft_base_string_string_piece.h"

namespace bubblefs {
namespace mytoft {

/// @brief percent encoding, majorly for url
/// @see http://en.wikipedia.org/wiki/Percent-encoding
struct PercentEncoding {
public:
    // Same as escape in javascript

    static void EncodeAppend(const StringPiece& input, std::string* output);

    static void EncodeTo(const StringPiece& input, std::string* output);

    static void Encode(std::string *str);

    static std::string Encode(const StringPiece& input);

    // Same as encodeURI in javascript

    static void EncodeUriAppend(const StringPiece& input, std::string* output);

    static void EncodeUriTo(const StringPiece& input, std::string* output);

    static void EncodeUri(std::string *str);

    static std::string EncodeUri(const StringPiece& input);

    // Same as encodeURIComponent in javascript

    static void EncodeUriComponentAppend(const StringPiece& input, std::string* output);

    static void EncodeUriComponentTo(const StringPiece& input, std::string* output);

    static void EncodeUriComponent(std::string *str);

    static std::string EncodeUriComponent(const StringPiece& input);


    static bool DecodeAppend(const StringPiece& input, std::string* output);

    static bool DecodeTo(const StringPiece& input, std::string* output);

    static bool Decode(std::string* str);
};

} // namespace mytoft
} // namespace bubblefs

#endif // BUBBLEFS_UTILS_TOFT_ENCODING_PERCENT_H_