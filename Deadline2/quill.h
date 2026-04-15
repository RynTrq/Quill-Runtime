#ifndef QUILL_H
#define QUILL_H

#include <functional>

namespace quill {

void init_runtime();
void finalize_runtime();

void start_finish();
void end_finish();

void async(std::function<void()> &&lambda);

} // namespace quill

#endif
