#ifndef RUST_DEMO_H
#define RUST_DEMO_H

#ifdef __cplusplus
extern "C" {
#endif

int benchmark_init(int parameters_num, void **parameters);

void benchmark_execution(int parameters_num, void **parameters);

void benchmark_teardown(int parameters_num, void **parameters);

#ifdef __cplusplus
}
#endif

#endif