/*
 * Copyright (C) 2021 magicxqq <xqq@xqq.im>. All rights reserved.
 *
 * This file is part of libaribcaption.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef ARIBCAPTION_CONTEXT_HPP
#define ARIBCAPTION_CONTEXT_HPP

#include <memory>
#include <functional>
#include "aribcc_export.h"

namespace aribcaption {

enum class LogLevel {
    kError,
    kWarning,
    kVerbose
};

/**
 * Logcat callback function prototype
 *
 * See @Context::SetLogcatCallback()
 */
using LogcatCB = std::function<void(LogLevel level, const char* message)>;

class Logger;

/**
 * Construct a context before using any other aribcc APIs.
 *
 * Context must be freed after all the objects constructed from the context have been freed.
 */
class Context {
public:
    ARIBCC_API Context();
    ARIBCC_API ~Context();
    ARIBCC_API Context(Context&&) noexcept;
    ARIBCC_API Context& operator=(Context&&) noexcept;
public:
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;
};

}  // namespace aribcaption

#endif  // ARIBCAPTION_CONTEXT_HPP
